#include "MovementSystem.h"
#include "../MonsterWorld.h"
#include "../Components.h"
#include <cmath>

namespace monster_ecs {

    void MovementSystem::update(float dt, MonsterWorld& ecs, MonsterEnvironment& env)
    {
        for (Entity e : ecs.monsters) {
            auto& ai = ecs.aiComp.get(e);
            auto& tr = ecs.transform.get(e);

            // 정지 상태는 무조건 정지 (Idle에서도 좌표 변하는 문제 여기서 차단)
            if (ai.state == CAI::State::Idle ||
                ai.state == CAI::State::Attack ||
                ai.state == CAI::State::Dead)
            {
                if (ai.moveSpeed != 0.f || ai.moveDirX != 0.f || ai.moveDirY != 0.f)
                    stop_move(e, ai, env);
                continue;
            }

            float dirX = 0.f, dirY = 0.f;
            float speed = ai.moveSpeed;

  
            if (ai.state == CAI::State::Chase) {
                float px, py;
                if (!ai.targetId || !env.getPlayerPosition(ai.targetId, px, py)) {
                    // 타겟 없으면 이동 중지
                    stop_move(e, ai, env);
                    continue;
                }

                float dx = px - tr.x;
                float dy = py - tr.y;
                float len2 = dx * dx + dy * dy;
                if (len2 < 1e-6f) {
                    stop_move(e, ai, env);
                    continue;
                }
                float inv = 1.f / std::sqrt(len2);
                dirX = dx * inv;
                dirY = dy * inv;

                // AI가 speed 세팅 안 했을 수도 있으니 기본값 보정
                if (speed <= 0.f) speed = 6.0f;
            }
            // Patrol/Return은 AI가 만들어둔 방향으로 이동
            else if (ai.state == CAI::State::Patrol || ai.state == CAI::State::Return) {
                dirX = ai.moveDirX;
                dirY = ai.moveDirY;

                float len2 = dirX * dirX + dirY * dirY;
                if (len2 < 1e-6f || speed <= 0.f) {
                    // 방향/속도 없으면 멈춤
                    stop_move(e, ai, env);
                    continue;
                }
            }
            else {
                // 알 수 없는 상태면 안전하게 정지
                stop_move(e, ai, env);
                continue;
            }

            tr.x += dirX * speed * dt;
            tr.y += dirY * speed * dt;

            // AOI 갱신은 이동 시스템에서만
            env.moveInAoi(e, tr.x, tr.y);

            ai.moveDirX = dirX;
            ai.moveDirY = dirY;
            ai.moveSpeed = speed;
            env.set_monster_move(e, dirX, dirY, speed);
        }
    }


} // namespace monster_ecs
