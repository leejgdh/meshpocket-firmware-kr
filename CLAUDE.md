# MeshPocket Korean firmware — working notes

## Build target

**The user's device is the 10000mAh MeshPocket.** Always build:

```
heltec-mesh-pocket-10000-inkhud
```

Do **not** default to `heltec-mesh-pocket-5000-inkhud` — that's a different battery variant the user doesn't have. If both need checking, ask first or say so explicitly; don't build 5000 "just in case."

## Building

Only build when the user explicitly asks (they run/iterate on their own pace — don't build proactively after every edit).

PlatformIO isn't on PATH; invoke it directly:

```bash
cd "/c/code/meshpocket/meshpocket-firmware-kr"
"$HOME/.platformio/penv/Scripts/pio.exe" run -e heltec-mesh-pocket-10000-inkhud
```

Takes ~3-4 min. **Capture the full log** — don't pipe through `tail`, or the warning list is lost and only a clean rebuild can get it back. Check `RAM:` / `Flash:` percentages and grep for `warning\|error`, excluding the harmless `.vscode/extensions.json` CRLF git warning and pre-existing upstream warnings in unrelated libs like INA3221/ICM42607P/heartRate — those aren't ours.

Output UF2:
```
.pio\build\heltec-mesh-pocket-10000-inkhud\firmware-heltec-mesh-pocket-10000-inkhud-<version>.uf2
```

## Flashing

Not via PlatformIO upload — manual UF2 drag-and-drop:

1. Connect the magnetic pogo-pin cable (USB-C is charge-only, won't flash)
2. Double-click the RST button → enters DFU mode
3. A `HT-n5262` drive mounts on the PC
4. Drag the `.uf2` file onto that drive
5. Drive disappears = auto-reboot = success

Recovery if something goes wrong: same DFU steps, drag a known-good official UF2 (from `resource.heltec.cn/download/MeshPocket/firmware` or `flasher.meshtastic.org`) back onto the drive.

## Hardware facts worth knowing

**Three physical buttons, but only one is usable by firmware.**

- **USER** — `PIN_BUTTON1` = P1.10. The only button InkHUD wires (`nicheGraphics.h`, button #0: short press + long press). This is it.
- **RST** — P0.18. Silkscreened RESET, but the bootloader treats it as a plain GPIO — hence the commented-out `PIN_BUTTON2` in `variant.h`. Already spoken for: reset/wake plus double-click-DFU. Don't repurpose it; you'd lose both.
- **CTRL** — power-bank circuit only, **not connected to the nRF52840 at all**. Click = output on + battery LEDs, double-click = Qi2 wireless output off, long-press = wireless + wired off. Heltec's docs are explicit that it "relates only to the power delivery system, not the Meshtastic firmware." There is no pin for it and never will be. ([source](https://wiki.heltec.org/docs/devices/open-source-hardware/nrf52840-series/meshpocket/Usage))

That single-button constraint is exactly why `TwoButton::setHandlerDoublePress()` exists in the tree — double-press is how you get a third gesture out of one button. It currently has **zero callers**; it's infrastructure waiting for a consumer. Don't propose features that need a second button.

**No GPS.** `HAS_GPS 0` in `variant.h`, so `NodeDB.cpp` defaults `config.position.gps_mode = NOT_PRESENT` and the phone is the only position source. Consequences:

- The InkHUD Position menu page is compiled out entirely (`MenuApplet.cpp` guards it with `#if !MESHTASTIC_EXCLUDE_GPS && HAS_GPS`), so position settings can only be changed from the app or CLI.
- Firmware **never requests** position from the phone — it's a one-way push that firmware merely receives (and rate-limits to 1/10s in `PhoneAPI.cpp`). If someone blames the firmware for the iOS location indicator staying lit while the app is backgrounded, that's the app's background Core Location, not us. The firmware-side mitigation is `config.position.fixed_position`, which makes `PositionModule` ignore phone position updates outright (keeping only the time).

## Repo / branch context

- `main` — official Meshtastic **v2.7.27** tag + this fork's changes on top (Korean font support on **legacy InkHUD**, not InkHUD2). Rebuilt from scratch on the official base; see `archive/kuroanji-inkhud2-main` for how it used to be. Rebasing onto a newer upstream has been clean so far — the fork touches files upstream rarely does.
- `archive/kuroanji-inkhud2-main` — the old approach: a fork of `kuroanji/firmware`'s InkHUD2 rewrite. Kept as reference/backup only (local branch, not pushed to origin). Useful for finding prior art (e.g. the TwoButton double-press implementation and InkHUD2 design-token history were mined from here).
- Remotes: `origin` = this fork (leejgdh/meshpocket-firmware-kr), `upstream` = official meshtastic/firmware, `kuroanji` = kuroanji/firmware (added for research; kuroanji hasn't pushed anything new in over a month as of last check). `main` tracks `origin` only.
- **Flash is tight.** As of the 2.7.27 rebase: RAM 37.4%, **Flash 89.5%** (729,200 / 815,104 bytes) — about 85KB spare. Check the size report after any change that adds code/data, not just font work. When two implementations are on the table, prefer the one that removes code.

## Korean/CJK font system

- `src/graphics/niche/Fonts/CJK/CJKFont.h` — the lookup structure (`CJKFont`/`CJKGlyph`, binary search by full Unicode codepoint via `cjkLookup()`). Framework-agnostic, ported as-is from kuroanji's InkHUD2.
- `src/graphics/niche/Fonts/CJK/KoreanFont12px.h` — the actual glyph data. **Native 12px** (12x12 cell, xAdvance 13, yOffset -12), rendered directly from Noto Sans KR at that size rather than scaled down from a larger source. 2619 glyphs total: 2343 Hangul syllables plus CJK punctuation/fullwidth forms and U+FFFD. Deliberately excludes Latin/ASCII glyphs — legacy InkHUD already renders those via its own GFXfont path, so bundling them was 100% dead weight.
  - The Hangul subset spans the full U+AC00–U+D79D range (a KS X 1001-style ~2350-syllable set), **not** the upstream generator's naive "first N by codepoint". Common syllables across the whole range are present. Earlier notes here claimed this was still an open TODO; it isn't — it was fixed when the font was regenerated at 12px.
  - Generated via a modified copy of kuroanji's `font_generator/generate_font_korean.py`. **The script and source TTF are gone** — they lived in `/tmp/fontgen`, which is now empty, and were never committed. To regenerate, re-fetch from `github.com/kuroanji/InkHUD2/font_generator` and re-apply the 12px + CJK-only modifications.
- `Applet::cjkFont` (static, set once in `variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h`) is the single shared secondary font, scaled per active `AppletFont` (fontSmall/Medium/Large) via `Applet::cjkScale()` — matched against the actual `'0'` glyph height of the active GFXfont, not `AppletFont::lineHeight()` (which is skewed by outlier tall glyphs like accented Greek capitals — using it caused Hangul to render oversized/blocky, especially in `fontLarge` contexts).
- `Applet::drawMixed()` / `measureMixed()` in `Applet.cpp` are the mixed-script draw/measure paths (ASCII takes the original fast path unchanged; anything non-ASCII walks codepoints and prefers `cjkFont`, falling back to the existing WIN125x/emoji 8-bit path via `AppletFont::encodeCodepoint`).
- Any UI text that should support Hangul must NOT be pre-collapsed to 8-bit via `Applet::parse()` before reaching `printAt`/`printWrapped` — pass raw UTF-8 through. (`parse()`/`isPrintable()` are still used for the short-name hex-fallback decision only.)

## InkHUD gotchas

- **The one-page tip on every boot is `Tip::SAFE_SHUTDOWN`, and it's upstream design, not a bug.** `settings->tips.safeShutdownSeen = true` is set in exactly one place — `Events.cpp` `beforeDeepSleep()` — which runs only on a menu "Save & Shut Down" or a client-app shutdown. Pulling power, RST, or a flat battery never reaches it, so the tip re-queues forever. The same flag also keeps `LogoApplet` in its verbose onboarding boot logo. The fix is one clean shutdown; the code fix (if ever wanted) is deleting the `SAFE_SHUTDOWN` push_back in the `TipsApplet` constructor, which *saves* flash.
- Diagnosing which tip is showing: one page means `tips.firstBoot` already saved false, which also proves settings persistence is working. `Tip::ROTATION` can always be ruled out — it clears `config.display.flip_screen` during its own render, so it can never repeat.
