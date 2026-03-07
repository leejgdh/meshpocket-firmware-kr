/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "BatteryModule.h"

namespace InkHUD2 {

BatteryModule::BatteryModule() {
    // System module settings
    handlesInput = false;
    lockRendering = false;
    alwaysRender = true;  // Always show battery
    priority = 100;       // Render on top
}

void BatteryModule::onRender(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Rect r = layout->batteryRect();
    ctx.setClip(r);

    // Clear background (we're an overlay)
    ctx.fillRect(0, 0, r.w, r.h, Color::WHITE);

    // Battery dimensions - centered vertically in rect
    uint16_t bumpW = layout->batteryBumpWidth();
    uint16_t bodyInset = layout->batteryBodyInset();

    // Body dimensions - centered in rect with inset on all sides
    uint16_t bodyH = r.h - 2 * bodyInset;
    uint16_t bodyW = r.w - 2 * bodyInset - bumpW;
    int16_t bodyX = bodyInset + bumpW;
    int16_t bodyY = bodyInset;

    // Bump (positive terminal) - vertically centered, half height of body
    uint16_t bumpH = bodyH / 2;
    int16_t bumpX = bodyInset;
    int16_t bumpY = bodyY + (bodyH - bumpH) / 2;

    // Draw bump (positive terminal)
    ctx.fillRect(bumpX, bumpY, bumpW, bumpH, Color::BLACK);

    // Draw body outline
    ctx.rect(bodyX, bodyY, bodyW, bodyH, Color::BLACK);

    // Erase join between bump and body
    ctx.vLine(bodyX, bumpY, bumpH, Color::WHITE);

    // Battery level with hatch pattern
    uint16_t slicePad = layout->batterySlicePad();
    uint16_t maxSliceW = bodyW - (slicePad * 2);
    uint16_t sliceW = (maxSliceW * level) / 100;

    if (sliceW > 0) {
        int16_t sliceX = bodyX + slicePad;
        int16_t sliceY = bodyY + slicePad;
        uint16_t sliceH = bodyH - (slicePad * 2);
        int16_t sliceLeft = sliceX + (maxSliceW - sliceW);

        Rect fillRect = {
            sliceLeft,
            sliceY,
            sliceW,
            sliceH
        };
        ctx.hatch(fillRect, layout->batteryHatchSpacing(), Color::BLACK);
        ctx.rect(sliceLeft, sliceY, sliceW, sliceH, Color::BLACK);
    }

    ctx.clearClip();
}

void BatteryModule::onEvent(const Event& e) {
    if (e.type == EventType::LOW_BATTERY) {
        setLevel(e.data.battery.level);
        setCharging(e.data.battery.charging);
    }
}

void BatteryModule::setLevel(uint8_t lvl) {
    if (lvl > 100) lvl = 100;
    if (level != lvl) {
        level = lvl;
        requestUpdate();
    }
}

void BatteryModule::setCharging(bool chg) {
    if (charging != chg) {
        charging = chg;
        requestUpdate();
    }
}

} // namespace InkHUD2
