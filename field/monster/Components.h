#pragma once
#include "EntityTypes.h"
#include "proto/generated/field_generated.h"

namespace monster_ecs {

    struct CTransform {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct CStats {
        int maxHp = 50;
        int hp = 50;
        int maxSp = 50;
        int sp = 50;        
        int atk = 7;
        int def = 0;
		bool dirty = false;// 스탯 변경 알림 필요 여부
    };

    struct CMonsterTag {
        int monsterType = 0;
    };

    struct CSpawnInfo {
        float spawnX = 0.0f;
        float spawnY = 0.0f;
        float respawnDelay;   // 몇 초 후 리스폰할지
        bool  pendingRespawn; // 리스폰 대기 중인지
        float respawnTimer;   // 누적 시간
        float deadTimer = 0.0f;
    };

    struct CAI {
        enum class State : INT32 {
            Idle = 0,
            Patrol = 1,
            Chase = 2,
            Attack = 3,
            Return = 4,
			Dead = 5,			
        };
        State state = State::Patrol;
        std::uint64_t targetId = 0;
        float thinkCooldown = 0.0f;

        float attackCooldown{ 1.0f };
        float attackTimer{ 0.0f };

        // Idle/Patrol/Return 상태 유지 타이머로 사용 (roamTimer 대체)
        float idlePatrolTimer{ 0.f };

        // 이동 방향/속도 (roamDirX/Y 대체)
        float moveDirX{ 0.f };
        float moveDirY{ 0.f };
        float moveSpeed{ 0.f };
        bool netSynced = false; // 최초 동기화용
        // 몬스터별 RNG 시드 (유지)
        uint32_t rng = 0x12345678u;
        float attackCd = 0.0f;
    };

    struct CPrefabName {
        std::string name;
    };

    enum class PlayerState {
        Idle = 0,
        Patrol,
        Chase,
        Attack,
        Return,
        Dead,        
    };

    static field::AiStateType to_fb_state(PlayerState s)
    {
        switch (s) {
        case PlayerState::Idle:   return field::AiStateType::AiStateType_Idle;
        case PlayerState::Patrol:   return field::AiStateType::AiStateType_Patrol;
        case PlayerState::Chase:   return field::AiStateType::AiStateType_Chase;
        case PlayerState::Attack: return field::AiStateType::AiStateType_Attack;
        case PlayerState::Return: return field::AiStateType::AiStateType_Return;
        case PlayerState::Dead:   return field::AiStateType::AiStateType_Dead;        
        }
        return field::AiStateType::AiStateType_Idle;
    }

} // namespace monster_ecs
