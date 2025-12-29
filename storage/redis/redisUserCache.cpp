#include "storage/redis/redisUserCache.h"

#include <hiredis.h>
#include <cstdlib>   // strtof, atoi
#include <cstdio>

namespace storage::redis {

    std::string KeyRt(std::uint64_t uid) { return "u:" + std::to_string(uid) + ":rt"; }
    std::string KeyInv(std::uint64_t uid) { return "u:" + std::to_string(uid) + ":inv"; }

    // ---------- parse helpers ----------
    static bool parse_float(const redisReply* r, float& out) {
        if (!r) return false;

        switch (r->type) {
        case REDIS_REPLY_STRING:
            if (!r->str) return false;
            out = std::strtof(r->str, nullptr);
            return true;
        case REDIS_REPLY_INTEGER:
            out = static_cast<float>(r->integer);
            return true;
        case REDIS_REPLY_NIL:
        default:
            return false;
        }
    }

    static bool parse_int(const redisReply* r, int& out) {
        if (!r) return false;

        switch (r->type) {
        case REDIS_REPLY_INTEGER:
            out = static_cast<int>(r->integer);
            return true;
        case REDIS_REPLY_STRING:
            if (!r->str) return false;
            out = std::atoi(r->str);
            return true;
        case REDIS_REPLY_NIL:
        default:
            return false;
        }
    }

    // ---------- write rt only ----------
    bool WriteUsersRtOnly(redisContext* c, const std::vector<UserSnapshot>& snap)
    {
        if (!c) return false;
        if (snap.empty()) return true;

        // pipeline append
        for (const auto& s : snap) {
            const auto krt = KeyRt(s.uid);

            // HSET key x <float> z <float> hp <int> sp <int>
            if (redisAppendCommand(
                c,
                "HSET %s x %f z %f hp %d sp %d",
                krt.c_str(),
                (double)s.x, (double)s.z,
                s.hp, s.sp) != REDIS_OK)
            {
                // Append 실패는 보통 connection 문제
                return false;
            }
        }

        // collect replies (명령 수 = snap.size())
        for (size_t i = 0; i < snap.size(); ++i) {
            redisReply* r = nullptr;
            if (redisGetReply(c, (void**)&r) != REDIS_OK) {
                if (r) freeReplyObject(r);
                return false;
            }
            if (r) freeReplyObject(r);
        }

        return true;
    }

    // ---------- fetch rt only ----------
    bool FetchUsers(redisContext* c,
        const std::vector<std::uint64_t>& uids,
        std::vector<UserSnapshot>& out)
    {
        out.clear();
        if (!c) return false;
        if (uids.empty()) return true;

        //  pipeline append: uid마다 HMGET 1개
        for (auto uid : uids) {
            const auto krt = KeyRt(uid);

            if (redisAppendCommand(c, "HMGET %s x z hp sp", krt.c_str()) != REDIS_OK) {
                return false;
            }
        }

        out.reserve(uids.size());

        for (auto uid : uids) {
            redisReply* r = nullptr;
            if (redisGetReply(c, (void**)&r) != REDIS_OK) {
                if (r) freeReplyObject(r);
                return false;
            }

            UserSnapshot s;
            s.uid = uid;

            bool ok = false;

            if (r && r->type == REDIS_REPLY_ARRAY && r->elements == 4) {
                float x = 0.f, z = 0.f;
                int hp = 0, sp = 0;

                const bool okx = parse_float(r->element[0], x);
                const bool okz = parse_float(r->element[1], z);
                const bool okhp = parse_int(r->element[2], hp);
                const bool oksp = parse_int(r->element[3], sp);

                ok = okx && okz && okhp && oksp;
                if (ok) {
                    s.x = x; s.z = z; s.hp = hp; s.sp = sp;
                    s.inv_json.clear(); // 현재는 무시
                }
            }

            if (r) freeReplyObject(r);

            if (ok) out.push_back(std::move(s)); // rt가 유효한 uid만
        }

        return true;
    }

} // namespace storage::redis
