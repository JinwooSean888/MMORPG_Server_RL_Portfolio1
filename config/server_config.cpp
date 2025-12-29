#include "server_config.h"

#include <fstream>
#include <sstream>

#include <json.h>

namespace config {

    static std::string read_all(const std::string& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        std::ostringstream oss;
        oss << ifs.rdbuf();
        return oss.str();
    }

    bool LoadServerConfig(const std::string& path, ServerConfig& out, std::string* err) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            if (err) *err = "cannot open config file: " + path;
            return false;
        }

        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;

        Json::Value root;
        std::string errs;

        if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
            if (err) *err = "json parse failed: " + errs;
            return false;
        }

        // redis
        if (root.isMember("redis")) {
            auto r = root["redis"];
            if (r.isMember("host")) out.redis.host = r["host"].asString();
            if (r.isMember("port")) out.redis.port = r["port"].asInt();
            if (r.isMember("timeout_ms")) out.redis.timeout_ms = r["timeout_ms"].asInt();
        }

        // mysql
        if (root.isMember("mysql")) {
            auto m = root["mysql"];
            if (m.isMember("host")) out.mysql.host = m["host"].asString();
            if (m.isMember("port")) out.mysql.port = m["port"].asInt();
            if (m.isMember("user")) out.mysql.user = m["user"].asString();
            if (m.isMember("pass")) out.mysql.pass = m["pass"].asString();
            if (m.isMember("db")) out.mysql.db = m["db"].asString();
            if (m.isMember("charset")) out.mysql.charset = m["charset"].asString();
        }

        // storage
        if (root.isMember("storage")) {
            auto s = root["storage"];
            if (s.isMember("flush_interval_ms")) out.storage.flush_interval_ms = (std::uint64_t)s["flush_interval_ms"].asUInt64();
            if (s.isMember("db_max_queue")) out.storage.db_max_queue = (std::size_t)s["db_max_queue"].asUInt64();
            if (s.isMember("max_batch_uids")) out.storage.max_batch_uids = (std::size_t)s["max_batch_uids"].asUInt64();
        }

        return true;
    }

} // namespace config
