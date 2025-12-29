#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct redisContext;

namespace storage::redis {

    struct UserSnapshot {
        std::uint64_t uid = 0;
        float x = 0.f;
        float z = 0.f;
        int hp = 0;
        int sp = 0;

       
        std::string inv_json; 
    };

    // key helpers
    std::string KeyRt(std::uint64_t uid);
    std::string KeyInv(std::uint64_t uid);

    // rt만 Redis에 기록
    bool WriteUsersRtOnly(redisContext* c,
        const std::vector<UserSnapshot>& snap);

    // rt만 Redis에서 읽기 (성공한 uid만 out에 push)
    bool FetchUsers(redisContext* c,
        const std::vector<std::uint64_t>& uids,
        std::vector<UserSnapshot>& out);

} // namespace storage::redis
