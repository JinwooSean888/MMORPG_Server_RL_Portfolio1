#include "GameServer.h"
#include "SpawnSystem.h"

namespace monster_ecs {

    void SpawnSystem::update(float dt, MonsterWorld& ecs, MonsterEnvironment& env)
    {
        for (Entity e : ecs.monsters)
        {
            auto& st = ecs.stats.get(e);
            auto& sp = ecs.spawnInfo.get(e);
            auto& ai = ecs.aiComp.get(e);
            auto& tr = ecs.transform.get(e);

            // 살아 있으면 패스
            if (st.hp > 0)
                continue;

            // 리스폰 대기 중이 아니면 패스
            if (!sp.pendingRespawn)
                continue;

            if (sp.respawnDelay <= 0.0f)
                continue;

            sp.respawnTimer += dt;
            if (sp.respawnTimer < sp.respawnDelay)
                continue;

            // ===== 리스폰 =====
            sp.pendingRespawn = false;
            sp.respawnTimer = 0.0f;

            // 스탯 / 위치
            st.hp = st.maxHp;
            tr.x = sp.spawnX;
            tr.y = sp.spawnY;

            // AI 초기화
            ai.state = CAI::State::Idle;
            ai.targetId = 0;
            ai.thinkCooldown = 0.0f;
            ai.attackTimer = 0.0f;

            // AOI 재등장 (Enter)
            if (env.spawnInAoi)
                env.spawnInAoi(e, tr.x, tr.y);
        }
    }


} // namespace monster_ecs
