#pragma once
#include "../MonsterWorld.h"
#include "../MonsterEnvironment.h"

namespace monster_ecs {

    class SpawnSystem {
    public:
        void update(float dt, class MonsterWorld& ecs, MonsterEnvironment& env);
    };

} // namespace monster_ecs
