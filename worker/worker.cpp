#include "worker.h"
#include "workerManager.h"

namespace core {

    // ================ Worker 구현 ================

    Worker::Worker(std::string name)
        : name_(std::move(name))
    {
    }

    Worker::~Worker() {
        stop();
    }

    void Worker::start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) {

            return;
        }

        thread_ = std::thread([this] {
            this->loop();
            });
    }

    void Worker::stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {

            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);

        }
        cv_.notify_all();

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void Worker::set_on_message(Callback cb) {
        on_message_ = std::move(cb);
    }

    void Worker::push(NetMessage msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(msg));
        } 
        cv_.notify_one();
    }

    void Worker::loop() {
        while (running_.load()) {
            NetMessage msg;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] {
                    return !running_.load() || !queue_.empty();
                    });

                if (!running_.load() && queue_.empty())
                    break;

                msg = std::move(queue_.front());
                queue_.pop();
            }

            if (on_message_) {
                on_message_(msg);
            }
        }
    }

    INT32 Worker::GetMessageCount()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<INT32>(queue_.size());
    }

    // ================ GameWorker 헬퍼 구현 ================

    Worker::Ptr GetGameWorker() {
        return WorkerManager::instance().get(GAME_WORKER_NAME);
    }

    bool CreateGameWorker() {
        auto& mgr = WorkerManager::instance();
        auto existing = mgr.get(GAME_WORKER_NAME);
        if (existing) return true;

        auto worker = std::make_shared<Worker>(GAME_WORKER_NAME);
        if (!worker) return false;

        if (!mgr.insert(GAME_WORKER_NAME, worker))
            return false;

        worker->start();
        return true;
    }

    bool SendToGameWorker(NetMessage msg) {
        auto worker = GetGameWorker();
        if (!worker) return false;
        worker->push(std::move(msg));
        return true;
    }

} // namespace core
