\# MMORPG Server Core (C++)



C++ 기반 MMORPG 서버 코어 아키텍처 포트폴리오입니다.



본 프로젝트는 \*\*실제 접속·이동이 가능한 서버\*\*를 기반으로 하되,  

GitHub에는 \*\*서버 설계와 핵심 구조가 드러나는 코드만 공개\*\*합니다.



몬스터 AI는 \*\*ECS(Entity Component System) 기반\*\*으로 구현되었으며,  

오프라인 환경에서 \*\*PPO 강화학습 모델을 Grid 환경에서 학습\*\*시켜  

보다 합리적인 의사결정이 가능하도록 실험적으로 적용했습니다.



> 🔹 목적  

> 대규모 동시 접속 환경을 가정한  

> \*\*서버 아키텍처 설계 역량\*\*을 보여주기 위함  

> (네트워크, 워커 모델, AOI, 캐시/DB 분리 구조)



---



\## Demo



🎥 \*\*접속 및 이동 데모 영상\*\*  

(Unity 클라이언트 접속 → 이동 → AOI 기반 브로드캐스트 동작 확인)



실제 동작 여부는 영상으로 증명하며,  

이 저장소는 \*\*서버 코어 설계와 구현 방식\*\*에 집중합니다.



---



\## Architecture Overview



\[ Unity Client ]

|

TCP Socket

|

\[ net/session ]

|

\[ worker/codec ]

|

\[ FieldWorker ]

|

\[ Field + AOI ]

|

\[ Redis (RT Cache) ] ---> \[ DBWorker ] ---> \[ MySQL ]




\### Design Points

\- 네트워크 IO와 게임 로직 분리

\- Field 단위 워커 모델

\- AOI 기반 브로드캐스트 최소화

\- Redis 실시간 캐시 + 비동기 DB 영속화



---



\## Core Design Highlights



\### 1. Worker / Thread Model



\- Field 단위로 독립적인 `FieldWorker` 스레드 운용

\- 네트워크 스레드는 메시지를 워커 큐로 전달

\- 게임 로직은 워커 스레드에서만 처리하여 동기화 비용 최소화



\*\*관련 코드\*\*

worker/

├─ worker.h / worker.cpp

├─ workerManager.h / workerManager.cpp

└─ FieldWorker.h / FieldWorker.cpp



---



\### 2. AOI (Area of Interest) System



Grid / Sector 기반 AOI 관리 구조를 사용합니다.  

엔티티 이동 시 \*\*구독자 변경 이벤트만 브로드캐스트\*\*하여  

불필요한 네트워크 트래픽을 최소화합니다.



---



\## AOI 9-Sector Model



중앙 섹터를 기준으로 주변 3×3 섹터(총 9개)를  

관심 영역(AOI)으로 관리합니다.



+-----+-----+-----+

| S7 | S8 | S9 |

+-----+-----+-----+

| S4 | S5 | S6 | ← S5: Current Sector

+-----+-----+-----+

| S1 | S2 | S3 |

+-----+-----+-----+




\- AOI 범위: S5 + 인접 8개 섹터

\- Enter / Leave / Move 이벤트 분리 처리

\- AOI 범위 밖 엔티티는 비가시 처리



---



\### 3. Network \& Session Flow



\- TCP 기반 세션 관리

\- 패킷 디코딩 후 워커로 메시지 전달

\- 워커 내부에서 서버 권한 이동 및 상태 브로드캐스트



\*\*관련 코드\*\*

net/

├─ session.h / session.cpp

└─ tcp\_server.h / tcp\_server.cpp





---



\### 4. Storage Layer (Redis + DB)



\- Redis: 실시간 상태 캐시 (위치, 스탯 등)

\- DirtyHub: 변경된 엔티티만 추적

\- DBWorker: 비동기 큐 기반 DB write 처리



\*\*관련 코드\*\*

storage/

├─ DirtyHub.h / DirtyHub.cpp

└─ DBWorker.h / DBWorker.cpp




---



\## AI System (ECS-Based Monster AI)



몬스터 AI는 \*\*ECS(Entity Component System) 기반 구조\*\*로 설계되었습니다.



\### Why ECS for Server-Side AI

\- 수천 단위 몬스터 동시 업데이트

\- Tick 기반 고정 업데이트

\- 예측 가능한 CPU 사용량

\- 캐시 친화적인 메모리 접근



\### ECS Architecture Overview

\- Entity: 몬스터 ID

\- Components:

&nbsp; - Transform (position, direction)

&nbsp; - Stat (HP, SP, ATK, DEF)

&nbsp; - AI State (Idle / Chase / Attack / Dead)

\- Systems:

&nbsp; - AISystem

&nbsp; - MovementSystem

&nbsp; - CombatSystem



---



\## Reinforcement Learning (Monster AI Prototype)



\- 알고리즘: PPO

\- 프레임워크: PyTorch

\- 학습 환경: Grid 기반 시뮬레이션



강화학습은 \*\*서버 구조와의 결합 가능성 검증용 프로토타입\*\*이며,  

ECS 실행 구조 위에서 \*\*의사결정 모듈 교체 가능성\*\*을 실험했습니다.



> 강화학습 코드는 별도의 README를 참고하세요.



---



\## Folder Structure



net/ : 네트워크, 세션, 패킷 처리

worker/ : 워커 스레드, FieldWorker

field/ : 필드 로직, AOI 시스템

game/ : Player 도메인 모델 (부분 공개)

storage/ : Redis 캐시, DB 영속화

config/ : 서버 설정 (샘플만 제공)

docs/ : 설계 문서 및 다이어그램

rl/ : 강화학습 코드 (Python)






---



\## What Is Included / Excluded



\### Included

\- 서버 코어 아키텍처

\- 워커 스레드 모델

\- AOI 시스템

\- ECS 기반 몬스터 AI

\- RL 기반 의사결정 실험



\### Intentionally Excluded

\- 전체 게임 콘텐츠 로직

\- 실제 서비스 DB 스키마 및 쿼리

\- 보안/인증 관련 구현

\- 운영 환경 설정 값



---



\## Build \& Run (Simplified)



\- C++17

\- Windows / Visual Studio

\- Redis, MySQL (Docker)



설정 파일은 보안을 위해 샘플만 제공됩니다.



config/

└─ server\_config.sample.json



---



\## Notes



본 저장소는 \*\*게임 완성본이 아닌 서버 엔지니어링 포트폴리오\*\*를 목적으로 합니다.



---



\# MMORPG\_Server\_RL\_Portfolio

