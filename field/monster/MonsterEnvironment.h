#pragma once
#include <functional>
#include <cstdint>
#include <random>

#include "monster/Components.h"
#include "proto/generated/field_generated.h"
//#include "MonsterWorld.h"

namespace monster_ecs {


    class MonsterWorld;
    class MonsterEnvironment {

    public:
		//콜백 함수들
        std::function<std::uint64_t(float, float, float)> findClosestPlayer;
        std::function<bool(std::uint64_t, float&, float&)> getPlayerPosition;
        std::function<void(std::uint64_t, std::uint64_t, int, int)> broadcastCombat;
        std::function<void(std::uint64_t, float, float)> moveInAoi;
        std::function<void(uint64_t, monster_ecs::CAI::State)>  broadcastAiState;
        std::function<void(uint64_t, PlayerState st)>  broadcastPlayerState;

        std::function<void(uint64_t mid, float x, float y)> spawnInAoi;
        std::function<void(uint64_t mid)>                  removeFromAoi;

        std::function<void(uint64_t monsterId, int hp, int maxHp, int sp, int maxSp)> broadcastMonsterStat;
        std::function<void(uint64_t playerId, int hp, int maxHp, int sp, int maxSp)> broadcastPlayerStat;
        
		//필드워커에게 플레이어 스탯을 얻어오는 콜백
        std::function<bool(uint64_t playerId, int& hp, int& maxHp, int& sp, int& maxSp)> getPlayerStats;
		//필드워커에게 플레이어 스탯을 설정하는 콜백
        std::function<void(uint64_t playerId, int newHp, int newSp)> setPlayerStats;
		//필드워커에게 플레이어 데미지를 입히는 콜백
        std::function<void(uint64_t playerId, int dmg)> applyPlayerDamage;

        std::function<void(uint64_t playerId)> markPlayerDirty;
        MonsterEnvironment(MonsterWorld& world);

        void pick_random_walk_dir(float x, float y,
            float& outDx, float& outDy);

        void set_monster_move(Entity e,
            float dx, float dy,
            float speed);

    private:
        MonsterWorld& world_;
        std::mt19937 rng_{ std::random_device{}() };
    };
    static inline void stop_move(Entity e, auto& ai, MonsterEnvironment& env)
    {
        ai.moveDirX = 0.f;
        ai.moveDirY = 0.f;
        ai.moveSpeed = 0.f;
        env.set_monster_move(e, 0.f, 0.f, 0.f);
    }
} // namespace monster_ecs
