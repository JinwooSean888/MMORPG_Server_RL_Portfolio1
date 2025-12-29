#include <uv.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <mysql.h>
#include <hiredis.h>
#include "storage/StorageSystem.h"
#include "storage/DirtyHub.h"
#include "storage/DBworker/DbJob.h"
#include "storage/DBWorker.h"
#include "storage/redis/redisUserCache.h"
#include "storage/DB/userUpsert.h"
#include "storage/DB/Proc/userStateProc.h"


namespace storage {

    // ---- StorageSystem::Impl ----
    struct StorageSystem::Impl {
        uv_loop_s* loop{ nullptr };
        uv_timer_t timer{};
        std::atomic<bool> started{ false };

        config::ServerConfig cfg;
        DirtyHub dirty;
        DBWorker db;

        redisContext* redis_ctx{ nullptr };
        MYSQL* mysql{ nullptr };
		std::vector<storage::redis::UserSnapshot> snap;// 재사용 버퍼
        std::mutex rt_mu_;
        std::vector<storage::redis::UserSnapshot> rt_q_;
        std::vector<storage::redis::UserSnapshot> rt_local_;
        std::atomic<uint32_t> rt_pending_{ 0 };


        std::uint64_t interval_ms{ 2000 };

        void handle_db_job(const DbJob& job) {

            if (!ensure_connected()) {

                std::cout << "[DBWorker] ensure_connected failed\n";
                return;
            }


            flush_rt_writes();
            if (!ensure_connected()) return;

            if (job.uids.empty()) return;

            if (!storage::redis::FetchUsers(redis_ctx, job.uids, snap)) {
                std::cout << "[DBWorker] FetchUsers failed -> disconnect\n";
                disconnect_db();
                return;
            }

            if (snap.empty()) return;

            if (!storage::sql::CallSpUpsertUserStateBatch(mysql, snap)) {
                std::cout << "[DBWorker] SP upsert failed: " << mysql_error(mysql) << "\n";
                disconnect_db();
                return;
            }
        }
        bool ensure_connected()
        {
            if (redis_ctx && mysql) return true;

 
            disconnect_db();


            connect_db();

            return (redis_ctx && mysql);
        }

        void connect_db() {

            if (redis_ctx && mysql)
                return;
            printf("client=%s\n", mysql_get_client_info());

            {
                timeval tv;
                tv.tv_sec = cfg.redis.timeout_ms / 1000;
                tv.tv_usec = (cfg.redis.timeout_ms % 1000) * 1000;

                redis_ctx = redisConnectWithTimeout(
                    cfg.redis.host.c_str(),
                    cfg.redis.port,
                    tv
                );

                if (!redis_ctx || redis_ctx->err) {
                    std::cout << "[DBWorker] Redis connect failed: "
                        << (redis_ctx ? redis_ctx->errstr : "null") << "\n";
                    if (redis_ctx) {
                        redisFree(redis_ctx);
                        redis_ctx = nullptr;
                    }
                    return;
                }
            }
            mysql = mysql_init(nullptr);
            if (!mysql) {
                std::cout << "[DBWorker] mysql_init failed\n";
                disconnect_db();
                return;
            }
            mysql_options(mysql, MYSQL_SET_CHARSET_NAME, cfg.mysql.charset.c_str());

            if (!mysql_real_connect(
                mysql,
                cfg.mysql.host.c_str(),
                cfg.mysql.user.c_str(),
                cfg.mysql.pass.c_str(),
                cfg.mysql.db.c_str(),
                cfg.mysql.port,
                nullptr,
                0
            )) {
                std::cout << "[DBWorker] MySQL connect failed: "
                    << mysql_error(mysql) << "\n";
                disconnect_db();
                return;
            }

            std::cout << "[DBWorker] DB connections established "
                << "(redis " << cfg.redis.host << ":" << cfg.redis.port
                << ", mysql " << cfg.mysql.host << ":" << cfg.mysql.port << ")\n";
        }


        void disconnect_db() {
            if (redis_ctx) {
                redisFree(redis_ctx);
                redis_ctx = nullptr;
            }
            if (mysql) {
                mysql_close(mysql);
                mysql = nullptr;
            }
        }


