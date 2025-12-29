#pragma once
#include <cstdint>
#include <vector>

namespace storage {

    struct DbJob
    {
        std::vector<std::uint64_t> uids;

 
        std::uint64_t enqueued_ms{ 0 };  
        std::uint32_t batch_id{ 0 };    

        DbJob() = default;
        explicit DbJob(std::vector<std::uint64_t>&& ids)
            : uids(std::move(ids)) {
        }

        bool empty() const noexcept { return uids.empty(); }
        std::size_t size() const noexcept { return uids.size(); }

        void clear() {
            uids.clear();
            enqueued_ms = 0;
            batch_id = 0;
        }
    };

} // namespace storage
