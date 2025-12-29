#pragma once

#include "core/dispatcher.h"

namespace handlers {

    // 모든 게임 패킷 핸들러를 Dispatcher에 등록
    void RegisterAllGameHandlers(core::Dispatcher& disp);

}
