#pragma once
#include <uv.h>
#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <memory>
#include <functional>

#include "worker/codec.h"
#include "core/dispatcher.h"
#include "core/ids.h"

namespace core {
    class Worker;   // ★ GameWorker 포인터용 전방 선언
}

namespace net {


    enum class SessionState {
        Connected,   // 접속만 된 상태 (로그인 전)
        LoggedIn,    // 로그인 성공 (필드 입장 전)
        InField,     // 필드 안에 들어간 상태
    };

    class Session : public std::enable_shared_from_this<Session> {
    public:
        using Ptr = std::shared_ptr<Session>;
        using OnClose = std::function<void(Ptr)>;
        Session(uv_loop_t* loop, core::Dispatcher* disp);
        ~Session();

        void start();
        uv_stream_t* stream();

        void send_payload(const std::uint8_t* payload, std::uint32_t len);
        // TcpServer에서 등록하는 콜백
        void set_on_close(OnClose cb) { on_close_ = std::move(cb); }


        void set_game_worker(core::Worker* w) { gameWorker_ = w; }
        // ID / PlayerID
        std::uint64_t session_id() const { return id_; }
        void          set_player_id(std::uint64_t pid) { player_id_ = pid; }
        std::uint64_t player_id() const { return player_id_; }


        void         set_state(SessionState s) { state_ = s; }
        SessionState state() const { return state_; }

        void set_field_id(int fid) { fieldId_ = fid; }
        int  field_id() const { return fieldId_; }

    private:
        // ----- 기존 콜백들 -----
        static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
        static void read_cb(uv_stream_t* s, ssize_t nread, const uv_buf_t* buf);
        static void close_cb(uv_handle_t* handle);

        void on_read(ssize_t nread, const uv_buf_t* buf);
        void on_closed();

    private:

        struct PendingSend {
            std::vector<std::uint8_t> buf;
        };

        struct WriteReq {
            uv_write_t req{};
            std::vector<std::uint8_t> buf;
        };

        static void on_send_async(uv_async_t* h);
        void flush_send_queue();

    private:
        uv_loop_t* loop_{ nullptr };
        core::Dispatcher* dispatcher_{ nullptr };

        uv_tcp_t client_{};

        std::vector<std::uint8_t> recv_buf_;
        std::vector<std::uint8_t> scratch_;


        uv_async_t send_async_{};
        std::mutex send_mtx_;
        std::deque<PendingSend> send_q_;


        bool closing_{ false };

    private:
        std::uint64_t    id_{ 0 };
        std::uint64_t    player_id_{ 0 };


        SessionState     state_{ SessionState::Connected };
        int              fieldId_{ 0 };

        OnClose          on_close_;

        core::Worker* gameWorker_{ nullptr };
    };

    bool IsEnvelope(const uint8_t* data, size_t len);
    bool IsSkillEnvelope(const uint8_t* data, size_t len);
    bool IsFieldCmd(const uint8_t* data, size_t len);

} // namespace net

