#pragma once

#include <memory>
#include <string>
#include "core/core_types.h"

namespace net {
    class Session;
}

namespace core {

    struct Vec2
    {
        float x{ 0.0f };
        float y{ 0.0f };
    };


    // 플레이어 이동 상태 (서버 권한 이동용)
    struct PlayerMoveState
    {
        Vec2     dir{ 0.0f, 0.0f };  // 마지막 입력 방향(정규화)
        bool     moving{ false };    // 이동 중인지 여부
        float    speed{ PLAYER_SPEED };      // 유닛/초 (필요 시 변경)
        float    lastInputTime{ 0 }; // 서버 시각
    };

    struct PlayerStat
    {
        int hp{ 500 };
        int maxHp{ 500 };
        int sp{ 100 };
        int maxSp{ 100 };
    };



    class Player {
    public:
        using Ptr = std::shared_ptr<Player>;

        Player(uint64_t playerId,
            std::shared_ptr<net::Session> sess,
            std::string name)
            : id_(playerId)
            , session_(std::move(sess))
            , name_(std::move(name))
            , prefabName_("Paladin")
        {
        }

        uint64_t                        id()       const { return id_; }
        const std::string& name()     const { return name_; }
        std::shared_ptr<net::Session>   session()  const { return session_; }

        void set_field_id(int fieldId) { fieldId_ = fieldId; }
        int  field_id() const { return fieldId_; }

        const std::string& prefab_name() const { return prefabName_; }
        void set_prefab_name(const std::string& p) { prefabName_ = p; }

        // --- 위치 관련 ---
        void set_pos(float x, float y)
        {
            pos_.x = x;
            pos_.y = y;
        }

        Vec2 pos() const { return pos_; }
        float x() const { return pos_.x; }
        float y() const { return pos_.y; }

        // --- 이동 상태 관련 ---
        PlayerMoveState& move_state() { return moveState_; }
        const PlayerMoveState& move_state() const { return moveState_; }

        void set_speed(float s) { moveState_.speed = s; }
        void stop_move() { moveState_.moving = false; }

        // --- 스탯/HP ---
        const PlayerStat& stat() const { return stat_; }
        const PlayerStat& player_stat() const { return stat_; }

        int  hp() const { return stat_.hp; }
        int  max_hp() const { return stat_.maxHp; }
        int  sp() const { return stat_.sp; }
        int  max_sp() const { return stat_.maxSp; }

        void set_hp(int hp)
        {
            stat_.hp = hp;
            if (stat_.hp > stat_.maxHp) stat_.hp = stat_.maxHp;
            if (stat_.hp < 0)           stat_.hp = 0;
        }

        void set_max_hp(int maxHp)
        {
            stat_.maxHp = maxHp;
            if (stat_.hp > stat_.maxHp) stat_.hp = stat_.maxHp;
        }
        void set_sp(int sp)
        {
            stat_.sp = sp;
            if (stat_.sp > stat_.maxSp) stat_.sp = stat_.maxSp;
            if (stat_.sp < 0)           stat_.sp = 0;
        }

        void set_max_sp(int maxSp)
        {
            stat_.maxSp = maxSp;
            if (stat_.sp > stat_.maxSp) stat_.sp = stat_.maxSp;
        }
        double lastDirtyMarkTime() const { return last_dirty_mark_time_; }
        void set_lastDirtyMarkTime(double t) { last_dirty_mark_time_ = t; }
    private:
        uint64_t                      id_{ 0 };
        std::shared_ptr<net::Session> session_;
        std::string                   name_;
        std::string                 prefabName_;
        int                           fieldId_{ 0 }; // 0 = 아직 필드 없음
        Vec2                          pos_;          // 필드 내 위치
        PlayerMoveState               moveState_;    // 이동 상태
        PlayerStat                    stat_;
        double last_dirty_mark_time_{ 0.0 };
    };

} // namespace core
