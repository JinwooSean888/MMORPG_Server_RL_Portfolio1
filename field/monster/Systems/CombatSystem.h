#pragma once
#include "../MonsterEnvironment.h"

namespace monster_ecs {

    class CombatSystem {
    public:
        void update(float dt,  MonsterWorld& ecs, MonsterEnvironment& env);
        void attack_player(uint64_t monsterId, uint64_t playerId, MonsterWorld& monsterWorld_, MonsterEnvironment& env);
    };

} // namespace monster_ecs
