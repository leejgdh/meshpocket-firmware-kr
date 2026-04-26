# InkHUD2 버튼 UX

MeshPocket은 메인 버튼 한 개뿐인 디바이스다. 그 위에 모든 InkHUD2 입력이 매핑된다. upstream Meshtastic의 InkHUD나 다중 버튼 기기와 입력 모델이 다르므로 별도로 정리한다.

## 물리 입력 — 세 종류뿐

[TwoButton](../src/graphics/niche/Inputs/TwoButton.h) 라이브러리가 한 개 버튼에서 다음 세 가지 제스처를 분리해낸다:

| 제스처 | 검출 기준 | 본 fork 추가 여부 |
|---|---|---|
| 짧게 누름 (Short press) | 50 ~ 500 ms | upstream 기본 |
| 길게 누름 (Long press) | 500 ms 이상 | upstream 기본 |
| 더블탭 (Double tap) | 짧게 누름 → 300 ms 안에 또 누름 | **본 fork 추가** ([TwoButton.cpp 의 `setHandlerDoublePress`](../src/graphics/niche/Inputs/TwoButton.cpp)) |

더블탭은 라이브러리에 옵션 핸들러로 추가했다. `setHandlerDoublePress()`를 등록한 버튼에 한해서만 활성화되며, 활성화된 버튼은 short press가 300 ms 지연된다(두 번째 누름이 올지 봐야 하므로). 등록 안 된 버튼은 short press가 즉시 발화하는 종전 동작 유지.

## Input enum 의미적 ↔ 물리적 명명

[Pipe/Events.h](../src/graphics/niche/InkHUD2/Pipe/Events.h)의 `Input` enum은 본 fork에서 의미적 이름(`SELECT`, `BACK`)에서 물리적 이름(`SHORT_PRESS`, `LONG_PRESS`)으로 변경됐다.

| 이전 | 현재 | 이유 |
|---|---|---|
| `Input::SELECT` | `Input::SHORT_PRESS` | "Select"는 d-pad/조이스틱 디바이스의 OK 버튼 의미가 강해서, 단일 버튼 기기에서 모듈이 임의 의미를 부여할 때 헷갈림 |
| `Input::BACK` | `Input::LONG_PRESS` | 위와 동일 |
| `DOUBLE_TAP` | `DOUBLE_TAP` | 이미 물리적 이름이라 그대로 |
| `UP/DOWN/LEFT/RIGHT` | `UP/DOWN/LEFT/RIGHT` | d-pad용으로 그대로 두되, MeshPocket 버튼에선 절대 발화 안 함 |

모듈 코드에서 `case Input::SHORT_PRESS:` 형태로 받아서 자기 의미를 부여한다.

## 프로젝트 전반 UX 룰

모든 모듈 메인 화면에서 동일하게 적용:

| 입력 | 기본 의미 |
|---|---|
| **SHORT_PRESS** | 모듈 내부 액션 (스크롤, 항목 선택 이동, 또는 모듈별 단일 액션) |
| **LONG_PRESS** | **다음 모듈로 cycle** (HUD 레벨에서 자동 처리) |
| **DOUBLE_TAP** | **그 모듈의 메뉴/설정 진입** |

메뉴(또는 메뉴 같은 sub-state) 안에서는 다른 룰:

| 입력 | 메뉴 안 의미 |
|---|---|
| **SHORT_PRESS** | 다음 항목으로 (마지막에서 wrap → 첫 항목) |
| **LONG_PRESS** | 선택 항목 활성화 (OK / 토글 / submenu 진입) |
| **DOUBLE_TAP** | 사용 안 함 — 메뉴 내 `< Back` 항목으로 빠져나감 |

## HUD 레벨 입력 dispatch

[InkHUD2::onInput](../src/graphics/niche/InkHUD2/InkHUD2.cpp)이 입력을 받아 활성 모듈(slot)에 전달한다. 흐름:

```
TwoButton 콜백
  → InkHUD2::onInput(input)
    → pipeInstance.dispatchInputAndCheck(input)
      → 시스템 모듈(메뉴 overlay 등)이 handlesInput=true면 가로채고 return true
      → 그 외엔 활성 slot 모듈의 onInput() 호출
    → return false면 활성 모듈이 입력을 "소비하지 않은" 상태
  → handled==false && input==LONG_PRESS면 → cycleSlot(0) 호출 (다음 레이아웃)
```

핵심: **LONG_PRESS는 모듈이 명시적으로 처리하지 않으면 자동으로 cycle된다**. 모듈이 메뉴 등 sub-state로 진입할 때 `handlesInput = true`로 세팅하면 LONG_PRESS도 가로챌 수 있어 cycle을 막는다. 메뉴를 닫으면 다시 `handlesInput = false`로 돌려서 자동 cycle을 복구한다.

