// game_auth_logic.cpp
#include "game_auth_logic.h"

#include "core/proto/protocol_verify.h"
#include "core/ids.h"                // core::next_player_id()
#include "net/session.h"
#include "net/sessionManager.h"
#include "game/PlayerManager.h"
#include "field/FieldManager.h"

#include <flatbuffers/flatbuffers.h>
// Login / LoginAck 가 들어있는 FBS 헤더 (네 프로젝트에 맞게 수정)
// #include "game/Login_generated.h"

namespace logic {


    void OnRecv_Login(net::Session* session, const game::Envelope& env)
    {
        if (!session) return;

        auto& pm = core::PlayerManager::instance();
        auto& fm = core::FieldManager::instance();

        auto [err, req] = VERIFY_FBS_REQ(Login, env);
        if (err != proto::Error::Ok || !req) {
            return;
        }

        std::string userId = req->user_id() ? req->user_id()->str() : "";
        std::string token = req->token() ? req->token()->str() : "";

  
        if (userId.empty() || token.empty()) {   
            return;
        }


        if (session->player_id() != 0) {
        }


        const std::uint64_t playerId = core::next_player_id();
        auto playerSession = session->shared_from_this();
        auto player = pm.create_player(playerId, userId, playerSession);

        if (!player) {
 
            return;
        }


        session->set_player_id(playerId);
        net::SessionManager::instance().bind_player(playerId, playerSession);


        int defaultFieldId = 1000;

        flatbuffers::FlatBufferBuilder fbb;

        auto userIdOffset = fbb.CreateString(userId);


        auto ackOffset = game::CreateLoginAck(
            fbb,
            /*ok*/ true,
            /*player_id*/ playerId,
            /*user_id*/   userIdOffset,
            /*default_field_id*/ defaultFieldId
        );

        auto envOffset = game::CreateEnvelope(
            fbb,
            game::Packet::Packet_LoginAck,  
            ackOffset.Union()     
        );


        fbb.Finish(envOffset);

        session->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
        
    }

} // namespace logic
