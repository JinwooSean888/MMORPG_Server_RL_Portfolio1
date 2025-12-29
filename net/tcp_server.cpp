
#include "net/tcp_server.h"
#include "net/uv_utils.h"
#include "net/sessionManager.h"
#include "core/Dispatcher.h"


namespace net {

    TcpServer::TcpServer(uv_loop_t* loop,
        const char* ip,
        int port,
        core::Dispatcher* disp,
        core::Worker* gameWorker) 
        : loop_(loop)
        , ip_(ip)
        , port_(port)
        , dispatcher_(disp)
        , gameWorker_(gameWorker)       
    {
        uv_tcp_init(loop_, &server_);
        server_.data = this;
    }

    void TcpServer::start() {
        sockaddr_in addr{};
        net::uv_check(uv_ip4_addr(ip_, port_, &addr), "uv_ip4_addr");
        net::uv_check(
            uv_tcp_bind(&server_, reinterpret_cast<const sockaddr*>(&addr), 0),
            "uv_tcp_bind");
        net::uv_check(
            uv_listen(reinterpret_cast<uv_stream_t*>(&server_), 128, &TcpServer::on_new_conn),
            "uv_listen");
    }

    void TcpServer::on_new_conn(uv_stream_t* s, int status) {
        auto* self = reinterpret_cast<TcpServer*>(s->data);
        if (status < 0) {

            return;
        }

        auto sess = std::make_shared<Session>(self->loop_, self->dispatcher_);


        if (self->gameWorker_) {
            sess->set_game_worker(self->gameWorker_);
        }


        sess->set_on_close([self](Session::Ptr closed) {
            SessionManager::instance().remove_session(closed->session_id());
            self->on_session_closed(closed);
            });

        if (uv_accept(s, sess->stream()) == 0) {
            sess->start();

            sess->set_player_id(4);

            SessionManager::instance().add_session(sess);
            self->sessions_.push_back(sess);
        }
        else {
     
        }
    }

    void TcpServer::on_session_closed(const Session::Ptr& sess) {
        auto it = std::find(sessions_.begin(), sessions_.end(), sess);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

} // namespace net
