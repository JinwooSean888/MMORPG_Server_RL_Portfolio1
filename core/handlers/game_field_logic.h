#pragma once

#include "proto/generated/game_generated.h"

namespace net { class Session; }

namespace logic {

    void OnRecv_EnterField(net::Session* session, const game::Envelope& env);
    void OnRecv_SkillCmd(net::Session* session, const game::Envelope& env);

}
