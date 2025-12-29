#pragma once
#include <cstdint>
#include <ids.h>

typedef int32_t  INT32;
typedef uint32_t UINT32;
typedef int64_t  INT64;
typedef uint64_t UINT64;

static inline uint32_t xorshift32(uint32_t& s) {
    s ^= (s << 13);
    s ^= (s >> 17);
    s ^= (s << 5);
    return s;
}
static inline float frand01(uint32_t& s) {
    return (xorshift32(s) & 0xFFFFFF) / float(0x1000000); // [0,1)
}
static inline float frandRange(uint32_t& s, float a, float b) {
    return a + (b - a) * frand01(s);
}

// ===== Speed Policy (Player=5 ±âÁØ) =====
constexpr float PLAYER_SPEED = 5.0f;

constexpr float PATROL_SPEED_MUL = 0.50f; // 2.5
constexpr float CHASE_MELEE_MUL = 1.10f; // 
constexpr float CHASE_ARCHER_MUL = 1.10f; // 
constexpr float FLEE_MELEE_MUL = 0.70f; // 6.0
constexpr float FLEE_ARCHER_MUL = 0.70f; // 6.5

static inline float patrol_speed(bool isArcher) {
    (void)isArcher;
    return PLAYER_SPEED * PATROL_SPEED_MUL;
}
static inline float chase_speed(bool isArcher) {
    return PLAYER_SPEED * (isArcher ? CHASE_ARCHER_MUL : CHASE_MELEE_MUL);
}
static inline float flee_speed(bool isArcher) {
    return PLAYER_SPEED * (isArcher ? FLEE_ARCHER_MUL : FLEE_MELEE_MUL);
}
