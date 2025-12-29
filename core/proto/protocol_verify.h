#pragma once

#include <tuple>
#include <memory>
#include "proto/generated/game_generated.h"
#include "proto/generated/field_generated.h"
#include "net/session.h"
#include "game/PlayerManager.h"

namespace proto {

    enum class Error {
        Ok = 0,
        InvalidUnionType,
        InvalidReq,
        InvalidSession,
        InvalidPlayer,
    };

} // namespace proto

// ----------------------------------------------------------------------
// req만 필요할 때
//   auto [err, req] = VERIFY_FBS_REQ(Login, env);
// ----------------------------------------------------------------------
#define VERIFY_FBS_REQ(MSG_NAME, ENV)                                               \
    [&](const game::Envelope& _env)                                                 \
        -> std::tuple<proto::Error, const game::MSG_NAME*>                          \
    {                                                                               \
        if (_env.pkt_type() != game::Packet_##MSG_NAME) {                           \
            return { proto::Error::InvalidUnionType, nullptr };                     \
        }                                                                           \
                                                                                    \
        const game::MSG_NAME* _req = _env.pkt_as_##MSG_NAME();                      \
        if (!_req) {                                                                \
            return { proto::Error::InvalidReq, nullptr };                           \
        }                                                                           \
                                                                                    \
        return { proto::Error::Ok, _req };                                          \
    }(ENV)


// ----------------------------------------------------------------------
// session → Player까지 같이 필요할 때
//   auto [err, req, player] = VERIFY_FBS_REQ_BY_USER(EnterField, env, session);
// ----------------------------------------------------------------------
#define VERIFY_FBS_REQ_BY_USER(MSG_NAME, ENV, SESSION)                              \
    [&](const game::Envelope& _env, net::Session* _sess)                            \
        -> std::tuple<proto::Error, const game::MSG_NAME*,                          \
                     std::shared_ptr<core::Player>>                                 \
    {                                                                               \
        if (!_sess) {                                                               \
            return { proto::Error::InvalidSession, nullptr, nullptr };              \
        }                                                                           \
                                                                                    \
        if (_env.pkt_type() != game::Packet_##MSG_NAME) {                           \
            return { proto::Error::InvalidUnionType, nullptr, nullptr };            \
        }                                                                           \
                                                                                    \
        const game::MSG_NAME* _req = _env.pkt_as_##MSG_NAME();                      \
        if (!_req) {                                                                \
            return { proto::Error::InvalidReq, nullptr, nullptr };                  \
        }                                                                           \
                                                                                    \
        auto& pm      = core::PlayerManager::instance();                            \
        auto  _player = pm.get_by_session(_sess);                                   \
        if (!_player) {                                                             \
            return { proto::Error::InvalidPlayer, _req, nullptr };                  \
        }                                                                           \
                                                                                    \
        return { proto::Error::Ok, _req, _player };                                 \
    }((ENV), (SESSION))
