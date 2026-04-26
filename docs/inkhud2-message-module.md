# InkHUD2 Message 모듈

채팅 표시 + 채널/DM 전환 + canned 송신 진입을 담당하는 레이아웃 모듈. upstream Meshtastic의 InkHUD에 없던 multi-channel + DM-per-peer 모델로 재설계됐다.

대상 파일: [Modules/MessageModule.h](../src/graphics/niche/InkHUD2/Modules/MessageModule.h), [Modules/MessageModule.cpp](../src/graphics/niche/InkHUD2/Modules/MessageModule.cpp)

## 데이터 모델

### 채널 (configured at boot)

[Setup.cpp](../src/graphics/niche/InkHUD2/Setup.cpp)가 부팅 시 활성 채널 목록을 `messageModule->addChannel()`로 등록한다. DM은 채널이 아니므로 여기 안 들어간다.

```cpp
struct ChannelTab {
    uint8_t channelIndex;   // mesh 채널 index (0..7)
    bool hasUnread;         // 안 읽음 표시
    bool chatStyle;         // 항상 true 현재
    char name[16];          // 표시명 (channels.getName 으로 얻음)
};
std::vector<ChannelTab> channels;
std::vector<std::deque<ChannelMessage>> channelMessages;  // index 정렬 일치
```

채널당 deque 최대 `MAX_MESSAGES_PER_CHANNEL = 10`. push_front + 초과 시 pop_back.

### DM thread (peer별 동적 생성)

DM은 peer마다 별도 thread를 가진다. 새로운 peer로부터 메시지가 오거나 새 peer로 송신할 때 자동 생성됨.

```cpp
struct DMMessage {
    uint32_t from;          // 발신자: 받은 메시지면 peer, 보낸 메시지면 우리 nodeNum
    uint32_t timestamp;
    char text[237];
};
struct DMThread {
    uint32_t nodeNum;       // peer의 nodeNum (thread 키)
    bool hasUnread;
    std::deque<DMMessage> messages;   // newest at front
};
std::vector<DMThread> dmThreads;
```

thread 당 deque 최대 `MAX_MESSAGES_PER_DM = 20` (채널보다 많음 — DM은 보통 더 의미 있는 대화가 오래 가니까).

### 현재 보고 있는 view

```cpp
enum class ViewKind { CHANNEL, DM_THREAD };
ViewKind currentView;
size_t currentChannelIndex;     // CHANNEL일 때 의미
uint32_t currentDMNodeNum;      // DM_THREAD일 때 의미; 0 = 빈 placeholder
```

부팅 직후엔 첫 channel을 자동 선택. `addChannel()`이 첫 호출일 때 `currentView = CHANNEL, currentChannelIndex = 0`을 세팅한다.

## 상태머신

```
                       ┌────────┐
                       │  MAIN  │
                       └────────┘
                        ▲  ▲  ▲
            DOUBLE_TAP  │  │  │ SHORT_PRESS
              ↓         │  │  │   ↓
          ┌────────┐    │  │  │ ┌────────────┐
          │  MENU  │── back──┘  │ QUICK_MENU │
          └────────┘    ▲       └────────────┘
              │         │            │
       activate "DM"    │      activate "Send Canned"
              ↓         │            ↓
          ┌────────┐    │      ┌──────────────┐
          │ DM_LIST│── back ──→│  CANNED_LIST │
          └────────┘           └──────────────┘
                                  │      │
                              back↑      ↓ send
                                          │
                                          ▼
                                        MAIN (autoshow)
```

| 상태 | handlesInput | 진입 경로 | 화면 |
|---|---|---|---|
| MAIN | false | 기본 / 모든 sub-state 의 종착지 | StatusBar(채널 또는 "DM: short_name") + ChatView |
| MENU | true | MAIN의 DOUBLE_TAP | StatusBar("Channels") + Back / DM / 채널 |
| DM_LIST | true | MENU의 "DM" 활성화 | StatusBar("DM List") + Back / peer 목록 |
| QUICK_MENU | true | MAIN의 SHORT_PRESS | StatusBar("Quick") + Back / Send Canned / Share Position |
| CANNED_LIST | true | QUICK_MENU의 "Send Canned" 활성화 | StatusBar("Send -> #채널" 또는 "Send -> @peer") + 메시지 |

`handlesInput = true`일 때는 LONG_PRESS도 모듈이 처리(활성화)하므로 HUD 레벨 cycle이 일어나지 않는다.

**Back 의 트레일** — 각 sub-state의 "< Back"은 한 단계 위로:
- MENU → MAIN
- DM_LIST → MENU
- QUICK_MENU → MAIN
- CANNED_LIST → QUICK_MENU

**Send 의 트레일** — 작업 완료 후 MAIN 으로 직행 (중간 메뉴 거치지 않음):
- CANNED_LIST의 메시지 활성화 → 송신 → 즉시 MAIN (autoshow가 같은 thread 표시)
- QUICK_MENU의 "Share Position" 활성화 → 송신 → 즉시 MAIN + alert "Position sent."

