// core/handlers/game_system_logic.h
#pragma once

#include "proto/generated/game_generated.h"

namespace net { class Session; }

namespace logic {

    void OnRecv_Ping(net::Session* session, const game::Envelope& env);

}
