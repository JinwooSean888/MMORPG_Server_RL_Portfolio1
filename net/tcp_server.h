// net/tcp_server.h

#pragma once
#include <vector>
#include <memory>
#include <uv.h>

#include "net/session.h"
#include "core/Dispatcher.h"

namespace core {
    class Worker;   
}

namespace net {

    class TcpServer
    {
    public:
        TcpServer(uv_loop_t* loop,
            const char* ip,
            int port,
            core::Dispatcher* disp,
            core::Worker* gameWorker); 

        void start();


    private:
        static void on_new_conn(uv_stream_t* s, int status);

        void on_session_closed(const Session::Ptr& sess);

    private:
        uv_loop_t* loop_;
        const char* ip_;
        int               port_;
        core::Dispatcher* dispatcher_;

        core::Worker* gameWorker_;      

        uv_tcp_t          server_;
        std::vector<Session::Ptr> sessions_;
    };

} // namespace net
