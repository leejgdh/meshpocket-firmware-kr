/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "NotificationModule.h"

namespace InkHUD2 {

NotificationModule::NotificationModule() {
    handlesInput = false;
    lockRendering = false;
    alwaysRender = true;
    priority = 90;  // Below battery, above content
}

void NotificationModule::onRender(RenderContext& ctx) {
    if (!visible || message[0] == '\0') return;

    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Rect r = layout->notificationRect();
    uint16_t padding = layout->padding();
    uint16_t cornerR = padding;

    // Background (white with black border)
    ctx.fillRoundRect(r.x, r.y, r.w, r.h, cornerR, Color::WHITE);
    ctx.roundRect(r.x, r.y, r.w, r.h, cornerR, Color::BLACK);

    // Text centered in notification bar
    int16_t textX = r.x + r.w / 2;
    int16_t textY = r.y + (r.h - layout->lineHeight()) / 2;

    // Temporarily set clip to notification rect for text
    ctx.setClip(r);
    ctx.text(textX - r.x, textY - r.y, message, Align::CENTER, Color::BLACK);
    ctx.clearClip();
}

void NotificationModule::show(const char* msg, uint32_t durationMs) {
    if (!msg) return;

    // Copy message
    size_t len = strlen(msg);
    if (len >= MAX_MESSAGE_LEN) len = MAX_MESSAGE_LEN - 1;
    memcpy(message, msg, len);
    message[len] = '\0';

    visible = true;
    duration = durationMs;
    // showTime will be set by tick()

    requestUpdate();
}

void NotificationModule::dismiss() {
    if (visible) {
        visible = false;
        message[0] = '\0';
        requestUpdate();
    }
}

void NotificationModule::tick(uint32_t currentMs) {
    if (!visible) return;

    if (showTime == 0) {
        showTime = currentMs;
    }

    if (duration > 0 && currentMs - showTime >= duration) {
        dismiss();
    }
}

} // namespace InkHUD2
