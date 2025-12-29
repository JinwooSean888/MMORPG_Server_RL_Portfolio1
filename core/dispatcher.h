#pragma once
#include <unordered_map>
#include <functional>
#include <cstdint>
#include "Generated/game_generated.h"

namespace core {

    using HandlerFn = std::function<void(const game::Envelope&, void* session)>;

    class Dispatcher {
    public:
        void register_handler(game::MsgType type, HandlerFn fn);
        void dispatch(const game::Envelope& env, void* session) const;

    private:
        std::unordered_map<uint8_t, HandlerFn> handlers_;
    };

} // namespace core
