// FieldWorker.cpp
#include "fieldWorker.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

#include "workerManager.h"

#include "net/session.h"
#include "net/sessionManager.h"

#include "field/FieldAoiSystem.h"
#include "field/FieldManager.h"

#include "game/PlayerManager.h"

#include "field/monster/MonsterWorld.h"
#include "field/monster/MonsterEnvironment.h"

#include "storage/StorageSystem.h"
#include "storage/DirtyHub.h"

#include "proto/generated/field_generated.h"
#include "proto/generated/game_generated.h"

using net::SessionManager;

namespace core {
    namespace {

        std::string make_field_worker_name(int fieldId) {
            return "FieldWorker_" + std::to_string(fieldId);
        }

        bool is_monster_id(std::uint64_t id) {
            return id >= 1000; // 프로젝트 규칙 유지
        }

        field::FieldCmdType to_field_cmd_type(AoiEvent::Type t) {
            switch (t) {
            case AoiEvent::Type::Snapshot:
            case AoiEvent::Type::Enter: return field::FieldCmdType::FieldCmdType_Enter;
            case AoiEvent::Type::Leave: return field::FieldCmdType::FieldCmdType_Leave;
            case AoiEvent::Type::Move:  return field::FieldCmdType::FieldCmdType_Move;
            default:                    return field::FieldCmdType::FieldCmdType_Move;
            }
        }

    } // namespace

    FieldWorker::FieldWorker(int fieldId, storage::DirtyHub& hub)
        : Worker(make_field_worker_name(fieldId))
        , fieldId_(fieldId)
        , monsterWorld_()
        , env_(monsterWorld_)
        , dirtyHub_(hub)
    {
        init_monster_env();

        aoiSystem_ = std::make_shared<FieldAoiSystem>(fieldId_, 15.0f, 2);
        aoiSystem_->set_send_func([this](std::uint64_t watcherId, const AoiEvent& ev) {
            auto sess = SessionManager::instance().find_by_player_id(watcherId);
            if (!sess) return;

            flatbuffers::FlatBufferBuilder fbb;

            auto pos = field::CreateVec2(fbb, ev.position.x, ev.position.y);
            const field::FieldCmdType cmdType = to_field_cmd_type(ev.type);

            const bool isMonster = is_monster_id(ev.subjectId);
            const field::EntityType et = isMonster
                ? field::EntityType::EntityType_Monster
                : field::EntityType::EntityType_Player;

            std::string prefabName = get_prefab_name(ev.subjectId, isMonster);
            if (prefabName.empty()) prefabName = "Default";
            auto prefabStr = fbb.CreateString(prefabName);

            auto cmd = field::CreateFieldCmd(
                fbb,
                cmdType,
                et,
                ev.subjectId,
                pos,
                0,
                prefabStr
            );

            auto envOffset = field::CreateEnvelope(
                fbb,
                field::Packet::Packet_FieldCmd,
                cmd.Union()
            );
            fbb.Finish(envOffset);

            sess->send_payload(
                fbb.GetBufferPointer(),
                static_cast<std::uint32_t>(fbb.GetSize())
            );
            });

        set_on_message([this](const NetMessage& msg) { handle_message(msg); });

        if (fieldId == 1000) {
            SpawnMonstersEvenGrid(1000);
        }

        aoiSystem_->set_initialized(true);
    }

    FieldWorker::~FieldWorker() = default;

    void FieldWorker::handle_message(const NetMessage& msg)
    {
        if (!aoiSystem_) return;

        if (msg.type == MessageType::SkillCmd) {
            handle_skill(msg);
            return;
        }

        if (msg.type != MessageType::Custom) return;

        const uint8_t* buf = msg.payload.data();
        auto env = field::GetEnvelope(buf);
        if (!env) {
            std::cout << "[WARN] Invalid field envelope\n";
            return;
        }

        if (env->pkt_type() != field::Packet::Packet_FieldCmd) return;

        auto* cmd = env->pkt_as_FieldCmd();
        if (!cmd) return;

        if (cmd->type() == field::FieldCmdType::FieldCmdType_Move) {
            on_client_move_input(*cmd, msg.session);
        }
    }

    void FieldWorker::write_player_rt_enqueue(uint64_t uid, const Player& p)
    {
        if (!redisRtWriter_) return;

        auto& s = scratchSnap_;
        s.uid = uid;
        s.x = p.pos().x;
        s.z = p.pos().y;

        const auto& st = p.player_stat();
        s.hp = st.hp;
        s.sp = st.sp;

        s.inv_json.clear(); // 인벤 미사용
        redisRtWriter_(s);
    }

