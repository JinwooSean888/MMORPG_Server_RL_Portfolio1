#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>

#include "worker/FieldWorker.h" 
namespace storage { class StorageSystem; class DirtyHub; }

namespace core {

    class FieldManager {
    public:
        static FieldManager& instance()
        {
            static FieldManager inst;
            return inst;
        }
        void set_storage(storage::StorageSystem* ss);
        std::shared_ptr<FieldWorker> create_field(int fieldId);
        std::shared_ptr<FieldWorker> get_field(int fieldId);
        void stop_all();        

        template <typename Fn>
        void for_each_field(Fn&& fn)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [id, w] : fields_) {
                if (w) {
                    fn(w);
                }
            }
        }

    private:
        FieldManager() = default;
        ~FieldManager() = default;

        FieldManager(const FieldManager&) = delete;
        FieldManager& operator=(const FieldManager&) = delete;

    private:
        std::mutex mutex_;
        std::unordered_map<int, std::shared_ptr<FieldWorker>> fields_;
        storage::StorageSystem* storage_{ nullptr };
    };

} // namespace core