
#include "game_system_logic.h"

#include "core/proto/protocol_verify.h"
#include "net/session.h"

namespace logic {

    void OnRecv_Ping(net::Session* session, const game::Envelope& env)
    {
        if (!session) return;

        auto [err, req] = VERIFY_FBS_REQ(Ping, env);
        if (err != proto::Error::Ok) {

            return;
        }


        auto clientTime = req->client_time_ms();
        (void)clientTime;

    }

} // namespace logic
