#pragma once

#include "proto/generated/game_generated.h"

namespace net { class Session; }

namespace logic {

    // 로그인 패킷 수신 핸들러
    void OnRecv_Login(net::Session* session, const game::Envelope& env);

}
