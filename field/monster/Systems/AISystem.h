#pragma once
#include "../MonsterEnvironment.h"
#include "../MonsterWorld.h"
#include "../Components.h"
namespace monster_ecs {

    class AISystem {
    public:
        void update(float dt, class MonsterWorld& ecs, MonsterEnvironment& env);
    };

} // namespace monster_ecs
