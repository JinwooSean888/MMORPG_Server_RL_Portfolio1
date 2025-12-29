// net/session_manager.h
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "session.h"

namespace net {

    class SessionManager {
    public:
        static SessionManager& instance() {
            static SessionManager inst;
            return inst;
        }

        // 접속 직후 세션 등록
        void add_session(const Session::Ptr& sess) {
            if (!sess) return;
            std::lock_guard<std::mutex> lock(mutex_);
            auto sid = sess->session_id();
            sessions_by_session_id_[sid] = sess;
        }

        // 세션 제거 (연결 종료 시)
        void remove_session(std::uint64_t sessionId) {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_by_session_id_.erase(sessionId);

            // playerId 매핑도 정리
            for (auto it = sessions_by_player_id_.begin();
                it != sessions_by_player_id_.end(); )
            {
                if (auto sp = it->second.lock()) {
                    if (sp->session_id() == sessionId) {
                        it = sessions_by_player_id_.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
                else {
                    it = sessions_by_player_id_.erase(it);
                }
            }
        }
        void bind_player(std::uint64_t playerId, const Session::Ptr& sess) {
            if (!sess) return;
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_by_player_id_[playerId] = sess;
        }

        void unbind_player(std::uint64_t playerId) {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_by_player_id_.erase(playerId);
        }

        Session::Ptr find_by_session_id(std::uint64_t sessionId) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = sessions_by_session_id_.find(sessionId);
            if (it == sessions_by_session_id_.end())
                return nullptr;
            return it->second.lock();
        }

        Session::Ptr find_by_player_id(std::uint64_t playerId) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = sessions_by_player_id_.find(playerId);
            if (it == sessions_by_player_id_.end())
                return nullptr;

            auto sp = it->second.lock();
            if (!sp) {
                sessions_by_player_id_.erase(it);
            }
            return sp;
        }

    private:
        SessionManager() = default;
        ~SessionManager() = default;

        SessionManager(const SessionManager&) = delete;
        SessionManager& operator=(const SessionManager&) = delete;

        std::mutex mutex_;
        std::unordered_map<std::uint64_t, std::weak_ptr<Session>> sessions_by_session_id_;
        std::unordered_map<std::uint64_t, std::weak_ptr<Session>> sessions_by_player_id_;
    };

} // namespace net