    void FieldWorker::on_client_move_input(const field::FieldCmd& cmd, net::Session::Ptr session)
    {
        if (!session) return;
        if (session->player_id() != cmd.entityId()) return;
        if (cmd.type() != field::FieldCmdType::FieldCmdType_Move) return;

        auto it = players_.find(cmd.entityId());
        if (it == players_.end()) return;

        auto* dir = cmd.dir();
        if (!dir) return;

        float dx = dir->x();
        float dy = dir->y();
        const float len2 = dx * dx + dy * dy;

        auto& mv = it->second->move_state();
        mv.lastInputTime = worldTime_;

        if (len2 < 1e-4f) {
            if (!mv.moving) return;

            mv.moving = false;
            mv.dir = { 0.f, 0.f };
            mv.speed = 0.f;

            env_.broadcastPlayerState(cmd.entityId(), monster_ecs::PlayerState::Idle);
            return;
        }

        const float invLen = 1.0f / std::sqrt(len2);
        dx *= invLen;
        dy *= invLen;

        const bool wasMoving = mv.moving;
        mv.moving = true;
        mv.dir = { dx, dy };
        mv.speed = 4.5f;

        if (!wasMoving) {
            env_.broadcastPlayerState(cmd.entityId(), monster_ecs::PlayerState::Chase);
        }
    }

    void FieldWorker::add_player(Player::Ptr player)
    {
        if (!player) return;

        const uint64_t pid = player->id();
        players_[pid] = player;

        on_player_enter_field(player);
    }

    void FieldWorker::remove_player(std::uint64_t playerId)
    {
        if (aoiSystem_) {
            aoiSystem_->remove_entity(playerId);
        }
        players_.erase(playerId);
    }

    void FieldWorker::update_world(float dt)
    {
        if (dt <= 0.0f) return;

        worldTime_ += dt;

        int playerLoops = 0;
        playerAcc_ += dt;
        while (playerAcc_ >= PlayerStep && playerLoops < 5) {
            tick_players(PlayerStep);
            playerAcc_ -= PlayerStep;
            ++playerLoops;
        }

        int monsterLoops = 0;
        monsterAcc_ += dt;
        while (monsterAcc_ >= MonsterStep && monsterLoops < 3) {
            tick_monsters(MonsterStep);
            monsterAcc_ -= MonsterStep;
            ++monsterLoops;
        }
    }

    bool FieldWorker::is_walkable(const Vec2& from, const Vec2& to) const
    {
        (void)from;
        (void)to;
        return true;
    }

    void FieldWorker::send_combat_event(field::EntityType attackerType, uint64_t attackerId,
        field::EntityType targetType, uint64_t targetId, int damage, int remainHp)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(targetId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        auto evOffset = field::CreateCombatEvent(
            fbb,
            attackerType,
            attackerId,
            targetType,
            targetId,
            damage,
            remainHp
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_CombatEvent,
            evOffset.Union()
        );

        fbb.Finish(envOffset);

        sess->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
    }

    void FieldWorker::send_stat_event(std::uint64_t watcherId, std::uint64_t subjectId, bool isMonster,
        int hp, int maxHp, int sp, int maxSp)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        field::EntityType et = isMonster
            ? field::EntityType::EntityType_Monster
            : field::EntityType::EntityType_Player;

        auto statOffset = field::CreateStatEvent(
            fbb,
            et,
            subjectId,
            hp,
            maxHp,
            sp,
            maxSp
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_StatEvent,
            statOffset.Union()
        );

        fbb.Finish(envOffset);

