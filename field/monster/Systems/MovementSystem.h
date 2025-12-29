#pragma once
#include "../MonsterEnvironment.h"

namespace monster_ecs {


    class MovementSystem {
    public:
        void update(float dt, class MonsterWorld& ecs, MonsterEnvironment& env);
    };

} // namespace monster_ecs
