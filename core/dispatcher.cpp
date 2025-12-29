#include "dispatcher.h"

namespace core {

    void Dispatcher::register_handler(game::MsgType type, HandlerFn fn) {
        handlers_[static_cast<uint8_t>(type)] = std::move(fn);
    }

    void Dispatcher::dispatch(const game::Envelope& env, void* session) const {
        if (!env.pkt()) {
            std::cout << "[Disp] env.pkt() == null" << std::endl;
            return;
        }

        auto pktType = env.pkt_type();
        auto key = static_cast<uint8_t>(pktType);

        std::cout << "[Disp] pkt_type=" << (int)pktType
            << " key=" << (int)key << std::endl;

        auto it = handlers_.find(key);
        if (it == handlers_.end()) {
            std::cout << "[Disp] NO handler for key=" << (int)key << std::endl;
            return;
        }

        std::cout << "[Disp] call handler for key=" << (int)key << std::endl;
        it->second(env, session);
    }


} // namespace core
