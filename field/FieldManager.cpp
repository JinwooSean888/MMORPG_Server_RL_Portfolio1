#include "field/FieldManager.h"
#include "worker/FieldWorker.h"          
#include "storage/StorageSystem.h"      
namespace core {

    std::shared_ptr<FieldWorker> FieldManager::create_field(int fieldId)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = fields_.find(fieldId);
        if (it != fields_.end()) {
            return it->second;
        }

        if (!storage_) {
 

            std::cout << "[FieldManager] storage_ is null. Call set_storage() before create_field().\n";
            return nullptr;
        }

        // DirtyHub 레퍼런스 생성자 주입
        auto fw = std::make_shared<FieldWorker>(fieldId, storage_->dirty());

        fw->start();
        fields_[fieldId] = fw;
        return fw;
    }

    std::shared_ptr<FieldWorker> FieldManager::get_field(int fieldId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = fields_.find(fieldId);
        if (it != fields_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void FieldManager::stop_all()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, w] : fields_) {
            if (w) {
                w->stop();
            }
        }
        fields_.clear();
    }
    void FieldManager::set_storage(storage::StorageSystem* ss)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_ = ss;
    }



} // namespace core