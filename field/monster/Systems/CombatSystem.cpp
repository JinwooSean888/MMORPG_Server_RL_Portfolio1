#include "CombatSystem.h"
#include "../MonsterWorld.h"
#include "../Components.h"

namespace monster_ecs {

    void CombatSystem::update(float dt, MonsterWorld& ecs, MonsterEnvironment& env)
    {
        for (Entity e : ecs.monsters) {
            auto& ai = ecs.aiComp.get(e);
            
            if (ai.state != CAI::State::Attack) {
                // 공격 상태가 아니면 타이머 초기화
                ai.attackTimer = 0.0f;
                continue;
            }

            // 공격 쿨타임 누적
            ai.attackTimer += dt;
            if (ai.attackTimer < ai.attackCooldown) {
                // 아직 쿨 안 찼으면 패스
                continue;
            }

            float px, py;
            if (!env.getPlayerPosition(ai.targetId, px, py))
                continue;

            auto& st = ecs.stats.get(e);
            int damage = st.atk;
			

            // HP 깎는 건 env.broadcastCombat 안에서 처리
			attack_player(e, ai.targetId, ecs, env);
            ai.attackCd = 0.9f;


            ai.attackTimer = 0.0f;

        }
    }

    void CombatSystem::attack_player(uint64_t monsterId, uint64_t playerId, MonsterWorld& monsterWorld_, MonsterEnvironment& env_)
    {
        int hp = 0, maxHp = 0;
        int sp = 0, maxSp = 0;

        if (!env_.getPlayerStats(playerId, hp, maxHp, sp, maxSp))
            return; // invalid

        // === 데미지 계산 ===
        auto& mon = monsterWorld_.stats.get(monsterId);
        const int dmg = mon.atk;

        const int newHp = std::max(0, hp - dmg);
        const int newSp = sp;

        const bool hpChanged = (newHp != hp);
        const bool spChanged = (newSp != sp);
        const bool statChanged = (hpChanged || spChanged);

        // 변경 있을 때만 실제 반영
        if (statChanged) {
            env_.setPlayerStats(playerId, newHp, newSp);

            // 변경 있을 때만 dirty (안전하게 nullptr 체크)
            if (env_.markPlayerDirty) env_.markPlayerDirty(playerId);
        }

        // 전투 브로드캐스트는 “공격 발생” 자체가 의미라면 유지

        env_.broadcastCombat(monsterId, playerId, dmg, newHp);

        // 스탯 패킷도 변경 있을 때만 보내는 게 보통 더 맞음
        if (statChanged) {
            env_.broadcastPlayerStat(playerId, newHp, maxHp, newSp, maxSp);
        }

        if (hpChanged && newHp <= 0) {
            env_.broadcastPlayerState(playerId, monster_ecs::PlayerState::Dead)
        }
    }


} // namespace monster_ecs
