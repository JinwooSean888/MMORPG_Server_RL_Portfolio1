#include "core/thread_pool.h"

namespace core {

    TickWorkers::TickWorkers(int threads, int tick_ms)
        : tick_ms_(tick_ms), running_(true)
    {
        for (int i = 0; i < threads; i++) {
            workers_.emplace_back([this, i] { this->loop(i); });
        }
    }

    TickWorkers::~TickWorkers() {
        stop();
    }

    void TickWorkers::stop() {
        bool expected = true;
        if (running_.compare_exchange_strong(expected, false)) {
            for (auto& t : workers_) {
                if (t.joinable()) t.join();
            }
        }
    }

    void TickWorkers::on_tick(std::function<void(int)> cb) {
        on_tick_ = std::move(cb);
    }

    void TickWorkers::loop(int idx) {
        auto tick = std::chrono::milliseconds(tick_ms_);

        while (running_.load()) {
            auto start = std::chrono::steady_clock::now();
            if (on_tick_) on_tick_(idx);

            auto end = std::chrono::steady_clock::now();
            auto spent = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (spent < tick) {
                std::this_thread::sleep_for(tick - spent);
            }
        }
    }

} // namespace core
