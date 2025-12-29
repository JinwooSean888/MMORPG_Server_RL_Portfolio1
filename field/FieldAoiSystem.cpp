// FieldAoiSystem.cpp
#include "FieldAoiSystem.h"

namespace core {

    FieldAoiSystem::FieldAoiSystem(int fieldId,
        float sectorSize,
        int   viewRadiusSectors)
        : fieldId_(fieldId)
        , aoi_(sectorSize, viewRadiusSectors)
    {        
        
    }

    void FieldAoiSystem::set_send_func(FieldAoiSendFunc func)
    {
        sendFunc_ = std::move(func);
        setup_aoi_callback();   // 이 시점에서만 AOI 콜백을 물린다
    }

    void FieldAoiSystem::setup_aoi_callback()
    {
        aoi_.set_send_callback(
            [this](std::uint64_t watcherId, const AoiEvent& ev)
            {
                // watchers_ 갱신
                auto& vec = watchers_[ev.subjectId];

                switch (ev.type)
                {
                case AoiEvent::Type::Snapshot:
                case AoiEvent::Type::Enter:
                case AoiEvent::Type::Move:
                {
                    // subjectId 를 보고 있는 watcher 리스트에 추가 (중복 체크)
                    auto it = std::find(vec.begin(), vec.end(), watcherId);
                    if (it == vec.end())
                        vec.push_back(watcherId);
                    break;
                }
                case AoiEvent::Type::Leave:
                {
                    auto it = std::find(vec.begin(), vec.end(), watcherId);
                    if (it != vec.end())
                        vec.erase(it);
                    break;
                }
                }

                // 아직 초기화 중이면 외부로는 안 보냄
                if (!initialized_ || !sendFunc_)
                    return;

                // 원래 하던 FieldCmd/CombatEvent 전송
                sendFunc_(watcherId, ev);
            }
        );
    }


    void FieldAoiSystem::add_entity(std::uint64_t id, bool isPlayer, float x, float y)
    {
        AoiVec2 pos{ x, y };
        aoi_.add_entity(id, isPlayer, pos);
    }

    void FieldAoiSystem::move_entity(std::uint64_t id, float x, float y)
    {
        AoiVec2 pos{ x, y };
        aoi_.move_entity(id, pos);
    }

    void FieldAoiSystem::remove_entity(std::uint64_t id)
    {
        aoi_.remove_entity(id);
        watchers_.erase(id);
    }

    void FieldAoiSystem::for_each_watcher(
        uint64_t subjectId,
        std::function<void(uint64_t watcherId)> fn)
    {
        auto it = watchers_.find(subjectId);
        if (it == watchers_.end()) return;

        for (uint64_t watcher : it->second)
            fn(watcher);
    }
} // namespace core