        sess->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
    }

    void FieldWorker::monster_spawn_in_aoi(std::uint64_t monsterId, float x, float y)
    {
        if (aoiSystem_) {
            aoiSystem_->add_entity(monsterId, false, x, y);
        }
    }

    void FieldWorker::monster_remove_from_aoi(std::uint64_t monsterId)
    {
        if (aoiSystem_) {
            aoiSystem_->remove_entity(monsterId);
        }
    }

    void FieldWorker::handle_skill(const NetMessage& msg)
    {
        auto session = msg.session;
        if (!session) return;

        const uint64_t pid = session->player_id();

        flatbuffers::Verifier verifier(
            reinterpret_cast<const uint8_t*>(msg.payload.data()),
            msg.payload.size()
        );

        if (!verifier.VerifyBuffer<game::SkillCmd>(nullptr)) {
            std::cout << "[WARN] Invalid SkillCmd\n";
            return;
        }

        auto skill = flatbuffers::GetRoot<game::SkillCmd>(msg.payload.data());
        if (!skill) {
            std::cout << "[WARN] SkillCmd null\n";
            return;
        }

        const auto skillType = skill->skill();
        const uint64_t targetId = skill->targetId();

        env_.broadcastPlayerState(pid, monster_ecs::PlayerState::Attack);

        const bool dead = monsterWorld_.player_attack_monster(pid, targetId, skillType, env_);
        if (dead) {
            env_.broadcastAiState(targetId, monster_ecs::CAI::State::Dead);
        }
    }

    std::string FieldWorker::get_prefab_name(uint64_t id, bool isMonster)
    {
        if (isMonster) {
            if (monsterWorld_.prefabNameComp.has(id))
                return monsterWorld_.prefabNameComp.get(id).name;
            return "";
        }

        auto pit = players_.find(id);
        if (pit != players_.end())
            return "Paladin";

        return "";
    }

    void FieldWorker::send_field_enter(std::uint64_t watcherId, std::uint64_t subjectId, bool isMonster, const Vec2& pos)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        auto posOffset = field::CreateVec2(fbb, pos.x, pos.y);

        field::EntityType et = isMonster
            ? field::EntityType::EntityType_Monster
            : field::EntityType::EntityType_Player;

        std::string prefabName = get_prefab_name(subjectId, isMonster);
        if (prefabName.empty()) prefabName = "Default";
        auto prefabStr = fbb.CreateString(prefabName);

        auto cmd = field::CreateFieldCmd(
            fbb,
            field::FieldCmdType::FieldCmdType_Enter,
            et,
            subjectId,
            posOffset,
            0,
            prefabStr
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_FieldCmd,
            cmd.Union()
        );

        fbb.Finish(envOffset);

        sess->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
    }

    void FieldWorker::on_player_enter_field(Player::Ptr player)
    {
        if (!player) return;

        const uint64_t pid = player->id();
        const Vec2 p = player->pos();

        if (aoiSystem_) {
            aoiSystem_->add_entity(pid, true, p.x, p.y);
        }

        send_field_enter(pid, pid, false, p);

        for (auto& [otherId, other] : players_) {
            if (otherId == pid || !other) continue;
            send_field_enter(pid, otherId, false, other->pos());
        }

        for (auto mId : monsterWorld_.monsters) {
            auto& tr = monsterWorld_.transform.get(mId);
            send_field_enter(pid, mId, true, Vec2{ tr.x, tr.y });
        }

        for (auto& [otherId, other] : players_) {
            if (otherId == pid || !other) continue;
            send_field_enter(otherId, pid, false, p);
        }
    }

    void FieldWorker::init_monster_env()
    {
        env_.findClosestPlayer = [this](float x, float y, float maxDist) -> uint64_t {
            uint64_t closestId = 0;
            float closestDistSq = maxDist * maxDist;

            for (auto& [pid, player] : players_) {
                if (!player) continue;
                Vec2 pos = player->pos();

                const float dx = pos.x - x;
                const float dy = pos.y - y;
                const float distSq = dx * dx + dy * dy;

                if (distSq < closestDistSq) {
                    closestDistSq = distSq;
                    closestId = pid;
                }
            }
            return closestId;
            };

        env_.getPlayerPosition = [this](uint64_t pid, float& outX, float& outY) -> bool {
            auto it = players_.find(pid);
            if (it == players_.end() || !it->second) return false;

            outX = it->second->pos().x;
            outY = it->second->pos().y;
            return true;
            };

        env_.moveInAoi = [this](uint64_t mid, float x, float y) {
            if (aoiSystem_) aoiSystem_->move_entity(mid, x, y);
            };

        env_.spawnInAoi = [this](uint64_t mid, float x, float y) {
            if (!aoiSystem_) return;
            aoiSystem_->remove_entity(mid);
            aoiSystem_->add_entity(mid, false, x, y);
            };

        env_.removeFromAoi = [this](uint64_t mid) {
            if (aoiSystem_) aoiSystem_->remove_entity(mid);
            };

        env_.broadcastAiState = [this](uint64_t monsterId, monster_ecs::CAI::State newState) {
            broadcast_monster_ai_state(monsterId, newState);
            };

        env_.broadcastPlayerState = [this](uint64_t playerId, monster_ecs::PlayerState st) {
            broadcast_player_ai_state(playerId, st);
            };

        env_.broadcastCombat = [this](uint64_t monsterId, uint64_t playerId, int damage, int /*dummy*/) {
            auto it = players_.find(playerId);
            if (it == players_.end() || !it->second) return;

            auto& st = it->second->player_stat();

            send_combat_event(
                field::EntityType::EntityType_Monster, monsterId,
                field::EntityType::EntityType_Player, playerId,
                damage,
                st.hp
            );
            };

        env_.broadcastMonsterStat = [this](uint64_t monsterId, int hp, int maxHp, int sp, int maxSp) {
            broadcast_monster_stat(monsterId, hp, maxHp, sp, maxSp);
            };

        env_.broadcastPlayerStat = [this](uint64_t playerId, int hp, int maxHp, int sp, int maxSp) {
            broadcast_player_stat(playerId, hp, maxHp, sp, maxSp);
            };

        env_.getPlayerStats = [this](uint64_t pid, int& hp, int& maxHp, int& sp, int& maxSp) {
            auto p = PlayerManager::instance().get_by_id(pid);
            if (!p) return false;

            hp = p->hp();
            maxHp = p->max_hp();
            sp = p->sp();
            maxSp = p->max_sp();
            return true;
            };

        env_.setPlayerStats = [this](uint64_t pid, int hp, int sp) {
            auto p = PlayerManager::instance().get_by_id(pid);
            if (!p) return;

            p->set_hp(hp);
            p->set_sp(sp);
            };

        env_.markPlayerDirty = [this](uint64_t playerId) {
            auto it = players_.find(playerId);
            if (it != players_.end() && it->second) {
                it->second->set_lastDirtyMarkTime(worldTime_);
            }
            dirtyHub_.mark_dirty(playerId);
            };
    }

    void FieldWorker::tick_players(float step)
    {
        for (auto& [pid, playerPtr] : players_) {
            if (!playerPtr) continue;

            Player& player = *playerPtr;
            auto& mv = player.move_state();

            if (mv.moving && (worldTime_ - mv.lastInputTime) > 0.5f) {
                mv.moving = false;
                mv.dir = { 0.f, 0.f };
                mv.speed = 0.f;

                env_.broadcastPlayerState(pid, monster_ecs::PlayerState::Idle);
                mark_dirty_state(player);
                continue;
            }

            if (!mv.moving) continue;

            const Vec2 oldPos = player.pos();
            Vec2 newPos = oldPos;
            newPos.x += mv.dir.x * mv.speed * step;
            newPos.y += mv.dir.y * mv.speed * step;

            if (!is_walkable(oldPos, newPos)) continue;

            player.set_pos(newPos.x, newPos.y);

            if (aoiSystem_) {
                aoiSystem_->move_entity(pid, newPos.x, newPos.y);
            }

            mark_dirty_pos_if_needed(player, oldPos, newPos);
        }
    }

    void FieldWorker::tick_monsters(float step)
    {
        monsterWorld_.update(step, env_);
    }

    void FieldWorker::SpawnMonstersEvenGrid(int fieldId)
    {
        if (fieldId != 1000) return;

        constexpr int kSpawnCount = 300;
        constexpr float kMinX = 0.f, kMaxX = 500.f;
        constexpr float kMinY = 0.f, kMaxY = 500.f;

        constexpr int cols = 10;
        constexpr int rows = 10;

        const float cellW = (kMaxX - kMinX) / cols;
        const float cellH = (kMaxY - kMinY) / rows;

        for (int i = 0; i < kSpawnCount; ++i) {
            const int r = i / cols;
            const int c = i % cols;

            float x = kMinX + (c + 0.5f) * cellW;
            float y = kMinY + (r + 0.5f) * cellH;

            x = clampf(x, kMinX, kMaxX);
            y = clampf(y, kMinY, kMaxY);

            const int typeIndex = i % static_cast<int>(kMonsterTemplates.size());
            const MonsterTemplate& tpl = kMonsterTemplates[typeIndex];

            int monsterType = 0;
            if (typeIndex < 3) monsterType = 1;      // Bow
            else if (typeIndex < 6) monsterType = 2; // DoubleSword
            else monsterType = 3;                    // MagicWand

            const uint64_t monsterId = MakeDatabaseID(1);

            monsterWorld_.create_monster(
                monsterId,
                x, y,
                tpl.name,
                monsterType,
                tpl.maxHp,
                tpl.hp,
                tpl.maxSp,
                tpl.sp,
                tpl.atk,
                tpl.def
            );

            if (aoiSystem_) {
                aoiSystem_->add_entity(monsterId, false, x, y);
            }
        }
    }

    void FieldWorker::broadcast_ai_state(uint64_t entityId, field::EntityType et, field::AiStateType fbState)
    {
        aoiSystem_->for_each_watcher(entityId, [&](uint64_t watcherId) {
            auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
            if (!sess) return;

            flatbuffers::FlatBufferBuilder fbb;

            auto evOffset = field::CreateAiStateEvent(
                fbb,
                et,
                entityId,
                fbState
            );

            auto envOffset = field::CreateEnvelope(
                fbb,
                field::Packet::Packet_AiStateEvent,
                evOffset.Union()
            );

            fbb.Finish(envOffset);

            sess->send_payload(
                fbb.GetBufferPointer(),
                static_cast<std::uint32_t>(fbb.GetSize())
            );
            });
    }

    void FieldWorker::broadcast_monster_ai_state(uint64_t monsterId, monster_ecs::CAI::State newState)
    {
        broadcast_ai_state(
            monsterId,
            field::EntityType::EntityType_Monster,
            to_fb_state(newState)
        );
    }

    void FieldWorker::broadcast_player_ai_state(uint64_t playerId, monster_ecs::PlayerState st)
    {
        broadcast_ai_state(
            playerId,
            field::EntityType::EntityType_Player,
            to_fb_state(st)
        );
    }

    void FieldWorker::broadcast_stat_event(uint64_t entityId, field::EntityType et, int hp, int maxHp, int sp, int maxSp)
    {
        aoiSystem_->for_each_watcher(entityId, [&](uint64_t watcherId) {
            auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
            if (!sess) return;

            flatbuffers::FlatBufferBuilder fbb;

            auto evOffset = field::CreateStatEvent(
                fbb,
                et,
                entityId,
                hp,
                maxHp,
                sp,
                maxSp
            );

            auto envOffset = field::CreateEnvelope(
                fbb,
                field::Packet::Packet_StatEvent,
                evOffset.Union()
            );

            fbb.Finish(envOffset);

            sess->send_payload(
                fbb.GetBufferPointer(),
                static_cast<std::uint32_t>(fbb.GetSize())
            );
            });
    }

    void FieldWorker::broadcast_monster_stat(uint64_t monsterId, int hp, int maxHp, int sp, int maxSp)
    {
        broadcast_stat_event(
            monsterId,
            field::EntityType::EntityType_Monster,
            hp, maxHp,
            sp, maxSp
        );
    }

    void FieldWorker::broadcast_player_stat(uint64_t playerId, int hp, int maxHp, int sp, int maxSp)
    {
        broadcast_stat_event(
            playerId,
            field::EntityType::EntityType_Player,
            hp, maxHp,
            sp, maxSp
        );
    }

    void FieldWorker::mark_dirty_state(Player& player)
    {
        write_player_rt_enqueue(player.id(), player);
        dirtyHub_.mark_dirty(player.id());
        player.set_lastDirtyMarkTime(worldTime_);
    }

    void FieldWorker::mark_dirty_pos_if_needed(Player& player, const Vec2& oldPos, const Vec2& newPos)
    {
        const float dx = newPos.x - oldPos.x;
        const float dy = newPos.y - oldPos.y;
        const float distSq = dx * dx + dy * dy;

        const float threshSq = posDirtyDist_ * posDirtyDist_;
        if (distSq < threshSq) return;

        const double last = player.lastDirtyMarkTime();
        if ((worldTime_ - last) < dirtyMinInterval_) return;

        write_player_rt_enqueue(player.id(), player);
        dirtyHub_.mark_dirty(player.id());
        player.set_lastDirtyMarkTime(worldTime_);
    }

    // ---- helper functions (keep as-is; consider moving later if desired) ----

    std::shared_ptr<core::FieldWorker> GetFieldWorker(int fieldId)
    {
        return core::FieldManager::instance().get_field(fieldId);
    }

    bool SendToFieldWorker(int fieldId, core::NetMessage msg)
    {
        auto worker = core::FieldManager::instance().get_field(fieldId);
        if (!worker) return false;

        worker->push(std::move(msg));
        return true;
    }

    void send_move_to_fieldworker(std::uint64_t playerId, int fieldId, float x, float y)
    {
        flatbuffers::FlatBufferBuilder fbb;

        auto pos = field::CreateVec2(fbb, x, y);

        auto cmd = field::CreateFieldCmd(
            fbb,
            field::FieldCmdType::FieldCmdType_Move,
            field::EntityType::EntityType_Player,
            playerId,
            pos,
            0,
            fbb.CreateString("")
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_FieldCmd,
            cmd.Union()
        );

        fbb.Finish(envOffset);

        NetMessage msg;
        msg.type = MessageType::Custom;
        msg.session = nullptr;
        msg.payload.assign(
            fbb.GetBufferPointer(),
            fbb.GetBufferPointer() + fbb.GetSize()
        );

        SendToFieldWorker(fieldId, std::move(msg));
    }

} // namespace core