## 메뉴 진입/이탈 lifecycle

각 sub-state마다 진입 함수가 있다. 매 진입 시 항목을 다시 빌드해서 동적 변경(채널 추가/제거, DM peer 등장, canned config 변경, 컨텍스트별 항목 표시 여부 등)을 반영.

```cpp
void enterMenu();        // MAIN → MENU
void exitMenu();         // MENU → MAIN

void enterDMList();      // MENU → DM_LIST (MENU 의 "DM" 항목 활성화)
void exitDMListToMenu(); // DM_LIST → MENU (Back 항목)

void enterQuickMenu();   // MAIN → QUICK_MENU (SHORT_PRESS)
void exitQuickMenu();    // QUICK_MENU → MAIN (Back 항목)

void enterCannedList();  // QUICK_MENU → CANNED_LIST ("Send Canned" 활성화)
void exitCannedList();   // CANNED_LIST → MAIN (송신 성공 후)
                         // Back 항목은 enterQuickMenu() 호출해서 한 단계 위로 감
```

**Send 와 Back의 다른 종착지**:
- CANNED_LIST 메시지 활성화 → `sendCanned()` + `exitCannedList()` → MAIN (autoshow)
- CANNED_LIST의 Back 활성화 → `enterQuickMenu()` → QUICK_MENU (한 단계 위)

DM_LIST의 활성화는 `exitMenu()`를 호출해서 곧장 MAIN으로 돌아간다(channel 활성화와 동일). 즉 메뉴 → DM 항목 → DM_LIST → peer 항목 → MAIN(그 thread) 한 번에 흐른다.

## MenuItem / MenuList 활용

upstream의 [UI/MenuList](../src/graphics/niche/InkHUD2/UI/MenuList.h)와 [MenuItem](../src/graphics/niche/InkHUD2/UI/MenuItem.h)을 그대로 쓴다. MenuItem은 union 멤버로 `action`(`std::function<void()>*`), `submenu`, `toggleValue` 등을 가진다.

본 모듈은 모든 sub-state에서 `MenuItemType::ACTION`만 사용한다. lambda를 `std::function<void()>`로 캡쳐해서 멤버 vector에 보관하고, MenuItem.action이 그 vector의 원소 주소를 가리키게 한다.

```cpp
std::vector<MenuItem> topMenuItems;
std::vector<std::string> topMenuLabels;     // label 문자열 storage
std::vector<std::function<void()>> topMenuActions;  // action storage
```

**중요 함정**: vector reallocate되면 포인터 무효화된다. 그래서 rebuild 함수들이 항상 reserve(total) 후 emplace_back해서 storage가 안정된 뒤에야 MenuItem.label / .action 포인터를 채운다.

DM_LIST와 CANNED_LIST도 동일 패턴으로 자체 vector 3쌍을 가진다.

## 헤더 / 빈 상태 표시

| 상황 | 헤더 | 본문 |
|---|---|---|
| CHANNEL view, 메시지 있음 | `<채널이름>` | ChatView |
| CHANNEL view, 메시지 없음 | `<채널이름>` | "No Messages" 가운데 |
| DM_THREAD, peer 있음, 메시지 있음 | `DM: <short_name>` | ChatView |
| DM_THREAD, peer == 0 또는 메시지 없음 | `DM` 또는 `DM: <peer>` | "No DM Yet" 가운데 |
| MENU | `Channels` | 메뉴 항목 |
| DM_LIST, peer 있음 | `DM List` | peer 항목들 |
| DM_LIST, peer 없음 | `DM List` | "No DM Yet" + Back만 |
| CANNED_LIST, 메시지 있음 | `Send -> #채널` 또는 `Send -> @peer` | canned 항목들 |
| CANNED_LIST, canned 비어있음 | (위와 동일) | "No canned messages" + Back만 |

DM peer의 short_name은 `getShortNameCallback`(NodeDB lookup)으로 매번 가져온다. peer 정보가 없으면 4자리 hex로 fallback.

## DM thread의 발신/수신 식별

DM 메시지는 `DMMessage::from`이 누구냐로 방향이 결정된다:
- `from == myNodeNum` → 우리가 보낸 것 (outgoing)
- `from != myNodeNum` → 상대가 보낸 것 (incoming)

ChatView가 from을 myNodeNum과 비교해서 본인 메시지(우측 정렬·다른 색 등)를 그린다. 즉 별도 isOutgoing 플래그를 우리가 채울 필요 없고 from만 정확히 채우면 된다.

송신 메시지는 [MeshService::sendToMesh](../src/mesh/MeshService.cpp)에 추가된 hook을 통해 자동으로 모듈에 도달한다. 자세한 건 [inkhud2-canned-integration.md](inkhud2-canned-integration.md)의 outgoing hook 절 참조.

## DM thread의 키 산출

`setMessage(from, to, text, channel, timestamp)`에서 channel == CHANNEL_DM(=255)이면 DM. peer 키는:

```cpp
uint32_t peer = (from == myNodeNum) ? to : from;
```

