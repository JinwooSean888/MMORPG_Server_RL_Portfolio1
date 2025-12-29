#include "game_field_logic.h"

#include "core/proto/protocol_verify.h"
#include "net/session.h"
#include "game/PlayerManager.h"
#include "field/FieldManager.h"

#include "proto/generated/game_generated.h"
#include "proto/generated/field_generated.h"
#include "worker/FieldWorker.h" 
#include "worker/worker.h"   
namespace logic {
    static void SendSkillCmdErrorAck(net::Session* session,
        const game::SkillCmd* sk,
        game::SkillError err)
    {
        if (!session || !sk) return;

        flatbuffers::FlatBufferBuilder fbb(128);

        auto ackOffset = game::CreateSkillCmdAck(
            fbb,
            sk->skill(),
            sk->targetId(),
            false,      // ok = false
            err
        );

        auto envOffset = game::CreateEnvelope(
            fbb,
            game::Packet::Packet_SkillCmdAck,
            ackOffset.Union()
        );

        fbb.Finish(envOffset);

        session->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
    }


    void OnRecv_EnterField(net::Session* session, const game::Envelope& env)
    {
        if (!session) return;

        auto [err, req, player] =
            VERIFY_FBS_REQ_BY_USER(EnterField, env, session);
        if (err != proto::Error::Ok || !player) {
            return;
        }

        uint32_t fieldId = req->field_id();

        // 1) Player 기본 세팅
        player->set_field_id(fieldId);
        player->set_pos(102.1f, 155.91f);
        //player->set_prefab_name("Paladin");

        //  FieldWorker 준비만 해둔다 (아직 add_player 안함)
        auto& fm = core::FieldManager::instance();
        auto fwBase = fm.create_field(fieldId);   // 생성 or 재사용
        if (!fwBase) return;

        auto fw = std::dynamic_pointer_cast<core::FieldWorker>(fwBase);
        if (!fw) return;

        // 3세션 상태를 필드로 전환
        session->set_field_id(static_cast<int>(fieldId));
        session->set_state(net::SessionState::InField);

        // 4먼저 클라에 EnterFieldAck 전송
        {
            flatbuffers::FlatBufferBuilder fbb;

            auto enterAck = game::CreateEnterFieldAck(
                fbb,
                fieldId,
                player->id()
            );

            auto envAck = game::CreateEnvelope(
                fbb,
                game::Packet::Packet_EnterFieldAck,
                enterAck.Union()
            );

            fbb.Finish(envAck);

            session->send_payload(
                fbb.GetBufferPointer(),
                static_cast<std::uint32_t>(fbb.GetSize())
            );
        }

        //이제야 FieldWorker 에 플레이어 등록
        // 여기서 AOI Snapshot / Enter 이벤트가 발생하고
        // FieldCmd(Enter/Move)들이 클라로 날아감.
        fw->add_player(player);
    }

    void OnRecv_SkillCmd(net::Session* session, const game::Envelope& env)
    {
        if (!session) return;

        auto* sk = env.pkt_as_SkillCmd();
        if (!sk) return;

        const auto playerId = session->player_id();
        const auto fieldId = session->field_id();

        // 아직 필드 안 들어간 상태라면 바로 에러 ACK
        if (fieldId == 0 || playerId == 0)
        {
            SendSkillCmdErrorAck(session, sk, game::SkillError::SkillError_InvalidState);
            return;
        }

        // 여기서부터는 "필드 워커에 넘겨서 처리"
        core::NetMessage msg;
        msg.type = core::MessageType::SkillCmd;
        msg.session = session->shared_from_this();

        flatbuffers::FlatBufferBuilder fbb;
        auto skillOffset = game::CreateSkillCmd(
            fbb,
            sk->skill(),
            sk->targetId()
        );
        fbb.Finish(skillOffset);

        msg.payload.assign(
            fbb.GetBufferPointer(),
            fbb.GetBufferPointer() + fbb.GetSize()
        );

        core::SendToFieldWorker(fieldId, std::move(msg));
    }




}
