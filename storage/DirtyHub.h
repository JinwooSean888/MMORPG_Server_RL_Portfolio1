#pragma once
#include <unordered_set>
#include <vector>
#include <mutex>
#include <cstdint>

namespace storage {

    class DirtyHub {
    public:
        static constexpr std::size_t kShards = 64;

        void mark_dirty(std::uint64_t uid) {
            auto& shard = shards_[uid & (kShards - 1)];
            std::lock_guard<std::mutex> lk(shard.mu);
            shard.set.insert(uid);
        }


        std::vector<std::uint64_t> steal_all() {
            std::vector<std::uint64_t> out;

            for (auto& s : shards_) {
                std::lock_guard<std::mutex> lk(s.mu);
                out.insert(out.end(), s.set.begin(), s.set.end());
                s.set.clear();
            }
            return out;
        }

    private:
        struct Shard {
            std::mutex mu;
            std::unordered_set<std::uint64_t> set;
        };
        Shard shards_[kShards];
    };

} // namespace storage
