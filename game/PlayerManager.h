#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include "game/Player.h"

namespace core {

    class PlayerManager {
    public:
        static PlayerManager& instance() {
            static PlayerManager inst;
            return inst;
        }

        // 세션 기준으로 플레이어 찾기
        Player::Ptr get_by_session(net::Session* sess);

        // ID 기준으로 찾기
        Player::Ptr get_by_id(uint64_t id);

        // 로그인 시 Player 생성 + 등록
        Player::Ptr create_player(uint64_t id,
            const std::string& name,
            std::shared_ptr<net::Session> sess);

        // 로그아웃/세션 끊김 시 제거
        void remove_by_session(net::Session* sess);
        void remove_by_id(uint64_t id);

    private:
        PlayerManager() = default;

        std::mutex mtx_;
        std::unordered_map<uint64_t, Player::Ptr> playersById_;
        std::unordered_map<net::Session*, uint64_t> idBySession_;
    };

} // namespace core
