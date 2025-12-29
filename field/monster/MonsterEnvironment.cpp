#include <random>
#include <cmath>
#include "MonsterEnvironment.h"
#include "MonsterWorld.h"

namespace monster_ecs {

    MonsterEnvironment::MonsterEnvironment(MonsterWorld& world): world_(world)
    {

    }

    // 랜덤 방향 하나 뽑기 (단위 벡터)
    void MonsterEnvironment::pick_random_walk_dir(float /*x*/, float /*y*/,
        float& outDx, float& outDy)
    {
        // 0 ~ 2π 랜덤 각도
        std::uniform_real_distribution<float> dist(0.f, 2.f * 3.1415926f);
        float angle = dist(rng_);

        outDx = std::cos(angle);
        outDy = std::sin(angle);
    }

    // 몬스터 이동 설정: AI 컴포넌트에 방향/속도 기록
    void MonsterEnvironment::set_monster_move(Entity e,
        float dx, float dy,
        float speed)
    {
        auto& ai = world_.aiComp.get(e);

        float len2 = dx * dx + dy * dy;
        if (len2 < 1e-4f || speed <= 0.f) {
            ai.moveDirX = 0.f;
            ai.moveDirY = 0.f;
            ai.moveSpeed = 0.f;
            return;
        }

        float invLen = 1.0f / std::sqrt(len2);
        ai.moveDirX = dx * invLen;
        ai.moveDirY = dy * invLen;
        ai.moveSpeed = speed;
    }

} // namespace monster_ecs
