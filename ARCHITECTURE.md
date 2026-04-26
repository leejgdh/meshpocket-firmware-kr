# Meshtastic 펌웨어 — 기능 분석 (한국어)

이 문서는 본 fork(MeshPocket + Heltec V3 한국화)의 향후 작업을 위해 펌웨어의 동작 방식을 정리한 것이다. **무엇이 어디서 어떻게 동작하는지** 의 기능 단위 분석이며, 폴더 구조 같은 정적 정보는 [KOREAN.md](KOREAN.md)와 [README.md](README.md)를 참고.

분석 대상: `src/` 836 파일 + `variants/heltec_mesh_pocket/` + `niche/InkHUD2/`

---

## 목차

- [A. 부팅 흐름](#a-부팅-흐름) — `setup()` 11단계
- [B. 스레딩 모델](#b-스레딩-모델) — OSThread 기반 협력적 멀티태스킹
- [C. mesh 패킷 데이터 흐름](#c-mesh-패킷-데이터-흐름) — 수신·송신 경로, 암호화 시점
- [D. 모듈 시스템](#d-모듈-시스템) — class hierarchy, 등록 방식, 핵심 모듈 5개
- [E. InkHUD2 내부 동작](#e-inkhud2-내부-동작) — 이벤트→화면, Module/SystemModule, 버튼 입력
- [F. PowerFSM 절전](#f-powerfsm-절전) — 4가지 절전 상태
- [G. 향후 작업 hook 포인트](#g-향후-작업-hook-포인트) — 어디를 손대면 무엇이 바뀌는지
- [H. iPhone 앱 상호작용](#h-iphone-앱-상호작용) — BLE GATT, ToRadio/FromRadio, PhoneAPI 상태 머신, AdminMessage
- [I. 시스템 동작 주기](#i-시스템-동작-주기) — OSThread 주기, broadcast 주기, 절전별 주기 변경
- [J. 하드웨어 연결 체크 및 컨트롤](#j-하드웨어-연결-체크-및-컨트롤) — I2C 자동 감지, 배터리, 라디오 health, 센서

---

## A. 부팅 흐름

`main.cpp setup()` (1199줄, [src/main.cpp:308](src/main.cpp#L308))은 거대한 단일 함수로 11단계 순차 init을 진행한다:

| 단계 | 라인 | 역할 |
|---|---|---|
| 1. 전원·LED·SPI·핀 | [312-457](src/main.cpp#L312) | 기본 하드웨어 wake-up, 배터리 체크 |
| 2. 파일시스템·I2C scan | [460-544](src/main.cpp#L460) | flash 마운트, I2C 디바이스 자동 감지 |
| 3. 주변기기 detect | [545-680](src/main.cpp#L545) | 화면, RTC, 키보드, 가속도계, RGB LED |
| 4. **NodeDB load** | [706](src/main.cpp#L706) | flash에서 노드·channel·config 복원 |
| 5. **Router 생성** | [713](src/main.cpp#L713) | ReliableRouter (FloodingRouter 상속) |
| 6. AccelerometerThread / AmbientLighting | [739-750](src/main.cpp#L739) | 센서 thread 시작 |
| 7. **Timezone setup** | [827-841](src/main.cpp#L827) | `setenv("TZ", config.device.tzdef)` + `tzset()` |
| 8. **MeshService + setupModules** | [885-896](src/main.cpp#L885) | 모든 모듈 self-register 트리거 |
| 9. **LoRa init** | [951](src/main.cpp#L951) | Radio 인터페이스를 Router에 attach |
| 10. WiFi/Eth/WebServer | [969-992](src/main.cpp#L969) | (옵션, ESP32만) |
| 11. **PowerFSM start** | [1009-1010](src/main.cpp#L1009) | 절전 상태 머신 + thread |

`loop()`은 거의 비어있고 `esp32Loop()` / `nrf52Loop()`만 호출. 실제 작업은 모두 OSThread 기반.

---

## B. 스레딩 모델

**협력적 멀티태스킹** — FreeRTOS preemptive 안 씀. 모든 작업을 `OSThread`에 등록하고 `ThreadController`가 순환 호출.

```cpp
class MyThread : public OSThread {
    int32_t runOnce() {
        // 일하고
        return 100;  // 다음 호출까지 ms (적응형)
    }
};
```

활성 thread:
- **PowerFSMThread** (100ms) — 전원 상태 전이
- **AccelerometerThread**, **AmbientLightingThread**, **AudioThread** (옵션)
- **InkHUD2** 자체도 OSThread (100ms 주기)
- **Router/MeshService**도 runOnce 패턴
- **NotifiedWorkerThread** — interrupt-driven (라디오 ISR이 깨움)

장점: FreeRTOS 의존성 최소, 디버깅 단순. 대신 한 thread가 무한 루프에 빠지면 전체가 멈춤 — 모든 runOnce는 빨리 return해야 함.

---

## C. mesh 패킷 데이터 흐름

### 수신 흐름

```
LoRa ISR
  → RadioLibInterface::handleReceiveInterrupt() : 헤더 파싱, 암호화 상태로 MeshPacket 생성
  → Router::enqueueReceivedMessage() : fromRadioQueue에 push
  → Router::runOnce() : queue 순회
  → FloodingRouter::shouldFilterReceived() : PacketHistory로 중복 검사
  → Router::handleReceived()
      → perhapsDecode() : 채널 PSK(AES-CCM) 또는 PKI(Curve25519)로 복호화
      → which_payload_variant: encrypted_tag → decoded_tag
  → 분기:
      ├─ 모듈 전달 : MeshModule::callModules() → portnum 매칭 모듈의 handleReceived()
      ├─ 앱 전달 : MeshService::sendToPhone() → toPhoneQueue
      └─ 재전파 : FloodingRouter::perhapsRebroadcast() (hop_limit--)
```

### 송신 흐름

```
앱 (PhoneAPI::handleToRadio) 또는 모듈 (MeshService::sendToMesh)
  → Router::sendLocal() : 목적지·채널·hop_limit 결정
  → Router::send()
      → 듀티 사이클 체크
      → perhapsEncode() : 암호화
      → MQTT publish (옵션)
  → RadioLibInterface::send() : txQueue + 랜덤 지연
  → onNotify(ISR_TX/DELAY) : CAD(채널 활동 감지) → startSend()
  → SX126xInterface 하드웨어 송신
  → ISR_TX → completeSending() → 수신 모드 복귀
```

### 핵심 객체

| 객체 | 역할 |
|---|---|
| **RadioLibInterface** | LoRa 라디오 하드웨어 추상화; ISR 처리, 송수신 타이밍, TX/RX 큐 관리 |
| **SX126xInterface** | MeshPocket이 사용; RadioLib 라이브러리로 SX1262 칩셋 제어 |
| **Router** | 패킷 라우팅 핵심 엔진; 암호화/복호화, 목적지 판정, 재전파 결정 |
| **FloodingRouter** | 플로우딩 알고리즘; 중복 필터, hop_limit 감소, 모든 노드에 재전파 |
| **NextHopRouter** | 이웃 정보 기반 라우팅; 최적 경로 계산 |
| **ReliableRouter** | 신뢰성 단일 홉 송신; ACK 요청/수신, 재시도 |
| **MeshService** | 상위 레이어 조정; 모바일 앱 API, 모듈 호출, toPhoneQueue 관리 |
| **MeshModule** | 기능 모듈 베이스; 텍스트 메시지, GPS, 텔레메트리 등 portnum별 처리 |
| **CryptoEngine** | 암호화/복호화; AES-CCM (채널), Curve25519 (PKI), 논스 생성 |
| **PacketHistory** | 중복 필터; 최근 본 패킷 ID 기록, `wasSeenRecently()`로 flood 방지 |

### 암호화 시점

- **복호화**: 수신 직후 `Router::handleReceived()` → `perhapsDecode()` ([Router.cpp:413](src/mesh/Router.cpp#L413)). 채널 PSK로 AES-CCM 시도, 실패 시 PKI Curve25519. 실패 시 패킷 폐기.
- **암호화**: 송신 직전 `Router::send()` → `perhapsEncode()` ([Router.cpp:171](src/mesh/Router.cpp#L171)). nonce는 `(fromNode, packetId)`에서 생성 ([CryptoEngine.h:93](src/mesh/CryptoEngine.h#L93)).

---

## D. 모듈 시스템

### Class hierarchy

```
MeshModule (base, 자동 self-register)
  ├─ SinglePortModule (단일 portnum 처리)
  └─ ProtobufModule<T> (자동 protobuf decode/encode)
       ├─ TextMessageModule
       ├─ PositionModule
       ├─ AdminModule
       ├─ NeighborInfoModule
       └─ Telemetry/* (Device, Environment, AirQuality, Power, Health)
```

### 등록 방식

- 각 모듈이 생성자에서 `modules` 정적 vector에 자동 push ([MeshModule.cpp:21-28](src/mesh/MeshModule.cpp#L21))
- [Modules.cpp:106-250](src/modules/Modules.cpp#L106)의 `setupModules()`에서 활성 모듈만 `new` 호출
- [main.cpp:896](src/main.cpp#L896)에서 단 한 번 호출

### 라우팅

- `MeshModule::callModules()` ([MeshModule.cpp:88-200](src/mesh/MeshModule.cpp#L88))가 모든 모듈을 순회하며 `wantPacket(packet)` 호출
- 매칭되면 `handleReceived()` 호출, `ProcessMessage::STOP` 반환 시 다음 모듈 처리 중단
- `isPromiscuous=true` 모듈은 모든 패킷 감시 (RoutingModule, NeighborInfoModule)

### 핵심 모듈 5개

| 모듈 | portnum | trigger | 동작 |
|---|---|---|---|
| **TextMessageModule** | 1 | 수신 | 메시지 저장 + observer 알림 + 화면 깨움 |
| **PositionModule** | 3 | 수신 + 주기적(거리/시간 임계) | NodeDB 갱신 + RTC 설정 + ATAK PLI |
| **AdminModule** | 6 | 수신 (로컬/PKI 서명) | 설정·채널·소유자 변경 + 응답 + 재부팅 |
| **RoutingModule** | 5 | promiscuous | ACK/NAK 전송, 라우터 메타 |
| **NeighborInfoModule** | 71 | promiscuous + 주기적 | SNR 기록, 이웃 정보 broadcast |

### 모듈 간 협력 — 텍스트 메시지 도착 시

1. **RoutingModule**이 promiscuous로 먼저 잡아 SNR 로깅
2. **TextMessageModule**이 portnum 1 매칭 → 메시지 저장 + observer 알림 (InkHUD2 화면 갱신 trigger)
3. **NeighborInfoModule**이 발신자 SNR 추가
4. 만약 `want_response`면 응답 모듈이 처리, 아무도 응답 안 하면 `RoutingModule::sendAckNak(NO_RESPONSE)`로 NAK ([MeshModule.cpp:182-199](src/mesh/MeshModule.cpp#L182))

---

## E. InkHUD2 내부 동작

### 이벤트 → 화면 흐름

```
Meshtastic 이벤트 발생
  → Events.cpp의 observer 콜백 (textMessageObserver, powerStatusObserver 등)
  → Module의 setMessage() / setLevel() / updateNode() 호출
  → Module.requestUpdate()
  → Pipe.updateNeeded = true
  → 다음 InkHUD2.runOnce() (100ms 주기)에서 render() 실행
  → Buffer.clear() → Pipe.render() (slot 모듈들 + system 오버레이)
  → DisplayDriver.update(fullRefresh)
```

구현 위치: [Events.cpp](src/graphics/niche/InkHUD2/Events.cpp), [InkHUD2.cpp:101-133](src/graphics/niche/InkHUD2/InkHUD2.cpp#L101)

### Module vs SystemModule

| | Module | SystemModule |
|---|---|---|
| 렌더 위치 | slot (1/2/4 분할) | 항상 오버레이 |
| `priority` | 없음 | 있음 (높을수록 위) |
| `lockRendering` | 없음 | true면 다른 모듈 렌더 차단 (메뉴/부트 화면) |
| `alwaysRender` | 없음 | true면 무조건 렌더 (배터리 아이콘) |
| 입력 | focus된 slot만 | priority 역순 (높은 게 먼저 캡처) |
| 예시 | Message, NodeList, Map | Battery, Boot, Menu |

### Setup.cpp wiring

[InkHUD2/Setup.cpp](src/graphics/niche/InkHUD2/Setup.cpp)에서 순차적으로 진행되는 init:

1. **디스플레이/폰트 init** ([Line 88-102](src/graphics/niche/InkHUD2/Setup.cpp#L88)) — EInkAdapter, InkHUD2 인스턴스
2. **모듈 생성** ([Line 108-113](src/graphics/niche/InkHUD2/Setup.cpp#L108)) — Battery/Boot/Menu/Message/NodeList/Map
3. **메뉴 구조 정의** ([Line 199-311](src/graphics/niche/InkHUD2/Setup.cpp#L199)) — Main(GPS, Ping, Alerts, Screen, System, Exit) + 서브메뉴
4. **모듈 등록** ([Line 327-333](src/graphics/niche/InkHUD2/Setup.cpp#L327)):
   - SystemModule: Battery, Boot, Menu
   - Module (slot): NodeList, Message, Map
5. **버튼/백라이트 wiring** ([Line 354-388](src/graphics/niche/InkHUD2/Setup.cpp#L354))
6. **Events 시작** ([Line 344-345](src/graphics/niche/InkHUD2/Setup.cpp#L344))

### 버튼 입력 경로

```
TwoButton 하드웨어
  → Setup.cpp 콜백 (shortpress=SELECT, longpress=BACK)
  → InkHUD2::onInput(Input)
  → Pipe.dispatchInputAndCheck()
      ├─ SystemModule.onInput() (priority 역순 — 메뉴가 열려있으면 MenuModule이 먼저 잡음)
      ├─ 없으면 focused slot Module의 onInput()
      └─ 둘 다 false면 cycleSlot() (다음 모듈로 넘기기)
  → Module.requestUpdate() → 화면 갱신 trigger
```

---

## F. PowerFSM 절전

4개 핵심 상태 ([src/PowerFSM.cpp:58-227](src/PowerFSM.cpp#L58)):

| 상태 | 설명 |
|---|---|
| **SDS** (Shallow Deep Sleep) | 최저 전력, ~30초 깊은 수면 (`doDeepSleep`) |
| **LS** (Light Sleep) | 30ms 주기 깜박임, 배터리 절감 |
| **ON** | 정상 (BLE/Screen/Radio 활성) |
| **POWER** | USB/AC 연결 시, 절전 무시 |

전이 트리거:
- `EVENT_PRESS` (버튼)
- `EVENT_WAKE_TIMER` (타이머)
- `EVENT_POWER_CONNECTED/DISCONNECTED` (전원)
- `EVENT_LOW_BATTERY` (배터리 부족)
- `EVENT_SHUTDOWN` (강제 종료)

---

## G. 향후 작업 hook 포인트

| 하고 싶은 것 | 어디를 손대나 | 난이도 |
|---|---|---|
| **메뉴 항목 추가/제거** | [InkHUD2/Setup.cpp:282-308](src/graphics/niche/InkHUD2/Setup.cpp#L282) `mainMenuItems` 배열 | 🟢 |
| **부트 화면 메시지** | [InkHUD2/Modules/BootModule.cpp](src/graphics/niche/InkHUD2/Modules/BootModule.cpp) | 🟢 |
| **새 메뉴 액션 추가** | Setup.cpp에 `actionXxx = [](){...}` lambda 추가 후 MenuItem 등록 | 🟢 |
| **StatusBar에 새 정보 표시** | [InkHUD2/UI/StatusBar.cpp](src/graphics/niche/InkHUD2/UI/StatusBar.cpp) | 🟡 |
| **새 InkHUD2 화면(Module) 추가** | `Module` 상속한 클래스 + `addModule()` | 🟡 |
| **새 시스템 오버레이** | `SystemModule` (priority + lockRendering) | 🟡 |
| **메시지 자동 처리/필터** | [TextMessageModule.cpp](src/modules/TextMessageModule.cpp)의 `handleReceivedProtobuf` 후크 | 🟡 |
| **자동 응답 모듈** | `MeshModule` 상속 + portnum 등록 ([ReplyBotModule](src/modules/ReplyBotModule.cpp) 참고) | 🟠 |
| **새 BLE 명령** | `meshtastic_AdminMessage` protobuf 확장 + AdminModule 핸들러 | 🟠 |
| **라디오 인터페이스 변경** | RadioInterface 상속 (큰 작업) | 🔴 |

### 본인 fork에서 가장 자주 만질 만한 3개 위치

1. **[InkHUD2/Setup.cpp](src/graphics/niche/InkHUD2/Setup.cpp)** — 메뉴, wiring, 버튼 동작. UI 변경의 중심
2. **[InkHUD2/Modules/](src/graphics/niche/InkHUD2/Modules/)** — 새 화면 추가 (Module 상속)
3. **[modules/](src/modules/)** — 새 mesh 기능 추가 (MeshModule 상속)

---

## H. iPhone 앱 상호작용

### BLE GATT 구조

Service UUID: `6ba1b218-15a8-461f-9fa8-5dcae273eafd` ([NRF52Bluetooth.cpp:11-15](src/platform/nrf52/NRF52Bluetooth.cpp#L11))

| Characteristic | 속성 | 방향 | 용도 |
|---|---|---|---|
| **FromNum** | NOTIFY+READ | 펌웨어→앱 | "새 데이터 있음" 알림 (32-bit 카운터) |
| **FromRadio** | READ (≤512B) | 펌웨어→앱 | 실제 데이터 (config, NodeDB, 패킷, 로그) |
| **ToRadio** | WRITE (≤512B) | 앱→펌웨어 | 명령·패킷·설정 변경 |
| **LogRadio** | NOTIFY+READ | 펌웨어→앱 | 디버그 로그 |

### 페어링 모드
- `NO_PIN`: 암호화 없이 오픈 (`SECMODE_OPEN`)
- `FIXED_PIN` / `RANDOM_PIN`: 암호화 + PIN (`SECMODE_ENC_NO_MITM`, MITM 방어 없음)
- iOS 호환: connection interval 15~100ms, slave latency 0
- **Hide PIN** 기능: `FIXED_PIN` 모드일 때만 화면에 PIN 안 표시

### ToRadio 메시지 (앱→펌웨어, 6종)
[mesh.pb.h:1269-1293](src/mesh/generated/meshtastic/mesh.pb.h#L1269)
- `want_config_id` — 연결 직후 dump 요청 (nonce 포함)
- `packet` — LoRa 패킷 송신
- `disconnect` — 연결 해제 알림
- `xmodemPacket` — 파일 전송
- `mqttClientProxyMessage` — MQTT 메시지 프록시
- `heartbeat` — 연결 유지

### FromRadio 메시지 (펌웨어→앱, 14종)
[mesh.pb.h:1211-1258](src/mesh/generated/meshtastic/mesh.pb.h#L1211)
- `my_info`, `node_info`, `metadata` — 노드/디바이스 정보
- `config`, `moduleConfig`, `channel`, `deviceuiConfig` — 설정 dump
- `config_complete_id` — 동기화 완료 신호 (받은 nonce 그대로 echo)
- `packet`, `log_record`, `queueStatus`, `clientNotification` — 정상 운영 데이터
- `fileInfo` — 파일 시스템 정보

### 첫 연결 시퀀스 — PhoneAPI 상태 머신

[PhoneAPI.h:38-51](src/mesh/PhoneAPI.h#L38), [PhoneAPI.cpp:224-524](src/mesh/PhoneAPI.cpp#L224)

```
1. 앱: ToRadio.want_config_id = <nonce>
   ↓
2. 펌웨어: 11단계 dump (각 단계마다 FromNum notify → 앱이 FromRadio read)
   STATE_SEND_MY_INFO         → 자신의 노드 정보
   STATE_SEND_UIDATA          → UI config
   STATE_SEND_OWN_NODEINFO    → 자기 상세
   STATE_SEND_METADATA        → 펌웨어 버전, hw model
   STATE_SEND_CHANNELS        → 8채널 모두
   STATE_SEND_CONFIG          → device/position/power/network/display/lora/bluetooth/security
   STATE_SEND_MODULECONFIG    → mqtt/serial/telemetry/canned 등 15종
   STATE_SEND_OTHER_NODEINFOS → 알고 있는 모든 다른 노드
   STATE_SEND_FILEMANIFEST    → 파일 목록
   STATE_SEND_COMPLETE_ID     → config_complete_id = <nonce>
   ↓
3. STATE_SEND_PACKETS — 정상 운영 (메시지·로그·큐 상태)
```

**특수 nonce** ([PhoneAPI.h:23-24](src/mesh/PhoneAPI.h#L23)):
- `69421` (`SPECIAL_NONCE_ONLY_NODES`) — 노드 정보만 요청
- `69420` (`SPECIAL_NONCE_ONLY_CONFIG`) — 설정만 요청

### AdminMessage 흐름 — Timezone 변경 예시

```
앱: ToRadio.packet {
      to: 0xFFFFFFFF (자신),
      portnum: ADMIN_APP (6),
      payload: AdminMessage.set_config { device.timezone_offset = ... }
    }
  ↓
PhoneAPI::handleToRadioPacket()
  → AdminModule::handleReceivedProtobuf()
  → handleSetConfig() → config.device 메모리에 적용
  → saveChanges(SEGMENT_CONFIG, requiresReboot)
  → 필요 시 Bluetooth 잠시 끊고 재부팅
  ↓
앱: AdminMessage_get_config_response 또는 ACK 수신
```

### 로컬(BLE) vs 원격(mesh) admin 권한 ([AdminModule.cpp:96-121](src/modules/AdminModule.cpp#L96))

| 조건 | 로컬 | 원격 |
|---|---|---|
| 접근 권한 | 페어링된 앱 | 3개 admin_key 중 하나로 PKI 서명 필수 |
| SessionKey | 선택 | 필수 (`checkPassKey()`) |
| 채널 | direct + admin | admin 채널만 (`config.security.admin_channel_enabled`) |

### 텍스트 메시지 송수신

**앱 → LoRa**:
```
ToRadio.packet { portnum=TEXT_MESSAGE_APP(1), payload="...", to=대상 }
  → PhoneAPI::handleToRadioPacket()
  → Router::enqueuePacket() (TX 큐)
  → RadioInterface가 LoRa 송신
```

**LoRa → 앱**:
```
LoRa 수신
  → Router::handleReceived() → 복호화
  → MeshService → toPhoneQueue
  → FromRadio.packet (rx_rssi/rx_snr/rx_time 메타 포함)
```

### 주의할 디테일

- **BLE MTU 512B 한도** — protobuf 메시지가 이걸 넘으면 안 됨 (생성 시 enforce)
- **`lastToRadio[512]` 중복 탐지** — BLE 재전송 같은 ToRadio 두 번 와도 한 번만 처리 ([NRF52Bluetooth.cpp:158-168](src/platform/nrf52/NRF52Bluetooth.cpp#L158))
- **`recentToRadioPacketIds[20]`** — 최근 20개 패킷 ID로 송신 중복 방지 ([PhoneAPI.h:59](src/mesh/PhoneAPI.h#L59))
- **Heartbeat ↔ QueueStatus** — 앱이 heartbeat 보내면 펌웨어가 free_slots 응답해서 송신 가능 여부 알림
- **StreamAPI vs BluetoothPhoneAPI** — Serial/TCP는 `0x94C3` 프레임 헤더+길이, BLE는 framing 없이 직접. 둘 다 `PhoneAPI` 상속 → 프로토콜 로직 재사용
- **Reboot 필요 여부** — LoRa RF 파라미터·역할 변경은 reboot 필수. Bluetooth 설정은 불필요 ([AdminModule.cpp:896-901](src/modules/AdminModule.cpp#L896))

---

## 참고

- 폴더 구조 분석은 [KOREAN.md](KOREAN.md) 참조
- InkHUD2 자체 설계 문서: [src/graphics/niche/InkHUD2/ARCHITECTURE.md](src/graphics/niche/InkHUD2/ARCHITECTURE.md)
- 본 문서는 시점별 스냅샷 — 코드가 바뀌면 줄 번호 등은 어긋날 수 있음. 최신은 항상 코드 자체.
