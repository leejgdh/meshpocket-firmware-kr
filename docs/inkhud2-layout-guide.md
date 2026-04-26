# InkHUD2 레이아웃 / 디자인 토큰 가이드

이 문서는 InkHUD2 모듈을 작성할 때의 **레이아웃 표준 규약**과 **현재 코드 베이스 분석**을 정리한다. kuroanji 가 처음 InkHUD2 를 작성할 때 디자인 시스템을 명시적으로 정립하지 않아 모듈마다 매직 넘버가 흩어져 있는 상태고, 이 문서는 그걸 통합하기 위한 가이드 + 마이그레이션 계획.

---

## 0. 디스플레이 사양 (Heltec MeshPocket)

| 항목 | 값 | 출처 |
|---|---|---|
| 패널 컨트롤러 | LCMEN2R13ECC1 (SSD16XX 계열) | [variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h](../variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h) |
| 물리 해상도 (native) | **122 × 250** (portrait, 흑백) | [src/graphics/niche/Drivers/EInk/LCMEN2R13ECC1.h](../src/graphics/niche/Drivers/EInk/LCMEN2R13ECC1.h) |
| 회전 적용 후 (defaultRotation = 2) | **250 × 122 (landscape)** | [variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h:34](../variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h#L34) |
| 비율 | aspect ratio ≈ 2.05 → "elongated" 분기 발동 (`maxDim*10/minDim > 15`) | InkHUD2 모듈 다수 |
| 디스플레이 종류 | E-Ink, 정적 표시 시 0 mW, 부분 갱신 비용 큼 | hardware |

**디자인적 함의**:
- 화면이 **좌우로 긴 얇은 띠**. 세로 행은 매우 부족하고 가로 폭은 비교적 여유. 한 화면에 5–7개 행 정도가 한계.
- E-Ink 라 **잦은 부분 갱신은 회피** (전류 + 잔상). 정적이거나 이벤트 기반 갱신이 정공.
- **컬러 없음** → 강조는 굵기·크기·구분선·아이콘으로만. 색 토큰 없음.

---

## 1. 레이아웃 골격 — Header / Body / Footer

표준 InkHUD2 모듈 화면은 세 영역으로 나눈다.

```
┌──────────────────────────────────────┐
│  Header (StatusBar)        🔋        │ ← 항상 존재. 제목 + 아이콘 + 배터리
├──────────────────────────────────────┤
│                                      │
│             Body                     │ ← 모듈 콘텐츠
│         (ContentArea)                │
│                                      │
├──────────────────────────────────────┤
│  Footer (선택)                       │ ← 거의 안 씀. Map/Menu 정도
└──────────────────────────────────────┘
```

### 1.1 Header — `StatusBar`

[`src/graphics/niche/InkHUD2/UI/StatusBar.h`](../src/graphics/niche/InkHUD2/UI/StatusBar.h)

- **모든 모듈의 메인 화면이 사용** (NodeList, Message, Map, Menu, Boot 일부 제외).
- 표준 호출:
  ```cpp
  StatusBar statusBar(buffer, layout, &textRenderer);
  int16_t contentTop = statusBar.render(layout->margin() + padding, title, StatusBar::Icon::ENVELOPE);
  ```
- 높이 = `lineHeight() + padding * 2` ≈ 22 px (250×122 기준).
- 좌측 아이콘 + 제목, 우측 배터리 아이콘.
- 제목 폰트 스케일 = `StatusBar::TITLE_SCALE` = `Layout::titleScale` (= 0.78f).

### 1.2 Body — `ContentArea`

[`src/graphics/niche/InkHUD2/UI/ContentArea.h`](../src/graphics/niche/InkHUD2/UI/ContentArea.h)

- StatusBar 가 반환한 `contentTop` 부터 footer 가 있으면 그 위까지가 body.
- `calculateContentArea(layout, contentTop, contentBottom)` 로 구함 — 우측은 배터리 아이콘 폭 빼고 패딩 들어감.
- body 안의 내부 좌표 계산은 `area.left()`, `area.top()`, `area.right()`, `area.bottom()`, `area.innerLeft(padding)` 등 헬퍼 사용.

### 1.3 Footer — 거의 미사용

[`src/graphics/niche/InkHUD2/UI/Footer.h`](../src/graphics/niche/InkHUD2/UI/Footer.h)

현재 사용하는 모듈:
- `MapModule` — 스케일바 + 힌트
- `MenuModule` — "Long press: select" 같은 힌트 표시

**MeshPocket 기본 사용 시나리오에선 footer 거의 안 씀**. NodeList, Message, Boot 모두 footer 없음. 새 모듈 만들 땐 footer 가 정말 필요한지 한 번 더 고민하고, 안 쓰면 body 가 화면 끝까지 가도록.

---

## 2. 토큰 카탈로그 — 의미 기반 토큰 시스템

[`src/graphics/niche/InkHUD2/Core/Layout.h`](../src/graphics/niche/InkHUD2/Core/Layout.h) 가 디자인 시스템의 단일 소스. 모든 모듈은 의미 기반 alias 만 사용한다.

### 2.1 폰트 스케일 (의미 기반 토큰)

| 토큰 | 값 | 의미 / 사용 |
|---|---|---|
| `Layout::bodyScale` | **1.00f** | 기본 본문 텍스트 (NodeList shortName, ChatView 본문, Boot pairing PIN 등) |
| `Layout::bodyDenseScale` | **0.89f** | elongated 화면용 본문 압축 (ChatView elongated 본문) |
| `Layout::titleScale` | **0.78f** | 헤더/StatusBar 제목, 보조 라벨 (NodeList row 시간 스탬프) |
| `Layout::captionScale` | **0.78f** | 보조 텍스트 (NodeList longName-elongated, hint, ChatView info 라인) |
| `Layout::metaScale` | **0.67f** | 가장 작은 메타 정보 (NodeList longName-elongated). 이보다 작게 쓰지 말 것 |
| `Layout::menuItemScale` | **0.89f** | 메뉴 항목 (MenuModule list rows) |

`titleScale = captionScale = 0.78f` 는 **현재 같은 값** 이지만 의미가 다르므로 분리 유지. 미래에 헤더/캡션 톤이 달라지면 둘 중 하나만 조정 가능.

`bodyDenseScale = menuItemScale = 0.89f` 도 같은 이유 — 채팅 본문과 메뉴 항목이 우연히 같은 사이즈일 뿐, 한 쪽만 조정 가능해야 함.

### 2.1.1 Legacy alias (backward compat)

| Legacy | → 새 토큰 | 비고 |
|---|---|---|
| `Layout::smallScale` | `captionScale` | 기존 호출 사이트 다수 (MapModule labels, MenuModule, NodeListModule.h truncate default) |
| `Layout::menuScale` | `menuItemScale` | |
| `Layout::hintScale` | `captionScale` | |
| `Layout::verticalScale` | `menuItemScale` | |

⚠️ **결합 주의**: legacy alias 가 새 토큰을 가리키므로, `captionScale` 을 조정하면 모든 `smallScale` 호출 사이트도 같이 움직임. 의도하지 않으면 alias 를 자체 literal 로 분리.

### 2.2 라인 높이

| 헬퍼 | 의미 |
|---|---|
| `layout->lineHeight()` | 기본 폰트 (1.0) 의 라인 높이 |
| `layout->smallLineHeight()` | smallScale 폰트의 라인 높이 |
| `layout->menuLineHeight()` | menuScale 폰트의 라인 높이 |
| `layout->hintLineHeight()` | hintScale 폰트의 라인 높이 |

라인 높이 헬퍼는 잘 마련돼있음. 단, 모듈에서 `static_cast<uint16_t>(lineH * 0.67f)` 같은 매직 곱셈을 직접 하는 곳이 있어 라인 높이도 토큰화 필요.

### 2.3 간격

| 헬퍼 | 참조값 (REF, ~200x200 기준) | 의미 |
|---|---|---|
| `padding()` | 2 | 컨테이너 안쪽 패딩 |
| `margin()` | 2 | 컨테이너 바깥쪽 마진 |
| `elementSpacing()` | 2 | 인접 요소 사이 |
| `sectionSpacing()` | 4 | 섹션 사이 (큰 구분) |
| `slotSpacing()` | (별도) | 슬롯 사이 |
| `contentPadding()` | 4 | 콘텐츠 패딩 |
| `dotSpacing()` | 3 | 점/도트 사이 |
| `textInset()` | 4 | 텍스트와 가장자리 |
| `verticalTextGap()` | 10 | 수직 텍스트 사이 |
| `nodeTextInset()` | 2 | NodeList 전용 (얇음) |

이 정도면 충분한데 **모듈이 이걸 사용하지 않거나, 더 작은 단위 (`nodeTextInset`) 가 필요해서 모듈별 inset 헬퍼를 만들어 흩어져 있는 게 문제**.

---

## 3. 매직 넘버 인벤토리 — `feature/design-tokens` 에서 정리됨

이전 상태:

| 위치 | 매직 | 의미 |
|---|---|---|
| `NodeListModule.cpp` | `0.67f`, `1.0f`, `Layout::smallScale` | metaScale, bodyScale, captionScale |
| `ChatView.cpp` | `ELONGATED_BODY_SCALE = 0.89f` | menuItemScale |
| `ChatView.h` | `INFO_SCALE = 0.78f` | captionScale |
| `StatusBar.h` | `TITLE_SCALE = 0.78f` | titleScale |
| `BootModule.cpp` | `Layout::smallScale : 1.0f` | captionScale vs bodyScale |
| `CannedMessageModule.cpp` | `Layout::smallScale : 1.0f` | captionScale vs bodyScale |
| `DMView.{cpp,h}` | `ELONGATED_BODY_SCALE = 0.89f` (중복) | 파일 자체 dead → 삭제 |

정리 후: 모든 모듈이 `Layout::titleScale / bodyScale / captionScale / metaScale / menuItemScale` alias 또는 같은 값으로 정의된 `Layout::smallScale / menuScale` legacy alias 만 사용. 매직 float 0개.

`LINE_MARGIN = 2` 같은 모듈 로컬 정수 상수는 의미상 `elementSpacing()` 헬퍼와 동등해 향후 정리 후보.

### 결론
**이미 같은 값을 다른 이름으로 세 군데서 박아두고 있음**. 디자인 토큰 통합으로 한 곳에서 관리해야 함.

---

## 4. 가이드라인 (제안)

### 4.1 폰트 사이즈 토큰

`Layout` 클래스에 의미 기반 스케일 추가:

```cpp
class Layout {
public:
    // === Type scale ===
    static constexpr float titleScale   = 0.78f;  // 헤더 제목 (= 기존 TITLE_SCALE)
    static constexpr float bodyScale    = 1.00f;  // 본문 기본 (= "regular")
    static constexpr float captionScale = 0.78f;  // 부가 정보 (= 기존 smallScale)
    static constexpr float metaScale    = 0.67f;  // 가장 작은 메타 정보 (= NodeList 의 0.67f)
    static constexpr float menuItemScale = 0.89f; // 메뉴 항목 (= 기존 menuScale)

    // === 매칭 라인 높이 ===
    uint16_t titleLineHeight() const   { return scaledHeight(titleScale); }
    uint16_t bodyLineHeight() const    { return lineHeight(); }
    uint16_t captionLineHeight() const { return smallLineHeight(); }
    uint16_t metaLineHeight() const    { return scaledHeight(metaScale); }
    uint16_t menuLineHeight() const;   // 이미 있음
};
```

**규칙**:
- 모듈은 매직 숫자 (0.67f 등) 직접 쓰지 않는다.
- 모듈은 `Layout::titleScale` / `bodyScale` / `captionScale` / `metaScale` / `menuItemScale` 중 하나를 골라 쓴다.
- 새 의미가 필요하면 Layout 에 토큰 추가 (모듈에서 만들지 않는다).

### 4.2 간격 토큰 — 이미 충분하지만 사용 일관성 강화

기존 토큰 그대로 사용:
- 인접 요소 간격 = `elementSpacing()`
- 섹션 사이 = `sectionSpacing()`
- 컨테이너 안쪽 = `padding()`
- 컨테이너 바깥쪽 = `margin()`

**규칙**:
- 모듈에서 `2`, `4` 같은 raw 정수를 좌표 계산에 직접 쓰지 않는다.
- `nodeTextInset()` 처럼 모듈 전용 inset 이 정말 필요하면 Layout 에 헬퍼로 추가하고, 의미 있는 이름으로 둔다 (지금 `nodeTextInset` 은 의미 OK).

### 4.3 글자–아이콘 간격 규칙

레이아웃 row 안에 `[icon][text]` 또는 `[text][icon]` 패턴이 자주 나옴 (StatusBar, NodeList row, Footer 등).

**규칙**:
- 아이콘과 텍스트 사이 간격 = `elementSpacing()` (현재 약 2 px)
- 같은 슬롯의 두 시각 요소 사이 (예: time ↔ signal bars) = `elementSpacing() * 2` 정도로 살짝 넉넉하게 (NodeList 에서 채택 중)
- 아이콘 자체의 박스 크기 = `lineHeight()` 기준 (StatusBar: `iconW = lineH`, `iconH = lineH * 2 / 3`)

### 4.4 베이스라인 정렬

현재 InkHUD2 의 `text()` / `textScaled()` 는 **Y 인자가 텍스트 박스의 top** 으로 동작한다 (cursor line 이 아니라). 그래서 정렬이 박스 top 기준.

문제는 **세로 중앙 정렬이 필요한 경우 모듈이 매번 직접 계산**:
```cpp
int16_t centerY = lineAY + (shortLineH - WIFI_H) / 2;
```

**규칙 / 개선**:
- 같은 row 안에 **여러 폰트 사이즈가 섞이면**, 시각 중심이 아니라 **모두 같은 baseline 에 정렬**한다.
- baseline = "행 박스의 bottom 에서 (descender 무시) 폰트 X-height 만큼 위" 가 정석이지만, 우리 텍스트 렌더러가 baseline 기반 정렬 API 를 안 가지고 있으면 먼저 그걸 추가하는 것이 첫 작업.
- 그 전까지는 **cap-line (행 박스 top + ascent)** 기준으로 맞추되, 한 row 의 모든 텍스트는 **같은 Y top** 을 쓰도록 강제 (각 모듈이 알아서 +offset 하지 말 것).

현재 NodeListModule line A 가 그래도 한 Y 기준 (`lineAY`) 으로 텍스트/아이콘 모두 정렬해서 비교적 OK. ChatView 는 info 라인 / body 라인이 다른 lineHeight 를 사용해서 baseline 이 어긋날 가능성. **확인 후 baseline 정렬 헬퍼 도입 여부 결정**.

### 4.4.1 우측 가장자리 마진 — raw 픽셀 vs 폰트 글리프

E-Ink 우측 끝에 무언가를 그릴 때 마진 폭이 **무엇을 그리느냐에 따라 다름**:

| 그리는 대상 | 권장 우측 마진 | 이유 |
|---|---|---|
| **Raw 픽셀 그래픽** (signal bars, Wi-Fi 아이콘 등 `fillRect`/`setPixel` 직접) | `nodeTextInset()` (또는 `padding()`) 만 | 픽셀 좌표가 정확. 작은 inset 으로 충분 |
| **폰트 글리프** (`text()` / `textScaled()` RIGHT-align 호출) | `nodeTextInset() + elementSpacing()` 또는 그 이상 | 글자 마지막 pixel 이 advance position 너머로 돌출되어 클립됨 |

NodeList 의 line A (signal/icon) 와 line B (distance text) 가 다른 마진을 쓰는 이유. 새 모듈도 같은 정책 적용.

### 4.5 폰트 스타일 (volumes)

현재 InkHUD2 가 사용하는 폰트는 **`UnifiedFont18px` 한 종류**, 굵기 한 가지 (regular). Bold/Italic variant 없음.

**규칙**:
- **굵기로 강조하지 않는다**. 강조 필요하면 크기 (titleScale > bodyScale) 또는 시각 요소 (구분선, 아이콘) 로.
- 만약 미래에 Bold variant 가 추가되면 `Layout::bodyBoldScale` 같은 게 아니라 `TextStyle { scale, weight }` 같은 구조체를 도입.

### 4.6 행 (Row) 패턴

NodeList row 가 좋은 사례 (line A + line B 두 줄):

```
Line A: [primary text]                  [meta] [signal/status]
Line B: [secondary text...]                    [distance]
```

**규칙**:
- Row 는 최대 2 줄 (E-Ink 세로 공간 한정).
- Line A = primary scale (`bodyScale` 또는 `captionScale`), Line B = 더 작은 scale (`captionScale` 또는 `metaScale`).
- 우측 슬롯은 1–2 개 (시간 + 신호 바 같이). 더 많으면 행이 깨짐.
- 좌측 텍스트가 길면 **truncate with ellipsis** ([`NodeListModule::truncateWithEllipsis`](../src/graphics/niche/InkHUD2/Modules/NodeListModule.cpp) 참고).

ChatView 의 메시지 row 는 다른 패턴 (info 라인 + 본문) — chat 특수성 때문. 일반화하지 말 것.

---

## 5. 모듈 분석 — 현재 구조

### 5.1 NodeListModule

[`src/graphics/niche/InkHUD2/Modules/NodeListModule.cpp`](../src/graphics/niche/InkHUD2/Modules/NodeListModule.cpp)

**Row 구성**:
```
Line A:  [shortName fullScale]              [HH:MM smallScale]   [bars 또는 wifi icon]
Line B:  [longName ... metaScale]                                [N km metaScale]
```

**좋은 점**:
- 두 줄 row 패턴이 깔끔.
- truncate-with-ellipsis 처리.
- elongated / square 분기로 화면 비율 대응.
- 우측 컬럼 위치 계산이 `rightSlotLeftX = rightSlotX - barsWidth` 식으로 정렬 기준 명확.

**개선 필요**:
- `0.67f` 매직 넘버 → `metaScale` 토큰화.
- `1.0f`, `Layout::smallScale` 분기 → `bodyScale`, `captionScale` 의미로 재명명.
- 우측 마진이 line A (`nodeTextInset`) 와 line B (`nodeTextInset + elementSpacing`) 가 다른데, 라인별 마진 정책이 가이드에 명시되어야 (지금은 cpp 코멘트로만).

### 5.2 MessageModule + ChatView

[`src/graphics/niche/InkHUD2/Modules/MessageModule.cpp`](../src/graphics/niche/InkHUD2/Modules/MessageModule.cpp), [`src/graphics/niche/InkHUD2/Views/ChatView.cpp`](../src/graphics/niche/InkHUD2/Views/ChatView.cpp)

**구성**:
- MessageModule: 5 가지 state (MAIN / MENU / DM_LIST / QUICK_MENU / CANNED_LIST) — 각 state 가 별도 render 함수
- MAIN view 는 ChatView 가 메시지 리스트 그림 (info 라인 + 본문)

**좋은 점**:
- state 별 render 함수 분리 깔끔
- info 라인 (`Sender - 12:34 - 2H 5dB`) 과 본문이 시각적으로 구분
- elongated 분기로 본문 폰트 살짝 작게

**완료 / 개선 필요**:
- ✅ `INFO_SCALE` → `Layout::captionScale` (alias)
- ✅ `ELONGATED_BODY_SCALE` → `Layout::bodyDenseScale` (전용 토큰)
- info 라인 baseline 과 본문 baseline 이 어긋날 가능성 (시각 검증 필요)

### 5.3 BootModule (PAIRING 등)

별도 분석 — 현재 큰 문제 없음. 큰 PIN 표시는 Layout 토큰 아니라 직접 큰 폰트 곱셈 사용 중. 이건 BLE pairing 의 한정 상황이라 OK.

### 5.4 MenuModule

`menuItemScale = 0.89f` 사용. 이미 토큰화 되어 있음. 좋음.

### 5.5 MapModule (빌드 제외)

UX 룰과 안 맞아 빌드 제외 상태. 디자인 토큰 도입할 때 같이 정리.

---

## 6. 마이그레이션 계획 (진행도)

### ✅ 단계 1: 토큰 추가 — 완료
- `Layout.h` 에 `bodyScale / bodyDenseScale / titleScale / captionScale / metaScale / menuItemScale` 추가
- 라인 높이 헬퍼 `bodyLineHeight() / titleLineHeight() / captionLineHeight() / metaLineHeight() / menuItemLineHeight()` 추가
- legacy `smallScale / menuScale / hintScale / verticalScale` 는 alias 유지 (값 같음)

### ✅ 단계 2: 모듈 매직 넘버 → alias 교체 — 완료
- NodeListModule, ChatView, StatusBar, Footer, BootModule, CannedMessageModule, MenuModule 모두 의미 기반 토큰 사용
- DMView 삭제 (dead code, ChatView 가 흡수)
- 변경 후 매직 float (font scale) 0개

### 단계 3: 시각 회귀 검증 — 진행 중
- 각 모듈 화면 캡처 (또는 디바이스에서 직접 확인) — 폰트 사이즈 1px 차이도 e-ink 에서 보임.
- 값은 동일하게 보존되었으므로 시각 차이는 없어야 함.
- baseline 어긋남 발견되면 헬퍼 도입.

### 단계 4: 신규 모듈 작성 시 토큰만 사용 (지속)
- 매직 넘버 한 줄도 추가하지 않는 룰. PR review 시 강제.

### 단계 5: 문서 업데이트 — 완료
- 이 가이드를 alias 추가 후 갱신
- docs/README.md 인덱스에 가이드 등록

---

## 7. 빠른 체크리스트 — 새 모듈 만들 때

- [ ] StatusBar 로 헤더 시작 (`statusBar.render(...)`)
- [ ] `calculateContentArea()` 로 body 영역 계산
- [ ] Footer 정말 필요한지 검토 (보통 안 필요)
- [ ] 모든 폰트 스케일 = `Layout::titleScale/bodyScale/captionScale/metaScale/menuItemScale` 중 하나
- [ ] 모든 간격 = `padding/margin/elementSpacing/sectionSpacing/textInset` 헬퍼
- [ ] elongated / square 분기는 화면 비율에 따라
- [ ] Row 는 최대 2 줄, 우측 슬롯 1–2 개
- [ ] 굵기로 강조하지 않음 (크기/구분선/아이콘으로)
- [ ] 매직 정수/실수 0 개

위 체크리스트 통과하지 않으면 PR 리뷰에서 필히 지적.

---

## 부록 — 현재 코드 베이스 레퍼런스 표

| 값 | 토큰 | 사용처 |
|---|---|---|
| **1.00f** | `Layout::bodyScale` | NodeList shortName(square), ChatView 본문(square), Boot pairing PIN(landscape), MenuModule shutdown text(landscape) |
| **0.89f** | `Layout::bodyDenseScale` | ChatView 본문(elongated) |
| **0.89f** | `Layout::menuItemScale` (≡ legacy `menuScale`/`verticalScale`) | MenuModule list rows |
| **0.78f** | `Layout::titleScale` | StatusBar TITLE_SCALE |
| **0.78f** | `Layout::captionScale` (≡ legacy `smallScale`/`hintScale`) | NodeList longName(elongated), NodeList row time, ChatView INFO_SCALE, Footer TEXT_SCALE, Boot pairing header(vertical), CannedMessage(elongated)+hint, MapModule labels |
| **0.67f** | `Layout::metaScale` | NodeList longName line height(elongated) |

