# InkHUD2 ↔ canned 메시지 통합

InkHUD2 빌드에서 canned 메시지를 동작시키는 데 필요한 두 가지 inflection point:

1. **저장/admin 동기화** — upstream `CannedMessageModule`이 빌드에서 빠지므로 별도 store 필요
2. **송신 메시지 미러** — 우리가 보낸 canned 메시지를 우리 화면에도 즉시 표시

각각 비자명한 함정과 우리 fork의 해결을 정리한다.

---

## 1. inkhud 빌드의 `HAS_SCREEN=0` 함정

### 증상

CLI나 모바일 앱에서 canned 메시지를 set 했는데:
- `meshtastic --set-canned-message "..."` 가 즉시 종료됨에도 불구하고 펌웨어에 저장 안 됨
- `meshtastic --get-canned-message` 가 응답 없이 hang 후 timeout
- 디바이스 재부팅 후 InkHUD2 의 canned 화면에서 메시지가 안 보임
- `/prefs/cannedConf.proto` 파일 자체가 만들어지지 않음

### 원인

매크로 연쇄:
```
inkhud 빌드 플래그 (PlatformioConfig.ini)
  ↓
-D MESHTASTIC_EXCLUDE_SCREEN
  ↓
configuration.h:526-528
  ↓
#undef HAS_SCREEN
#define HAS_SCREEN 0
  ↓
src/modules/CannedMessageModule.cpp:5
  ↓
#if HAS_SCREEN ← false
  ↓
파일 본문 전체가 컴파일에서 제외
```

제외되는 것들:
- 글로벌 변수 `cannedMessageModule` (포인터 정의 자체)
- 글로벌 변수 `cannedMessageModuleConfig` (저장 protobuf)
- `loadProtoForModule`, `saveProtoForModule`, `installDefaultCannedMessageModuleConfig`
- `handleAdminMessageForModule`, `handleSetCannedMessageModuleMessages`, `handleGetCannedMessageModuleMessages`
- `splitConfiguredMessages`
- 그리고 OLED 렌더링 등 모든 멤버 함수

`Modules.cpp:176-180`의 인스턴스 생성도 같은 가드 안:
```cpp
#if HAS_SCREEN && !MESHTASTIC_EXCLUDE_CANNEDMESSAGES
    if (config.display.displaymode != ...DisplayMode_COLOR) {
        cannedMessageModule = new CannedMessageModule();
    }
#endif
```

→ **핸들러 등록 자체가 안 됨**. `MeshModule::handleAdminMessageForAllModules`가 모듈 리스트를 순회해도 canned admin 태그를 처리할 모듈이 없어서 silently 무시됨. CLI/앱이 응답을 기다리다 timeout.

### Why upstream 그렇게 했나

upstream Meshtastic의 InkHUD는 **단독 송신 기능 자체가 없다**. 채팅은 모바일 앱에서만 한다는 전제. 그래서 canned 모듈도 OLED 디스플레이 의존이 강하게 들어가있고, OLED를 꺼버리면 통째로 빠지는 게 자연스러웠음.

본 fork는 InkHUD2 + canned 송신을 동시에 살리려 하는 첫 시도라 이 전제와 충돌한다.

### 해결: niche graphics 의 별도 store 활용

niche graphics 디렉토리에 이미 [CannedMessageStore](../src/graphics/niche/Utils/CannedMessageStore.h)가 있다. 누가 미리 만들어둔 듯:

- `MESHTASTIC_INCLUDE_NICHE_GRAPHICS` 가드 (우리 빌드에서 컴파일됨)
- 같은 `/prefs/cannedConf.proto` 파일을 load/save (포맷 호환)
- AdminModule observer로 등록되어 set/get 자동 처리
- 메시지를 `std::vector<std::string>`으로 파싱해서 RAM 캐시
- singleton 패턴 (`getInstance()`)

