#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "core/core_types.h"

namespace net {
    class Session; 
}

namespace core {

   
    enum class MessageType : uint8_t {
        NetEnvelope = 0,   // 네트워크 패킷(FlatBuffers Envelope)
        Custom = 1,   // 기타 용도
        EnterField = 2,   // 필드 입장
        LeaveField = 3,   // 필드 퇴장
        MoveField = 4,   // 필드 내 이동
		SkillCmd = 5,   // 스킬 커맨드
    };

  
    struct NetMessage {
        MessageType                      type{ MessageType::NetEnvelope };
        std::shared_ptr<net::Session>    session;   // 보낸 세션
        std::vector<uint8_t>             payload;   
    };

    class Worker {
    public:
        using Ptr = std::shared_ptr<Worker>;
        using Callback = std::function<void(const NetMessage&)>;

        explicit Worker(std::string name);
        virtual ~Worker();


        void start();
        void stop();

  
        INT32 GetMessageCount();


        void push(NetMessage msg);


        void set_on_message(Callback cb);

        const std::string& name() const { return name_; }

        std::thread::native_handle_type native_handle() {
            return thread_.native_handle();
        }

    private:
        void loop(); // 내부 스레드 루프

        std::string              name_;
        std::atomic<bool>        running_{ false };
        std::thread              thread_;

        std::mutex               mutex_;
        std::condition_variable  cv_;
        std::queue<NetMessage>   queue_;
        Callback                 on_message_;
    };
    inline constexpr char GAME_WORKER_NAME[] = "GameWorker";

    Worker::Ptr  GetGameWorker();         
    bool         CreateGameWorker();      
    bool         SendToGameWorker(NetMessage msg);

} // namespace core
