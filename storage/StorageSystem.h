#pragma once
#include <cstdint>
#include <memory>
#include "config/server_config.h"
#include "storage/redis/redisUserCache.h"
struct uv_loop_s;

namespace storage {

    class DirtyHub;

    class StorageSystem {
    public:
        StorageSystem();
        ~StorageSystem();

        StorageSystem(StorageSystem&&) noexcept;
        StorageSystem& operator=(StorageSystem&&) noexcept;

        StorageSystem(const StorageSystem&) = delete;
        StorageSystem& operator=(const StorageSystem&) = delete;

        static StorageSystem Create(uv_loop_s* loop, const config::ServerConfig& cfg);

        void start();
        void stop();


        DirtyHub& dirty();

        void enqueue_rt_write(const storage::redis::UserSnapshot& s);
    private:
        struct Impl;                
        std::unique_ptr<Impl> impl_; 
    };

} // namespace storage