InkHUD V1의 [MenuApplet.cpp:57](../src/graphics/niche/InkHUD/Applets/System/Menu/MenuApplet.cpp#L57)이 이미 `getInstance()`를 호출해서 V1 빌드에선 자동 활성화. 우리 InkHUD2는 호출 안 했었기에 singleton이 만들어지지 않아 admin observer도 등록 안 됐었음.

### 본 fork 의 wiring

**[InkHUD2/Setup.cpp](../src/graphics/niche/InkHUD2/Setup.cpp)** — 채널 설정 직후 한 줄 호출:
```cpp
NicheGraphics::CannedMessageStore::getInstance();
```
이 한 번의 getInstance가:
- singleton 생성자 호출 → flash 에서 load → admin observer 등록
- 이후 모든 set_canned/get_canned admin 메시지를 자동 처리

**[InkHUD2/Modules/MessageModule.cpp](../src/graphics/niche/InkHUD2/Modules/MessageModule.cpp)** — `rebuildCannedListItems()`가 backend 분기:
```cpp
#if HAS_SCREEN
    // upstream의 글로벌을 split
    const char* raw = cannedMessageModuleConfig.messages;
    ...split on '|'...
#elif defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)
    // store API 사용
    auto* store = NicheGraphics::CannedMessageStore::getInstance();
    if (store) {
        for (uint8_t i = 0; i < store->size(); ++i) {
            messages.emplace_back(store->at(i));
        }
    }
#endif
```

upstream 코드는 한 줄도 건드리지 않았다 — 우리 InkHUD2 측에서 store를 활용하는 방향.

### 검증 방법

```bash
py -m meshtastic --set-canned-message "안녕|가는중|도착|네|OK"
py -m meshtastic --get-canned-message    # 정상이면 즉시 응답
```

응답이 오면 펌웨어 admin handler가 살아있다는 증거. 그 다음 디바이스 재부팅 후 InkHUD2 의 Message 화면에서 SHORT_PRESS → "Send -> #채널" 헤더 + 입력한 메시지 목록 확인.

### 향후 주의사항

- niche graphics 빌드에서는 `cannedMessageModuleConfig` 글로벌을 직접 참조하지 말 것. 그 글로벌이 정의 안 됨 — link 에러 또는 (운 나쁘면) zero-init된 다른 글로벌과 충돌
- upstream `CannedMessageModule.cpp`의 `#if HAS_SCREEN` 가드는 풀지 말 것 — 풀면 OLED 의존 코드가 같이 끌려와 niche graphics 빌드가 깨진다 (Screen, OLEDDisplay, MessageRenderer, NotificationRenderer 등)
- 다른 모듈 (Position, Telemetry 등)도 비슷한 함정 가능성 있음. 모듈별 글로벌 protobuf 변수를 InkHUD2가 참조하려 할 때, 그 변수가 `#if HAS_SCREEN` 안에 정의돼있는지 먼저 확인

---

## 2. 송신 메시지를 화면에 미러하기 — `sendToMesh` hook

### 문제

InkHUD2에서 canned나 다른 자체 송신을 했을 때, **우리가 방금 보낸 메시지가 화면에 표시 안 됨**. 받은 메시지만 보이고 보낸 건 안 보임 → 사용자 입장에선 "보낸 게 맞나" 확인 불가.

### 왜 안 보이나 (in/out 메시지 경로 차이)

inbound 메시지 경로:
```
LoRa 또는 BLE
  ↓
Router → MeshModule::callPlugins
  ↓
TextMessageModule::handleReceived()
  ↓
notifyObservers(&mp)
  ↓
InkHUD2/Events.cpp:onReceiveTextMessage observer 콜백
  ↓
messageModule->setMessage(...)
```
잘 동작.

outbound 메시지 경로:
```
앱(BLE) 또는 펌웨어 자체
  ↓
MeshService::handleToRadio (앱 경로일 때만)
  ↓
MeshService::sendToMesh
  ↓
Router::sendLocal → 라디오 큐
```

`TextMessageModule::handleReceived`는 inbound 전용. 송신 경로엔 observable이 없어서 InkHUD2가 알아챌 방법이 없었다.

### 해결 방법 1차 시도 (handleToRadio hook) — 한계

처음에 [MeshService::handleToRadio](../src/mesh/MeshService.cpp)에 hook을 넣었다. 하지만 그건 **앱이 보낸 송신만** 잡는다. 펌웨어가 자체적으로 `service->sendToMesh()`를 직접 호출하는 경우(canned, position broadcast 등)는 `handleToRadio`를 거치지 않아 빠졌다.

### 해결 방법 2차 (현재) — `sendToMesh` hook

[MeshService::sendToMesh](../src/mesh/MeshService.cpp) 시작부에 hook 배치. 모든 outbound TEXT_MESSAGE_APP 패킷이 여기를 거치므로 빠짐없이 캐치.

**[src/modules/TextMessageModule.h](../src/modules/TextMessageModule.h)**: outbound 통지용 public wrapper 추가
```cpp
void notifyOutgoing(const meshtastic_MeshPacket *mp) { notifyObservers(mp); }
```

**[src/mesh/MeshService.cpp](../src/mesh/MeshService.cpp)** `sendToMesh()` 안:
```cpp
if (textMessageModule && p && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP &&
    p->decoded.payload.size > 0) {
    meshtastic_MeshPacket outgoing = *p;
    outgoing.from = nodeDB->getNodeNum();    // handleToRadio가 0으로 zero하기 때문
    textMessageModule->notifyOutgoing(&outgoing);
}
```

`outgoing.from` 을 우리 nodeNum 으로 채워서 observer 가 inbound 와 동일한 형태로 처리할 수 있게 한다.

**[src/graphics/niche/InkHUD2/Events.cpp](../src/graphics/niche/InkHUD2/Events.cpp)** `onReceiveTextMessage` 안:
- 기존의 `if (packet->from == nodeDB->getNodeNum()) return 0;` outgoing 가드 제거 (이제 outgoing 도 보고 싶음)
- DM 판별을 `to != NODENUM_BROADCAST` 로 변경 (수신/송신 모두 커버)
- `setMessage(packet->from, packet->to, ...)`로 to 도 같이 전달 (DM thread 키 산출에 필요)

### 결과 — 자동 "send and bounce back"

사용자가 canned 항목을 long press 하면:
1. `MessageModule::sendCanned(text)` → `service->sendToMesh(p, RX_SRC_LOCAL, true)`
2. sendToMesh의 hook이 발화 → `notifyOutgoing(&outgoing)`
3. observer 가 `Events::onReceiveTextMessage` 호출
4. Events 가 `messageModule->setMessage(우리, 대상, text, channel)` 호출
5. `setMessage` 가 적절한 thread/channel 에 push, autoshow 발화
6. 화면이 같은 thread/channel MAIN 으로 자동 복귀 + 새 메시지 추가됨

별도의 "송신 후 화면 전환" 코드 없이 자동으로 동작. 사용자 입장에선 send 누르면 → 메뉴 닫히면서 → 자기 메시지가 thread 맨 위에 떠있는 흐름.

### ccToPhone 인자

`service->sendToMesh(p, RX_SRC_LOCAL, true)`의 마지막 인자가 `ccToPhone=true`. 이게 있어야 앱에도 송신 메시지가 미러된다 (BLE를 통해 phone queue로 복사). 우리 sendCanned는 항상 true로 호출. upstream CannedMessageModule도 동일.

### PKI 처리

DM 송신 시 peer 가 32바이트 공개키를 가지면 PKI 암호화:
```cpp
if (dest != NODENUM_BROADCAST) {
    meshtastic_NodeInfoLite* node = nodeDB->getMeshNode(dest);
    if (node && node->num != nodeDB->getNodeNum() &&
        node->has_user && node->user.public_key.size == 32) {
        p->pki_encrypted = true;
        p->channel = 0;   // PKI는 채널 0 강제
    }
}
```
upstream의 `CannedMessageModule::sendText`와 동일 패턴 그대로 사용.

---

## 요약 — 각 파일의 역할

| 파일 | 역할 |
|---|---|
| [src/graphics/niche/Utils/CannedMessageStore.cpp](../src/graphics/niche/Utils/CannedMessageStore.cpp) | niche graphics 빌드용 canned 저장/admin 핸들러 (upstream에 이미 존재, 우리는 활용만) |
| [src/graphics/niche/InkHUD2/Setup.cpp](../src/graphics/niche/InkHUD2/Setup.cpp) | `CannedMessageStore::getInstance()` 한 번 호출해서 깨움 |
| [src/graphics/niche/InkHUD2/Modules/MessageModule.cpp](../src/graphics/niche/InkHUD2/Modules/MessageModule.cpp) | `rebuildCannedListItems()`가 store API로 메시지 가져옴, `sendCanned()`가 `service->sendToMesh` 호출 |
| [src/modules/TextMessageModule.h](../src/modules/TextMessageModule.h) | `notifyOutgoing()` public wrapper 추가 |
| [src/mesh/MeshService.cpp](../src/mesh/MeshService.cpp) | `sendToMesh()`에 outgoing TEXT_MESSAGE_APP hook 추가 |
| [src/graphics/niche/InkHUD2/Events.cpp](../src/graphics/niche/InkHUD2/Events.cpp) | outgoing 가드 제거, `to` 전달, DM 판별 양방향 대응 |

upstream 코드 변경은 두 곳뿐:
- `TextMessageModule.h` — public wrapper 한 줄 추가 (기존 동작 영향 없음)
- `MeshService.cpp::sendToMesh` — outgoing hook 약 10줄 추가 (기존 동작 영향 없음, textMessageModule이 nullptr이거나 textMessageModule이 컴파일 안 된 빌드에선 자동 우회)

나머지는 모두 InkHUD2 우리 코드 영역.
