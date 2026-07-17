---
description: Build the MeshPocket InkHUD firmware (10000mAh variant by default) and report size/warnings
argument-hint: [env-name]
---

# `/build` — MeshPocket firmware build

Build the firmware and report a clean summary. Only run this when the user actually asks for a build — don't build proactively after every edit.

## What to do

1. **Pick the target environment.**
   - No argument → `heltec-mesh-pocket-10000-inkhud`. **This is the user's device.** Never default to `heltec-mesh-pocket-5000-inkhud` — that's a different battery variant they don't have. If you think 5000 also needs checking, ask first instead of building it unprompted.
   - `$ARGUMENTS` given → use that env name instead (e.g. `/build heltec-mesh-pocket-5000-inkhud` if the user explicitly wants the other variant).

2. **Run PlatformIO directly** (it isn't on PATH):

   ```bash
   cd "/c/code/meshpocket/meshpocket-firmware-kr"
   "$HOME/.platformio/penv/Scripts/pio.exe" run -e <env>
   ```

   Takes ~2-3 min on a clean/incremental build. It'll likely exceed the default tool timeout — run it in the background and wait for the completion notification rather than polling.

3. **Check the result:**
   - `grep -n "warning\|error"` on the log, but ignore known noise: the `.vscode/extensions.json` CRLF git warning, and pre-existing upstream warnings in libs we don't own (INA3221 packed-pointer warnings, ICM42607PSensor unused-variable warnings, heartRate.cpp parentheses warning). Only flag warnings that touch files under `src/graphics/niche/`, `variants/nrf52840/heltec_mesh_pocket/`, or anything else we've actually edited.
   - Grab the `RAM:` / `Flash:` lines. Flash on this target runs tight (was 97.6% before the Korean font was cut down to a native 9px asset; check `CLAUDE.md` for current context) — call out the percentage either way, and flag clearly if it goes above ~95% again.

4. **Report the UF2 path:**

   ```
   .pio\build\<env>\firmware-<env>-<version>.uf2
   ```

5. **Don't flash it yourself.** Flashing is manual (UF2 drag-and-drop over DFU, not `pio run -t upload`) and only the user can do it — they have the physical device. Just hand them the path and, if they ask how, remind them:
   - Pogo-pin cable (USB-C is charge-only)
   - Double-click RST → DFU mode → `HT-n5262` drive mounts
   - Drag the `.uf2` onto that drive; drive disappearing = success
