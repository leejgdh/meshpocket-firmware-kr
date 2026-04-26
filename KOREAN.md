# MeshPocket 한국어 펌웨어 (한글 표시 지원)

## 이 fork는 무엇인가

[Heltec MeshPocket](https://heltec.org/project/meshpocket/) (nRF52840 + SX1262 + 2.13" E-Ink) 의 화면에 **한글 메시지를 표시할 수 있도록** 커스텀한 Meshtastic 펌웨어다.

공식 Meshtastic 펌웨어는 디스플레이에 8비트 폰트 인코딩(최대 255 글리프)을 사용해 11,172자에 달하는 한글을 그릴 수 없다. 이 fork는 [kuroanji/firmware](https://github.com/kuroanji/firmware)의 InkHUD2 시스템(UTF-32 codepoint 기반 secondary bitmap font system)을 채택하고 그 위에 한글 비트맵 폰트를 얹어 한글 메시지·노드명을 정상 표시한다.

**Upstream chain**: [meshtastic/firmware](https://github.com/meshtastic/firmware) → [kuroanji/firmware (InkHUD2)](https://github.com/kuroanji/firmware/tree/InkHUD2) → 이 fork

## 적용된 변경사항

### 한글 폰트 적용
- `src/graphics/niche/Fonts/CJK/UnifiedFont18px.h` — kuroanji/InkHUD2의 `KoreanFont18px.h`(2749 glyph: ASCII 95 + Hangul 2343 + 자모 95 + Fullwidth 94 + CJK Punct 64 + Symbols 46, 110KB)로 교체. 심볼명을 `UnifiedFont18px*`로 통일하여 InkHUD2 코어 수정 없이 동작.
- `src/graphics/niche/Fonts/CJK/UnifiedFont18px_JP.h.bak` — 일본어 원본 백업 (롤백용)

### MeshPocket variant — InkHUD2 활성화
- `variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h` — `USE_INKHUD2` 가드로 InkHUD2 setup() 호출. 레거시 InkHUD1 경로도 보존되어 있어 플래그 한 줄로 전환 가능
- `variants/nrf52840/heltec_mesh_pocket/platformio.ini` — `-inkhud` 환경 두 개 (5000/10000mAh)에 `-D USE_INKHUD2` 추가

### Timezone double-application 버그 수정
InkHUD2가 `getValidTime(..., true)`로 이미 TZ가 적용된 epoch를 받은 뒤 다시 `localtime()`을 호출해 TZ가 두 번 적용되는 버그 수정. 3개 호출 위치에서 `local` 인자를 `false`로 변경:
- `src/graphics/niche/InkHUD2/Modules/MenuModule.cpp` (StatusBar 시간 표시)
- `src/graphics/niche/InkHUD2/Modules/MapModule.cpp` (위치 elapsed 계산)
- `src/graphics/niche/InkHUD2/Events.cpp` (메시지 timestamp)

## 빌드 / 플래시

### 빌드 환경 4종
| 환경 | UI | 한글 | 비고 |
|---|---|---|---|
| `heltec-mesh-pocket-5000-inkhud` / `-10000-inkhud` | **InkHUD2** | ✅ | **권장** |
| `heltec-mesh-pocket-5000` / `-10000` | 레거시 InkHUD1 | ❌ | 한글 안 됨 |

5000/10000 차이는 배터리 OCV 환산 테이블만 (잘못 골라도 펌웨어는 동작, 배터리 % 표시만 부정확).

### 빌드
PlatformIO Project Tasks → `heltec-mesh-pocket-{5000|10000}-inkhud` → General → **Build**
산출물: `.pio/build/heltec-mesh-pocket-{5000|10000}-inkhud/firmware-*.uf2`

### 플래시
1. 포고핀 케이블 연결 (USB-C는 충전 전용)
2. RST 버튼 **더블클릭** → DFU 모드 진입 → `HT-n5262` 드라이브 마운트
3. UF2 파일을 그 드라이브로 드래그
4. 드라이브가 자동으로 사라지면 = 재부팅 = 성공

### 복구 (펌웨어 망가졌을 때)
DFU 모드 진입 후 **공식 UF2를 드라이브에 드래그**하면 원상복구.
- Heltec 공식: `resource.heltec.cn/download/MeshPocket/firmware`
- Meshtastic Web Flasher: `flasher.meshtastic.org` → MeshPocket 선택 → Download UF2

## 알려진 이슈 / 트레이드오프

### InkHUD2 채택의 대가
InkHUD(1) → InkHUD2 전환으로 다음 기능들이 빠진다 (InkHUD2는 작업 중인 신버전이라 미이식):

- **CannedMessage 송신** — 기기 단독으로 미리 만든 문구를 보내는 기능 부재. 송신은 100% 모바일 앱
- **Node Config 메뉴 트리** — LoRa region, Channel, Timezone, WiFi 등 펌웨어 설정 변경을 기기에서 못 함 (앱에서만)
- 기타: Layout 변경(슬롯 수), Invert Color, Tips, Recents Duration

대신 **추가된 것**: Hide PIN (BLE 보안), 채널별 Alerts ON/OFF, Backup/Restore (Golden backup), Ping (위치 즉시 송신), Shut Down 메뉴

### 펌웨어 버전
이 fork의 base는 Meshtastic **2.7.20**. 공식은 현재 stable release 없이 alpha만 내고 있으며 (2.7.18 ~ 2.7.22 alpha 범위), 우리 fork는 그 한가운데로 공식 alpha보다 단 2버전(2.7.21, 2.7.22) 뒤. 거의 최신 트랙이라 단기 rebase 필요성은 낮다.

## 라이선스

Meshtastic 본체와 동일하게 **GPL v3**.

## 참고

- 본 fork: [github.com/leejgdh/meshpocket-firmware-kr](https://github.com/leejgdh/meshpocket-firmware-kr)
- Upstream (InkHUD2): [github.com/kuroanji/firmware](https://github.com/kuroanji/firmware/tree/InkHUD2)
- Original InkHUD2 프로젝트: [github.com/kuroanji/InkHUD2](https://github.com/kuroanji/InkHUD2)
- 공식 Meshtastic 펌웨어: [github.com/meshtastic/firmware](https://github.com/meshtastic/firmware)