수신: `from`이 상대방, `to`가 우리. → peer = from
송신: `from`이 우리, `to`가 상대방. → peer = to

이렇게 양방향이 같은 thread에 들어간다.

## autoshow 정책

새 메시지가 setMessage로 들어올 때:
1. 알맞은 thread/channel에 push_front
2. 활성 view가 다른 곳이고 inbound이면 hasUnread = true
3. **사용자가 MAIN 상태일 때만** currentView를 그 thread/channel로 바꾸고 hasUnread 클리어
4. `requestUpdate()` + `requestAutoshow()` 호출 — 다른 레이아웃 보고 있어도 Message 모듈이 자동으로 표면으로 올라옴

`state == MAIN` 가드가 있어서 사용자가 메뉴 안에 있을 때는 view가 갑자기 바뀌지 않는다. 메뉴를 닫고 MAIN으로 돌아왔을 때야 새 view가 보임.

## 부팅 시 초기 view

`addChannel()` 호출 시 첫 번째라면 `currentView = CHANNEL, currentChannelIndex = 0`. DM이 안 와도 처음엔 첫 채널 화면이 뜸 — "No DM Yet"으로 시작하지 않게.

## 외부 의존

`Events.cpp`가 NodeDB lookup callback을 setup 시점에 등록:
- `setShortNameCallback(getShortNameFromDB)` — peer별 short_name (4자 헥스 fallback)
- `setLongNameCallback(getLongNameFromDB)` — peer별 long_name (없으면 nullptr)

ChatView가 발신자 표기에 short_name을 사용한다.

## Quick Menu — 송신 액션 허브

MAIN 화면에서 SHORT_PRESS를 누르면 QUICK_MENU로 진입한다. 컨텍스트 기반 액션 hub:

| 항목 | 조건 | 동작 |
|---|---|---|
| `< Back` | 항상 | MAIN 으로 돌아감 |
| `Send Canned` | 현재 view 에 송신 대상이 있을 때만 (CHANNEL 또는 DM_THREAD with peer) | CANNED_LIST 진입 |
| `Share Position` | 항상 | 현재 위치를 송신 + "Position sent." alert + MAIN 복귀 |

**Share Position의 destination 결정**:
- DM_THREAD (peer 있음) → `positionModule->sendOurPosition(peer, false, 0)` — peer로 직접 송신, PKI 자동
- CHANNEL view → `sendOurPosition(NODENUM_BROADCAST, false, channelIndex)` — 그 채널에 broadcast
- 그 외 (No DM Yet 등) → `sendOurPosition()` — 기본 broadcast (primary 채널)

Position 패킷은 `POSITION_APP` portnum이라 우리의 outgoing TEXT_MESSAGE_APP hook이 잡지 않는다. 즉 Message 화면에 자기 위치 메시지로 미러되지는 않음 — 의도된 동작 (위치는 채팅 항목이 아님). 송신 확인은 menuModule alert "Position sent." 로 사용자에게 즉각 피드백.

**확장 패턴**: QUICK_MENU의 `rebuildQuickMenuItems()`에 새 항목 추가는 단순 — 라벨/람다 한 쌍만 push_back. 미래 후보:
- "Ping" — 이미 시스템 메뉴에 있으나 Quick에 두면 더 자주 호출 가능
- "Send Bell" — canned에 bell 옵션 토글
- "Toggle Mute" — 채널별 alert
- "Save Backup" — Golden backup 트리거

## 메시지 송신 (canned)

`enterCannedList()` → 사용자가 항목 LONG_PRESS → lambda가 `sendCanned(text)` + `exitCannedList()` 호출.

`sendCanned`은:
1. `currentView`로 destination 결정 (broadcast vs DM)
2. `router->allocForSending()` + payload 채우기
3. PKI 가능 peer면 `pki_encrypted = true` 설정 (upstream CannedMessageModule와 동일 패턴)
4. `service->sendToMesh(p, RX_SRC_LOCAL, true)` — `ccToPhone=true`라 앱에도 미러됨

송신 후 `MeshService::sendToMesh` 안의 hook이 발화 → `notifyOutgoing` → `Events::onReceiveTextMessage` → `setMessage(우리nodeNum, 상대, ...)` → 자동 autoshow 다시 같은 thread MAIN으로. 사용자 입장에선 "send and bounce back"이 별도 코드 없이 동작한다.

자세한 수신/송신 데이터 흐름은 [inkhud2-canned-integration.md](inkhud2-canned-integration.md) 참조.

## 알려진 한계

- ChatView는 항상 threaded — 단건 표시(DMView 같은 단순 모드)는 더 이상 사용 안 함
- DM peer 목록에 "마지막 메시지 미리보기" 없음 (이름만 표시 + 안 읽음 `*` prefix)
- 메시지 단건 선택/답장/삭제 UI 없음
- 채널 메시지 송신은 canned 경로로만 가능 — 자유 텍스트 입력 UI 없음 (모바일 앱에서)
- 시간 표시는 `millis()` 기준 상대시간 (`5m`, `2h`) — 절대시간 표시 없음
