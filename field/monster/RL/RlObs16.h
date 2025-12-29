#pragma once
#include <cmath>
#include <cstdint>

struct ObsParams {
    float sight = 11.0f;
    float attack_range = 1.3f;
    float keep_attack_range = 1.8f;
    float low_hp_ratio = 0.25f;
};

enum class RLState : int32_t { Idle = 0, Patrol = 1, Chase = 2, Attack = 3, Return = 4, Dead = 5 };

inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Python env FsmMeleeEnvV2._obs() 그대로
inline void MakeObs16(
    float outObs[16],
    bool hasTarget,
    float mx, float my,
    float tx, float ty,
    float m_hp, float m_max_hp,
    float m_attack_cd,              // 서버의 공격 쿨타임(초). 없으면 0 넣어도 됨(품질↓)
    RLState last_action,            // 직전 상태(Idle~Return)
    const ObsParams& p
) {
    for (int i = 0; i < 16; ++i) outObs[i] = 0.0f;

    float dist = 0.0f;
    float dirx = 0.0f, diry = 0.0f;

    if (hasTarget) {
        float vx = tx - mx;
        float vy = ty - my;
        dist = std::sqrt(vx * vx + vy * vy);
        if (dist > 1e-6f) { dirx = vx / dist; diry = vy / dist; }

        outObs[0] = 1.0f;
        outObs[1] = clampf(dist / p.sight, 0.0f, 2.0f);
        outObs[2] = dirx;
        outObs[3] = diry;

        outObs[7] = (dist <= p.attack_range) ? 1.0f : 0.0f;
        outObs[8] = (dist <= p.keep_attack_range) ? 1.0f : 0.0f;

        outObs[6] = (m_attack_cd <= 0.0f && outObs[7] > 0.5f) ? 1.0f : 0.0f;
    }
    else {
        outObs[0] = 0.0f;
    }

    float hpRatio = (m_max_hp > 0.0f) ? (m_hp / m_max_hp) : 0.0f;
    hpRatio = clampf(hpRatio, 0.0f, 1.0f);
    outObs[4] = hpRatio;
    outObs[5] = (hpRatio <= p.low_hp_ratio) ? 1.0f : 0.0f;

    int la = (int)last_action;
    if (0 <= la && la <= 4) outObs[9 + la] = 1.0f; // 9~13 one-hot
}

// logits[5] argmax
inline int ArgMax5(const float* logits5) {
    int best = 0;
    float bestv = logits5[0];
    for (int i = 1; i < 5; i++) { if (logits5[i] > bestv) { bestv = logits5[i]; best = i; } }
    return best;
}
