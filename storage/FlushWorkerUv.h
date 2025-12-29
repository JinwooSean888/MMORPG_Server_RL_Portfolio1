#pragma once
#include <uv.h>
#include <atomic>
#include <cstdint>

namespace storage {

    class DirtyHub;
    class DBWorker;

    class FlushWorkerUv {
    public:
        FlushWorkerUv(uv_loop_t* loop, DirtyHub* hub, DBWorker* db);
        ~FlushWorkerUv();

        bool start(std::uint64_t interval_ms);
        void stop();

    private:
        static void on_timer(uv_timer_t* t);

    private:
        uv_loop_t* loop_{ nullptr };
        uv_timer_t timer_{};

        DirtyHub* hub_{ nullptr };
        DBWorker* db_{ nullptr };

        std::uint64_t interval_ms_{ 2000 };
        std::atomic<bool> running_{ false };
    };

} // namespace storage
