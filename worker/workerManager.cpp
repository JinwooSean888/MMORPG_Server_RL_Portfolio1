#include "workerManager.h"

namespace core {
  

    WorkerManager& WorkerManager::instance() {
        static WorkerManager g;
        return g;
    }

    bool WorkerManager::insert(const key_type& key, Worker::Ptr worker) {
        if (!worker) return false;
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = workers_.find(key);
        if (it != workers_.end())
            return false;

        workers_.emplace(key, std::move(worker));
        return true;
    }

    Worker::Ptr WorkerManager::get(const key_type& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = workers_.find(key);
        if (it == workers_.end())
            return nullptr;
        return it->second;
    }

    void WorkerManager::remove(const key_type& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        workers_.erase(key);
    }

    void WorkerManager::stop_all() {
        std::unordered_map<key_type, Worker::Ptr> copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            copy = workers_;
        }
        for (auto& kv : copy) {
            if (kv.second) {
                kv.second->stop();
            }
        }
    }

} // namespace core
