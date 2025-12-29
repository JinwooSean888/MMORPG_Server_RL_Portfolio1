// AoiWorld.h
#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>


struct AoiVec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct AoiSectorCoord
{
    int x = 0;
    int y = 0;

    bool operator==(const AoiSectorCoord& other) const noexcept {
        return x == other.x && y == other.y;
    }

    struct Hasher {
        std::size_t operator()(const AoiSectorCoord& s) const noexcept {
            return (static_cast<std::size_t>(s.x) << 32)
                ^ static_cast<std::size_t>(s.y);
        }
    };
};

// AOI 이벤트 타입: 네트워크 패킷으로 매핑하면 됨
struct AoiEvent
{
    enum class Type
    {
        Enter,      // 어떤 엔티티가 내 시야에 새로 등장
        Leave,      // 내 시야에서 사라짐
        Move,       // 내 시야 안에서 이동
        Snapshot    // 새로 구독한 섹터의 초기 스냅샷
    };

    Type          type{};
    std::uint64_t subjectId = 0;
    AoiVec2       position{};    
};

using AoiSendCallback = std::function<void(std::uint64_t watcherId,
    const AoiEvent& ev)>;


class AoiWorld
{
public:

    struct Entity
    {
        std::uint64_t id = 0;
        bool          isPlayer = false;

        AoiVec2       pos{};
        AoiSectorCoord sector{};

        // 내가 구독 중인 섹터 집합
        std::unordered_set<AoiSectorCoord, AoiSectorCoord::Hasher> subscribed;
    };

    // 섹터 한 칸
    struct Sector
    {
        std::unordered_set<std::uint64_t> entities; // 이 섹터에 있는 엔티티 IDs
        std::unordered_set<std::uint64_t> watchers; // 이 섹터를 구독 중인 플레이어 IDs
    };

public:
    // sectorSize: 한 섹터의 길이 (예: 5m, 10m)
    // viewRadiusSectors: AOI 반경 (1이면 3x3, 2면 5x5)
    explicit AoiWorld(float sectorSize, int viewRadiusSectors);

    void set_send_callback(AoiSendCallback cb) { sendCb_ = std::move(cb); }

    // 엔티티 등록/삭제
    void add_entity(std::uint64_t id, bool isPlayer, const AoiVec2& pos);
    void remove_entity(std::uint64_t id);


    void move_entity(std::uint64_t id, const AoiVec2& newPos);


    void update_player_aoi(std::uint64_t playerId);

    const Entity* get_entity(std::uint64_t id) const;
    Entity* get_entity(std::uint64_t id);

private:
    using EntityMap = std::unordered_map<std::uint64_t, Entity>;
    using SectorMap = std::unordered_map<AoiSectorCoord, Sector, AoiSectorCoord::Hasher>;

    EntityMap entities_;
    SectorMap sectors_;

    float sectorSize_ = 1.0f;
    int   viewRadius_ = 1;

    AoiSendCallback sendCb_;

private:

    AoiSectorCoord world_to_sector(const AoiVec2& pos) const;
    Sector* get_or_create_sector(const AoiSectorCoord& c);
    const Sector* get_sector(const AoiSectorCoord& c) const;
    void enter_sector(std::uint64_t id, const AoiSectorCoord& c);
    void leave_sector(std::uint64_t id, const AoiSectorCoord& c);
    void rebuild_player_subscriptions(Entity& e);
    void collect_view_sectors(const Entity& e,
        std::vector<AoiSectorCoord>& out) const;
    void broadcast_to_sector_watchers(const AoiSectorCoord& c,
        const AoiEvent& ev, std::int64_t excludeId = 0);
};
static void collect_watchers(const AoiWorld::Sector* s, std::vector<uint64_t>& out);
