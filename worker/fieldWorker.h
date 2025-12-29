// FieldWorker.h
#pragma once
#include <unordered_map>
#include <cstdint>
#include "worker/worker.h"
#include "game/player.h"
#include "proto/generated/field_generated.h"
#include "net/session.h"
#include "monster/MonsterWorld.h"
#include "monster/Components.h"
#include "field/monster/MonsterEnvironment.h"
#include "storage/redis/redisUserCache.h"

namespace storage { class DirtyHub; }
namespace core {

    class FieldAoiSystem;

    struct MonsterTemplate
    {
        std::string name;
        int maxHp;
        int hp;
        int maxSp;
        int sp;
        int atk;
        int def;
    };


    static const std::vector<MonsterTemplate> kMonsterTemplates = {
        {"BowAndArrow1",   200, 200, 100, 100, 7, 3},
        {"BowAndArrow2",   200, 200, 100, 100, 7, 3},
        {"BowAndArrow3",   200, 200, 100, 100, 7, 3},
        {"DoubleSwords1",  200, 200, 100, 100, 7, 3},
        {"DoubleSwords2",  200, 200, 100, 100, 7, 3},
        {"DoubleSwords3",  200, 200, 100, 100, 7, 3},
        {"MagicWand1",     200, 200, 100, 100, 7, 3},
        {"MagicWand2",     200, 200, 100, 100, 7, 3},
        {"MagicWand3",     200, 200, 100, 100, 7, 3},
    };


    class FieldWorker : public Worker {
    public:
        using Ptr = std::shared_ptr<FieldWorker>;
        using RedisRtWriter = std::function<void(const storage::redis::UserSnapshot&)>;
        
        void set_redis_rt_writer(RedisRtWriter fn) { redisRtWriter_ = std::move(fn); }
        explicit FieldWorker(int fieldId, storage::DirtyHub& hub);
        ~FieldWorker();

        void handle_message(const NetMessage& msg);
        static inline float clampf(float v, float lo, float hi) {
            return std::max(lo, std::min(v, hi));
        }
        // 서버 틱에서 호출 (dt: 초 단위)
        void update_world(float dt);
        void tick_players(float step);
        void tick_monsters(float step);
        // 플레이어 등록/제거 (필드 입장/퇴장 시 사용)
        void add_player(Player::Ptr player);
        void remove_player(std::uint64_t playerId);
        void init_monster_env(); 
        int field_id() const { return fieldId_; }
        void on_client_move_input(const field::FieldCmd& cmd, net::Session::Ptr session);
        std::string get_prefab_name(uint64_t entityId, bool isMonster);
        void send_field_enter(std::uint64_t watcherId, std::uint64_t subjectId, bool isMonster, const Vec2& pos);
        void on_player_enter_field(Player::Ptr player);
        void mark_dirty_state(Player& player);
        void mark_dirty_pos_if_needed(Player& player,const Vec2& oldPos,const Vec2& newPos);
    private:
        bool is_walkable(const Vec2& from, const Vec2& to) const;
        void SpawnMonstersEvenGrid(int fieldId);

        void broadcast_ai_state(uint64_t entityId, field::EntityType et, field::AiStateType fbState);
        void broadcast_monster_ai_state(uint64_t monsterId, monster_ecs::CAI::State newState);
        void broadcast_player_ai_state(uint64_t playerId, monster_ecs::PlayerState st);

        void broadcast_stat_event(uint64_t entityId, field::EntityType et, int hp, int maxHp, int sp, int maxSp);
        void broadcast_monster_stat(uint64_t monsterId, int hp, int maxHp, int sp, int maxSp);
        void broadcast_player_stat(uint64_t playerId, int hp, int maxHp, int sp, int maxSp);
        void write_player_rt_enqueue(uint64_t playerId, const Player& p);
    private:
        int fieldId_{ 0 };    
        float worldTime_ = 0.0f;  // 필드 기준 누적 시간(초)
        monster_ecs::MonsterWorld monsterWorld_;
        monster_ecs::MonsterEnvironment env_;
        std::shared_ptr<FieldAoiSystem> aoiSystem_;
        // playerId -> Player
        std::unordered_map<std::uint64_t, Player::Ptr> players_;
        float playerAcc_ = 0.0f;
        float monsterAcc_ = 0.0f;

        static constexpr float PlayerStep = 0.05f;  // 50ms
        static constexpr float MonsterStep = 0.10f;  // 100ms

        storage::DirtyHub& dirtyHub_;//레디스 더티 처리
        float posDirtyDist_ = 0.10f;      // 10cm
        float dirtyMinInterval_ = 0.25f;  // 250ms (선택)
        RedisRtWriter redisRtWriter_;             
        storage::redis::UserSnapshot scratchSnap_{}; 
    private:        
        void send_combat_event(field::EntityType attackerType,uint64_t  attackerId, field::EntityType targetType, uint64_t targetId,int damage,int remainHp);
        void send_stat_event(std::uint64_t watcherId, std::uint64_t subjectId, bool isMonster, int hp, int maxHp, int sp, int maxSp);
        void monster_spawn_in_aoi(std::uint64_t monsterId, float x, float y);
        void monster_remove_from_aoi(std::uint64_t monsterId);
        void handle_skill(const NetMessage& msg);
    };

    // WorkerManager 관련 헬퍼    
    std::shared_ptr<core::FieldWorker> GetFieldWorker(int fieldId);
    bool SendToFieldWorker(int fieldId, NetMessage msg);

    // (옵션) 클라 절대좌표 Move를 필드워커로 보내는 헬퍼
    void send_move_to_fieldworker(std::uint64_t playerId, int fieldId, float x, float y);

} // namespace core