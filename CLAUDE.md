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

Takes ~2-3 min. Check the tail of output for `RAM:` / `Flash:` percentages and grep for `warning\|error` (excluding the harmless `.vscode/extensions.json` CRLF git warning and pre-existing upstream warnings in unrelated libs like INA3221/ICM42607P/heartRate — those aren't ours).

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

## Repo / branch context

- `main` — official Meshtastic **v2.7.26** tag + this fork's changes on top (Korean font support on **legacy InkHUD**, not InkHUD2). Rebuilt from scratch on the official base; see `archive/kuroanji-inkhud2-main` for how it used to be.
- `archive/kuroanji-inkhud2-main` — the old approach: a fork of `kuroanji/firmware`'s InkHUD2 rewrite. Kept as reference/backup only (local branch, not pushed to origin). Useful for finding prior art (e.g. the TwoButton double-press implementation and InkHUD2 design-token history were mined from here).
- Remotes: `origin` = this fork (leejgdh/meshpocket-firmware-kr), `upstream` = official meshtastic/firmware, `kuroanji` = kuroanji/firmware (added for research; kuroanji hasn't pushed anything new in over a month as of last check).
- Flash on `heltec-mesh-pocket-10000-inkhud` is tight — check the size report after any change that adds code/data, not just font work.

## Korean/CJK font system

- `src/graphics/niche/Fonts/CJK/CJKFont.h` — the lookup structure (`CJKFont`/`CJKGlyph`, binary search by full Unicode codepoint via `cjkLookup()`). Framework-agnostic, ported as-is from kuroanji's InkHUD2.
- `src/graphics/niche/Fonts/CJK/KoreanFont9px.h` — the actual glyph data. **Native 9px**, generated directly from Noto Sans KR via a modified version of kuroanji's `font_generator/generate_font_korean.py` (script + source TTF are in `/tmp/fontgen` locally, not committed — regenerate by re-fetching from `github.com/kuroanji/InkHUD2/font_generator` if needed). Deliberately excludes Latin/ASCII glyphs (legacy InkHUD already renders those via its own GFXfont path — bundling them was 100% dead weight). Hangul syllable selection is still the upstream script's naive "first 2500 by codepoint" — **not actually frequency-sorted** despite the original comment claiming so; a real fix here (proper frequency-based subset, e.g. KS X 1001's 2350 or a corpus-frequency list) is still an open TODO if further size/coverage tuning is wanted.
- `Applet::cjkFont` (static, set once in `variants/nrf52840/heltec_mesh_pocket/nicheGraphics.h`) is the single shared secondary font, scaled per active `AppletFont` (fontSmall/Medium/Large) via `Applet::cjkScale()` — matched against the actual `'0'` glyph height of the active GFXfont, not `AppletFont::lineHeight()` (which is skewed by outlier tall glyphs like accented Greek capitals — using it caused Hangul to render oversized/blocky, especially in `fontLarge` contexts).
- `Applet::drawMixed()` / `measureMixed()` in `Applet.cpp` are the mixed-script draw/measure paths (ASCII takes the original fast path unchanged; anything non-ASCII walks codepoints and prefers `cjkFont`, falling back to the existing WIN125x/emoji 8-bit path via `AppletFont::encodeCodepoint`).
- Any UI text that should support Hangul must NOT be pre-collapsed to 8-bit via `Applet::parse()` before reaching `printAt`/`printWrapped` — pass raw UTF-8 through. (`parse()`/`isPrintable()` are still used for the short-name hex-fallback decision only.)
