#include "game_handler_registry.h"

#include <vector>
#include <functional>

#include "proto/generated/game_generated.h"   // game::Envelope, game::MsgType
#include "net/session.h"

#include "game_system_logic.h"
#include "game_auth_logic.h"
#include "game_field_logic.h"

namespace {

    using HandlerFunc = std::function<void(const game::Envelope&, void*)>;

    struct HandlerEntry {
        game::MsgType type;
        HandlerFunc   func;
    };

    inline net::Session* ToSession(void* ctx) {
        return static_cast<net::Session*>(ctx);
    }

    const std::vector<HandlerEntry>& GetHandlerRegistry()
    {
        static std::vector<HandlerEntry> list = {

            // ----- 시스템 관련 -----
            {
                game::MsgType_Ping,
                [](const game::Envelope& env, void* ctx) {
                    logic::OnRecv_Ping(ToSession(ctx), env);
                }
            },

            // ----- 인증 / 로그인 -----
            {
                game::MsgType_Login,  
                [](const game::Envelope& env, void* ctx) {
                    logic::OnRecv_Login(ToSession(ctx), env);
                }
            },

            // ----- 필드 관련 -----
            {
                game::MsgType_EnterField,  
                [](const game::Envelope& env, void* ctx) {
                    logic::OnRecv_EnterField(ToSession(ctx), env);
                }
            },
            {
                game::MsgType_SkillCmd,
                [](const game::Envelope& env, void* ctx) {
                    logic::OnRecv_SkillCmd(ToSession(ctx), env);
                }
            },


        };

        return list;
    }

} // anonymous namespace


namespace handlers {

    void RegisterAllGameHandlers(core::Dispatcher& disp)
    {
        const auto& table = GetHandlerRegistry();

        for (const auto& entry : table) {
            auto key = static_cast<uint8_t>(entry.type);
            std::cout << "[Reg] type=" << (int)entry.type
                << " key=" << (int)key << std::endl;
            disp.register_handler(entry.type, entry.func);
        }
    }

} // namespace handlers
