// net/session.cpp

#include "session.h"
#include "net/uv_utils.h"

#include "worker/worker.h"
#include "worker/codec.h"
#include "worker/fieldWorker.h"

#include <iostream>

namespace net {

    Session::Session(uv_loop_t* loop, core::Dispatcher* disp)
        : loop_(loop)
        , dispatcher_(disp)
        , state_(SessionState::Connected)
        , fieldId_(0)
    {
        id_ = core::next_session_id();

        uv_tcp_init(loop_, &client_);
        client_.data = this;

        recv_buf_.reserve(64 * 1024);
        send_async_.data = this;
        uv_async_init(loop_, &send_async_, &Session::on_send_async);
    }

    Session::~Session() {
    }

    uv_stream_t* Session::stream() {
        return reinterpret_cast<uv_stream_t*>(&client_);
    }

    void Session::start() {
        uv_read_start(stream(), &Session::alloc_cb, &Session::read_cb);
    }


    void Session::alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        auto* self = reinterpret_cast<Session*>(handle->data);
        self->scratch_.resize(suggested_size);
        *buf = uv_buf_init(
            reinterpret_cast<char*>(self->scratch_.data()),
            static_cast<unsigned>(suggested_size)
        );
    }

    void Session::read_cb(uv_stream_t* s, ssize_t nread, const uv_buf_t* buf) {
        auto* self = reinterpret_cast<Session*>(s->data);
        auto  session = self->shared_from_this();
        session->on_read(nread, buf);
    }

    void Session::close_cb(uv_handle_t* handle) {
        auto* self = reinterpret_cast<Session*>(handle->data);
        auto  session = self->shared_from_this();
        session->on_closed();
    }

    void Session::on_read(ssize_t nread, const uv_buf_t* buf) {
        if (nread < 0) {
            uv_read_stop(stream());
            closing_ = true;

            uv_close(reinterpret_cast<uv_handle_t*>(&send_async_), nullptr);

            uv_close(reinterpret_cast<uv_handle_t*>(&client_), &Session::close_cb);
            return;
        }
        if (nread == 0) return;

        recv_buf_.insert(
            recv_buf_.end(),
            reinterpret_cast<const uint8_t*>(buf->base),
            reinterpret_cast<const uint8_t*>(buf->base) + nread
        );

        std::size_t offset = 0;

        while (true) {
            if (recv_buf_.size() - offset < proto::Frame::kHeader)
                break;

            uint32_t len = proto::Frame::read_len(recv_buf_.data() + offset);
            if (recv_buf_.size() - offset - proto::Frame::kHeader < len)
                break;

            const uint8_t* payload =
                recv_buf_.data() + offset + proto::Frame::kHeader;

            if (gameWorker_) {
                core::NetMessage msg;
                msg.session = shared_from_this();
                msg.payload.assign(payload, payload + len);

                if (state_ == SessionState::InField) {
                    if (IsSkillEnvelope(payload, len)) {
                        msg.type = core::MessageType::NetEnvelope;
                        gameWorker_->push(std::move(msg));
                        // std::cout << "[SV] Skill Envelope(InField) -> GameWorker\n";
                    }
                    else if (IsFieldCmd(payload, len)) {
                        msg.type = core::MessageType::Custom;
                        core::SendToFieldWorker(fieldId_, std::move(msg));
                        // std::cout << "[SV] FieldCmd(InField) -> FieldWorker\n";
                    }
                    else {
                        std::cout << "[SV] Unknown payload in InField len=" << len << "\n";
                    }
                }
                else {
                    if (IsEnvelope(payload, len)) {
                        msg.type = core::MessageType::NetEnvelope;
                        gameWorker_->push(std::move(msg));
                        // std::cout << "[SV] Envelope -> GameWorker\n";
                    }
                    else {
                        std::cout << "[SV] verify_envelope FAILED len=" << len << "\n";
                    }
                }
            }

            offset += proto::Frame::kHeader + len;
        }

        if (offset > 0) {
            recv_buf_.erase(
                recv_buf_.begin(),
                recv_buf_.begin() + static_cast<long long>(offset)
            );
        }
    }

    void Session::on_closed() {
        if (on_close_) {
            on_close_(shared_from_this());
        }
    }

    void Session::send_payload(const std::uint8_t* payload, std::uint32_t len) {
        if (closing_) return;
        if (!payload || len == 0) return;

        PendingSend ps;
        proto::Frame::write(ps.buf, payload, len);

        {
            std::lock_guard<std::mutex> lock(send_mtx_);
            send_q_.push_back(std::move(ps));
        }

        uv_async_send(&send_async_);
    }

    void Session::on_send_async(uv_async_t* h) {
        auto* self = reinterpret_cast<Session*>(h->data);
        if (!self || self->closing_) return;
        self->flush_send_queue();
    }


    void Session::flush_send_queue() {
        std::deque<PendingSend> local;

        {
            std::lock_guard<std::mutex> lock(send_mtx_);
            local.swap(send_q_);
        }

        for (auto& ps : local) {
            auto* wr = new WriteReq{};
            wr->buf = std::move(ps.buf);
            wr->req.data = wr;

            uv_buf_t b = uv_buf_init(
                reinterpret_cast<char*>(wr->buf.data()),
                static_cast<unsigned>(wr->buf.size())
            );

            int r = uv_write(
                &wr->req,
                stream(),
                &b,
                1,
                [](uv_write_t* req, int status) {
                    auto* w = reinterpret_cast<WriteReq*>(req->data);

                    delete w;
                }
            );

            if (r < 0) {
     
                delete wr;

            }
        }
    }


    bool IsEnvelope(const uint8_t* data, size_t len)
    {
        flatbuffers::Verifier v(data, len);
        return v.VerifyBuffer<game::Envelope>(nullptr);
    }

    bool IsSkillEnvelope(const uint8_t* data, size_t len)
    {
        flatbuffers::Verifier v(data, len);
        if (!v.VerifyBuffer<game::Envelope>(nullptr))
            return false;

        auto env = flatbuffers::GetRoot<game::Envelope>(data);
        if (!env) return false;

        return env->pkt_type() == game::Packet_SkillCmd;
    }

    bool IsFieldCmd(const uint8_t* data, size_t len)
    {
        flatbuffers::Verifier v(data, len);
        return v.VerifyBuffer<field::FieldCmd>(nullptr);
    }

} // namespace net
