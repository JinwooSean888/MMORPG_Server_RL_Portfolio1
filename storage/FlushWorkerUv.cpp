#include "storage/FlushWorkerUv.h"
#include "storage/DirtyHub.h"
#include "storage/DBWorker.h"

namespace storage {

    FlushWorkerUv::FlushWorkerUv(uv_loop_t* loop, DirtyHub* hub, DBWorker* db)
        : loop_(loop), hub_(hub), db_(db) {
        timer_.data = this;
    }

    FlushWorkerUv::~FlushWorkerUv() {
        stop();
    }

    bool FlushWorkerUv::start(std::uint64_t interval_ms) {
        interval_ms_ = interval_ms;

        int rc = uv_timer_init(loop_, &timer_);
        if (rc != 0) return false;

        rc = uv_timer_start(&timer_, &FlushWorkerUv::on_timer, /*timeout*/100, /*repeat*/interval_ms_);
        if (rc != 0) return false;

        running_.store(true);
        return true;
    }

    void FlushWorkerUv::stop() {
        if (!running_.exchange(false)) return;

        uv_timer_stop(&timer_);
        uv_close(reinterpret_cast<uv_handle_t*>(&timer_), nullptr);
    }

    void FlushWorkerUv::on_timer(uv_timer_t* t) {
        auto* self = static_cast<FlushWorkerUv*>(t->data);
        if (!self || !self->running_.load()) return;

        auto uids = self->hub_->steal_all();
        if (uids.empty()) return;

        DbJob job;
        job.uids = std::move(uids);
        self->db_->try_push(std::move(job));
    }

} // namespace storage
