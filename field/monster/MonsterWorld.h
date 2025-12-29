#pragma once
#include <vector>
#include "ComponentStorage.h"
#include "Components.h"
#include "core_types.h"
#include "MonsterEnvironment.h"
#include "proto/generated/field_generated.h" // FieldCmd, Vec2 등
#include "proto/generated/game_generated.h" // FieldCmd, Vec2 등

namespace monster_ecs {

    class MonsterEnvironment;
    class SpawnSystem;
    class AISystem;
    class MovementSystem;
    class CombatSystem;

    using Entity = std::uint64_t;   // databaseid를 그대로 Entity로 사용

    static field::AiStateType to_fb_state(monster_ecs::CAI::State s)
    {
        switch (s) {
        case monster_ecs::CAI::State::Idle:   return field::AiStateType::AiStateType_Idle;
        case monster_ecs::CAI::State::Patrol: return field::AiStateType::AiStateType_Patrol;
        case monster_ecs::CAI::State::Chase:  return field::AiStateType::AiStateType_Chase;
        case monster_ecs::CAI::State::Attack: return field::AiStateType::AiStateType_Attack;
        case monster_ecs::CAI::State::Return: return field::AiStateType::AiStateType_Return;
        case monster_ecs::CAI::State::Dead:   return field::AiStateType::AiStateType_Dead;        
        }
        return field::AiStateType::AiStateType_Idle;
    }


    class MonsterWorld {
    public:
        MonsterWorld();

        Entity create_monster(INT64 databaseid, float x, float y, const std::string& prefab, int monsterType
            , int maxHp, int hp, int maxSp, int sp, int atk, int def);

        void kill_monster(Entity e);
        void update(float dt, MonsterEnvironment& env);
        bool player_attack_monster(uint64_t pid, uint64_t targetid, game::SkillType skillType, MonsterEnvironment& env);

        // ================= Components =================
        ComponentStorage<CTransform>  transform;
        ComponentStorage<CStats>      stats;
        ComponentStorage<CMonsterTag> monsterTag;
        ComponentStorage<CSpawnInfo>  spawnInfo;
        ComponentStorage<CAI>         aiComp;
        ComponentStorage<CPrefabName> prefabNameComp;

        std::vector<Entity> monsters;

    private:
        SpawnSystem* spawnSys_;
        AISystem* aiSys_;
        MovementSystem* moveSys_;
        CombatSystem* combatSys_;
    };

} // namespace monster_ecs
