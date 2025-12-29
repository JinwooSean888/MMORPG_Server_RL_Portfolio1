#include "game/PlayerManager.h"
#include "net/session.h"

namespace core {

    Player::Ptr PlayerManager::get_by_session(net::Session* sess)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = idBySession_.find(sess);
        if (it == idBySession_.end())
            return nullptr;

        auto it2 = playersById_.find(it->second);
        if (it2 == playersById_.end())
            return nullptr;

        return it2->second;
    }

    Player::Ptr PlayerManager::get_by_id(uint64_t id)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = playersById_.find(id);
        if (it == playersById_.end())
            return nullptr;
        return it->second;
    }

    Player::Ptr PlayerManager::create_player(uint64_t id,
        const std::string& name,
        std::shared_ptr<net::Session> sess)
    {
        std::lock_guard<std::mutex> lock(mtx_);

        auto p = std::make_shared<Player>(id, sess, name);
        playersById_[id] = p;
        idBySession_[sess.get()] = id;
        return p;
    }

    void PlayerManager::remove_by_session(net::Session* sess)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = idBySession_.find(sess);
        if (it == idBySession_.end())
            return;

        uint64_t id = it->second;
        idBySession_.erase(it);
        playersById_.erase(id);
    }

    void PlayerManager::remove_by_id(uint64_t id)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = playersById_.find(id);
        if (it == playersById_.end())
            return;

        auto sess = it->second->session();
        if (sess) {
            idBySession_.erase(sess.get());
        }
        playersById_.erase(it);
    }

} // namespace core