        Impl(uv_loop_s* l, const config::ServerConfig& c)
            : loop(l)
            , cfg(c)
            , db(
                [this](const DbJob& job) { this->handle_db_job(job); },
                [this]() { this->connect_db(); },     //  워커 스레드에서 1회 connect
                [this]() { this->disconnect_db(); }   // 워커 종료 시 close
            )
        {
            timer.data = this;
        }


        static std::uint64_t now_ms() {
            using namespace std::chrono;
            return (std::uint64_t)duration_cast<milliseconds>(
                steady_clock::now().time_since_epoch()).count();
        }

        static void on_timer(uv_timer_t* t)
        {
            auto* self = static_cast<Impl*>(t->data);
            if (!self || !self->started.load()) return;

            auto uids = self->dirty.steal_all();

   
            if (uids.empty()) {
                if (self->rt_pending_.load(std::memory_order_relaxed) > 0) {
                    DbJob job;
                    (void)self->db.try_push(std::move(job));
                }
                return;
            }

            const std::size_t maxBatch =
                std::max<std::size_t>(1, self->cfg.storage.max_batch_uids);

            for (std::size_t i = 0; i < uids.size(); i += maxBatch) {
                auto end = std::min(uids.size(), i + maxBatch);

                DbJob job;
                job.uids.assign(uids.begin() + i, uids.begin() + end);

                if (!self->db.try_push(std::move(job))) {
                    for (std::size_t k = i; k < end; ++k)
                        self->dirty.mark_dirty(uids[k]);
                }
            }
        }




        bool start(std::uint64_t ms) {
            if (started.exchange(true)) return true;
            interval_ms = ms;

            db.start(); 

            int rc = uv_timer_init(loop, &timer);
            if (rc != 0) return false;

            rc = uv_timer_start(&timer, &Impl::on_timer, 100, interval_ms);
            return rc == 0;
        }

        void stop() {
            if (!started.exchange(false)) return;

            uv_timer_stop(&timer);
            uv_close(reinterpret_cast<uv_handle_t*>(&timer), nullptr);

            db.stop();
            db.join();

            if (redis_ctx) { redisFree(redis_ctx); redis_ctx = nullptr; }
            if (mysql) { mysql_close(mysql); mysql = nullptr; }


            std::cout << "[Storage] stopped.\n";
        }
        bool start() {
            return start(cfg.storage.flush_interval_ms);
        } 

        void flush_rt_writes()
        {
            rt_local_.clear();

            {
                std::lock_guard<std::mutex> lk(rt_mu_);
                if (rt_q_.empty()) return;
                rt_local_.swap(rt_q_);
            }

            rt_pending_.fetch_sub((uint32_t)rt_local_.size(), std::memory_order_relaxed);

            if (!redis_ctx) return;

            if (!storage::redis::WriteUsersRtOnly(redis_ctx, rt_local_)) {
                disconnect_db();
                return;
            }
            std::cout << "[Storage] flush_rt_writes n=" << rt_local_.size() << "\n";
        }

    };


    StorageSystem::StorageSystem() = default;
    StorageSystem::~StorageSystem() = default;

    StorageSystem::StorageSystem(StorageSystem&&) noexcept = default;
    StorageSystem& StorageSystem::operator=(StorageSystem&&) noexcept = default;
    StorageSystem StorageSystem::Create(uv_loop_s* loop, const config::ServerConfig& cfg) {
        StorageSystem ss;
        ss.impl_ = std::make_unique<Impl>(loop, cfg);
        return ss;
    }

    void StorageSystem::start() {
        if (!impl_) return;
        if (!impl_->start()) std::cout << "[Storage] start failed\n";
    }

    void StorageSystem::stop() {
        if (!impl_) return;
        impl_->stop();
    }

    DirtyHub& StorageSystem::dirty() {
        return impl_->dirty;
    }

    void StorageSystem::enqueue_rt_write(const storage::redis::UserSnapshot& s)
    {
        if (!impl_) return;
        {
            std::lock_guard<std::mutex> lk(impl_->rt_mu_);
            impl_->rt_q_.push_back(s);
        }
        impl_->rt_pending_.fetch_add(1, std::memory_order_relaxed);
    }


} // namespace storage
