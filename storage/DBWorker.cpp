#include "storage/DBWorker.h"
namespace storage {

    DBWorker::DBWorker(Handler handler, Hook on_start, Hook on_stop)
        : handler_(std::move(handler))
        , on_start_(std::move(on_start))
        , on_stop_(std::move(on_stop))
    {
    }

    void DBWorker::start() {
        running_.store(true);
        th_ = std::thread([this] { run(); });
    }

    void DBWorker::stop() {
        running_.store(false);
        cv_.notify_all();
    }

    void DBWorker::join() {
        if (th_.joinable()) th_.join();
    }

    bool DBWorker::try_push(DbJob job) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (q_.size() >= max_queue_) return false;   // 꽉 찼으면 거절
            q_.push(std::move(job));
        }
        cv_.notify_one();
        return true;
    }

    void DBWorker::run() {
        if (on_start_) on_start_();   // 워커 스레드에서 1회 실행

        while (true) {
            DbJob job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [&] { return !q_.empty() || !running_.load(); });

                if (!running_.load() && q_.empty())
                    break;

                job = std::move(q_.front());
                q_.pop();
            }

            if (handler_) handler_(job);
        }

        if (on_stop_) on_stop_();     // 종료 직전 정리
    }

} // namespace storage
