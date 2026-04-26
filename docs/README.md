# `docs/` — fork 작업 노트

이 폴더는 본 fork(MeshPocket 한글 InkHUD2)에서 발견한 **비자명한 동작·제약·설계 결정**을 기록한다. upstream 코드를 읽는 것만으로는 알기 어려운 내용이 대부분이라, 향후 maintainer(또는 미래의 자기 자신)가 동일한 함정을 다시 밟지 않도록 정리했다.

전체 펌웨어의 기능 단위 분석은 상위 폴더의 [ARCHITECTURE.md](../ARCHITECTURE.md)에 있다. 여기 docs/ 는 그보다 더 좁고 깊은 주제별 노트.

## 문서 목록

| 파일 | 다루는 내용 |
|---|---|
| [inkhud2-button-ux.md](inkhud2-button-ux.md) | 단일 버튼(짧게/길게/더블탭) UX 룰, 모듈별 입력 매핑, Input enum 명명 |
| [inkhud2-message-module.md](inkhud2-message-module.md) | Message 모듈의 상태머신(MAIN/MENU/DM_LIST/CANNED_LIST), DM thread 데이터 모델, 채널 vs DM 분기 |
| [inkhud2-canned-integration.md](inkhud2-canned-integration.md) | inkhud 빌드의 `HAS_SCREEN=0` 함정과 `CannedMessageStore` 활용, 송신 메시지를 화면에 미러하는 `sendToMesh` hook |
| [inkhud2-layout-guide.md](inkhud2-layout-guide.md) | 디스플레이 사양, header/body/footer 골격, 디자인 토큰 가이드, 매직 넘버 인벤토리, 마이그레이션 계획 |

## 작성 원칙

- **Why 먼저** — 어떤 결정이 왜 그렇게 됐는지를 코드보다 먼저 설명
- **upstream 가정과 우리 가정의 차이** — 이 fork가 upstream과 어디서 갈라지는지 명시
- **줄 번호는 시점별 스냅샷** — 코드는 바뀌니 정확한 줄 번호보다 함수명·파일 경로 우선
- **사용자 시나리오 → 코드 경로 → 데이터 흐름** 순서로 풀이
