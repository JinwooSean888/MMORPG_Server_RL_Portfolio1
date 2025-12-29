#pragma once
#include <string>
#include <cstdint>

namespace config {

    struct RedisConfig {
        std::string host = "127.0.0.1";
        int port = 6379;
        int timeout_ms = 200;
    };

    struct MySqlConfig {
        std::string host = "127.0.0.1";
        int port = 3306;
        std::string user = "root";
        std::string pass = "";
        std::string db = "game";
        std::string charset = "utf8mb4";
    };

    struct StorageConfig {
        std::uint64_t flush_interval_ms = 2000;
        std::size_t db_max_queue = 32;
        std::size_t max_batch_uids = 2000;
    };

    struct ServerConfig {
        RedisConfig redis;
        MySqlConfig mysql;
        StorageConfig storage;
    };

    // 파일에서 로드 (jsoncpp)
    bool LoadServerConfig(const std::string& path, ServerConfig& out, std::string* err = nullptr);

} // namespace config
