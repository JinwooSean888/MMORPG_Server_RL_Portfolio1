#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <chrono>

namespace core {

    class TickWorkers {
    public:
        TickWorkers(int threads, int tick_ms);
        ~TickWorkers();

        void stop();
        void on_tick(std::function<void(int)> cb);

    private:
        void loop(int idx);

        std::vector<std::thread> workers_;
        std::function<void(int)> on_tick_;
        int tick_ms_;
        std::atomic<bool> running_;
    };

} // namespace core
