#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "core/core_types.h"
#include "worker.h"

namespace net {
    class Session; 
}

namespace core {

    //
    class WorkerManager {
    public:
        using key_type = std::string;

    
        static WorkerManager& instance();

        // 워커 등록/조회/정지
        bool insert(const key_type& key, Worker::Ptr worker);
        Worker::Ptr get(const key_type& key);
        void remove(const key_type& key);
        void stop_all();


    private:
        WorkerManager() = default;
        WorkerManager(const WorkerManager&) = delete;
        WorkerManager& operator=(const WorkerManager&) = delete;

        std::mutex                                      mutex_;
        std::unordered_map<key_type, Worker::Ptr>       workers_;
    };

} // namespace core
