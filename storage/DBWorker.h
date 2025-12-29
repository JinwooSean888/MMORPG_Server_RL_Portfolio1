#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>
#include "storage/DBworker/DbJob.h"
namespace storage {


    class DBWorker {
    public:
        using Handler = std::function<void(const DbJob&)>;
        using Hook = std::function<void()>;

        DBWorker(Handler handler, Hook on_start = {}, Hook on_stop = {});

        void start();
        void stop();   // graceful stop signal
        void join();

        bool try_push(DbJob job);

    private:
        void run();

    private:

        Hook on_start_;
        Hook on_stop_;

        std::atomic<bool> running_{ false };
        std::thread th_;

        std::mutex mu_;
        std::condition_variable cv_;
        std::queue<DbJob> q_;
		size_t max_queue_{ 360 };

        Handler handler_;
    };

} // namespace storage