## 모듈별 입력 매핑

### NodeListModule ([Modules/NodeListModule.cpp](../src/graphics/niche/InkHUD2/Modules/NodeListModule.cpp))

- 메인 화면(노드 목록):
  - SHORT_PRESS — 미할당 (사용자 의도 상 추후 노드 스크롤로 사용 가능)
  - LONG_PRESS — HUD 레벨이 cycle 처리
  - DOUBLE_TAP — 시스템 메뉴(`MenuModule::open()`)

### MessageModule ([Modules/MessageModule.cpp](../src/graphics/niche/InkHUD2/Modules/MessageModule.cpp))

- MAIN 상태(채널 메시지 또는 DM thread):
  - SHORT_PRESS — `enterQuickMenu()` (Send Canned / Share Position 등 액션 허브)
  - LONG_PRESS — HUD 레벨이 cycle 처리
  - DOUBLE_TAP — Message 모듈 채널 선택 메뉴 (`enterMenu()`)
- MENU 상태(채널 선택 메뉴: Back / DM / 채널들):
  - SHORT_PRESS — 다음 항목 (wrap)
  - LONG_PRESS — 활성화 (Back / 채널 선택 / DM 항목은 DM_LIST 진입)
- DM_LIST 상태(peer 목록):
  - SHORT_PRESS — 다음 peer (wrap)
  - LONG_PRESS — peer 선택해서 그 thread MAIN으로 진입
- QUICK_MENU 상태(액션 허브: Back / Send Canned* / Share Position):
  - SHORT_PRESS — 다음 항목 (wrap)
  - LONG_PRESS — 활성화. Send Canned는 CANNED_LIST로, Share Position은 즉시 송신 후 MAIN으로
  - *Send Canned는 현재 view에 송신 대상이 있을 때만 표시
- CANNED_LIST 상태(canned 메시지 송신):
  - SHORT_PRESS — 다음 메시지 (wrap)
  - LONG_PRESS — 송신 (현재 채널/peer가 자동 destination), 송신 후 MAIN
  - Back은 한 단계 위 QUICK_MENU로 (MAIN 직행 아님)

자세한 데이터 모델·메뉴 구성은 [inkhud2-message-module.md](inkhud2-message-module.md)에서 다룬다.

### MenuModule (overlay, [Modules/MenuModule.cpp](../src/graphics/niche/InkHUD2/Modules/MenuModule.cpp))

시스템 메뉴는 NodeList의 DOUBLE_TAP으로 진입한다. `handlesInput=true`라 cycle은 안 된다.
  - SHORT_PRESS — 다음 항목
  - LONG_PRESS — 활성화

### MapModule (현재 비활성, [Modules/MapModule.cpp](../src/graphics/niche/InkHUD2/Modules/MapModule.cpp))

레이아웃 사이클에서 빠져있어 사용자 시각엔 없다. 코드는 남아있고 자체 상태머신(MAP/SETTINGS/POSITION)을 가짐. 부활시킬 때는 LONG_PRESS = cycle, DOUBLE_TAP = SETTINGS 진입으로 일반 룰에 맞춰 손봐야 함.

## 트레이드오프 — DOUBLE_TAP 활성화의 비용

[InkHUD2/Setup.cpp](../src/graphics/niche/InkHUD2/Setup.cpp)에서 메인 버튼에 무조건 `setHandlerDoublePress`를 등록한다. 따라서 모든 SHORT_PRESS가 ~300 ms 지연된다(두 번째 누름이 올지 기다리기 위해). 빠른 스크롤이 필요한 NodeList에서 답답할 수 있음. 윈도우는 `setHandlerDoublePress(0, cb, windowMs)` 두 번째 인자로 줄일 수 있지만 더블탭 인식률과 trade-off.

LONG_PRESS는 영향 없음(release가 아니라 hold 길이로 검출).

## 사용자 모델 가이드

UI 흐름 설계 시 다음 룰이 깨지지 않게 유지:

1. **메인 화면에서 long press = 항상 다음 레이아웃**. 모듈이 의미를 임의로 바꾸지 말 것 — handlesInput으로 가로채려면 sub-state로 들어간 다음에만 그러도록.
2. **메뉴 내부에서 long press = 활성화**. `< Back` 항목으로 명시적으로 빠져나오게 둘 것 — 더블탭으로 빠져나오는 동작은 채택하지 않았다(더블탭은 모듈 메뉴 진입 전용).
3. **short press가 메인에서 무동작이면 안 됨이 권장**. 사용자가 무심코 눌렀을 때 어디로 안 가면 어색하다 — 안 쓰면 단순 스크롤이라도 매핑.
