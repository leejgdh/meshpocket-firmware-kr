/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "CannedMessageModule.h"
#include "MenuModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/ContentArea.h"
#include "../UI/HeaderText.h"
#include "../UI/StatusBar.h"
#include "../UI/TextUtils.h"

#include "configuration.h"
#if HAS_SCREEN
#include "MeshService.h"
#include "Channels.h"
#include "NodeDB.h"
#include "Router.h"
#include "mesh/generated/meshtastic/cannedmessages.pb.h"

extern meshtastic_CannedMessageModuleConfig cannedMessageModuleConfig;
#endif

#include <algorithm>
#include <cstring>

namespace InkHUD2 {

CannedMessageModule::CannedMessageModule() {
    messages.reserve(16);
}

void CannedMessageModule::onActivate() {
    reloadMessagesFromConfig();
    if (selectedIndex >= messages.size()) selectedIndex = 0;
    requestUpdate();
}

void CannedMessageModule::reloadMessagesFromConfig() {
    messages.clear();
#if HAS_SCREEN
    const char* raw = cannedMessageModuleConfig.messages;
    if (!raw || raw[0] == '\0') return;

    const char* start = raw;
    for (const char* p = raw; ; ++p) {
        if (*p == '|' || *p == '\0') {
            if (p > start) {
                messages.emplace_back(start, p - start);
            }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
#endif
}

void CannedMessageModule::onEvent(const Event& e) {
    // No specific event triggers a reload — we re-read on activation, which
    // is sufficient: the config is changed via the phone app and the user
    // would have to leave the layout and come back to see new messages anyway.
    (void)e;
}

void CannedMessageModule::onInput(Input input) {
    switch (input) {
        case Input::DOWN:
            if (!messages.empty()) {
                selectedIndex = (selectedIndex + 1) % messages.size();
                requestUpdate();
            }
            break;

        case Input::UP:
            if (!messages.empty()) {
                if (selectedIndex == 0) {
                    selectedIndex = messages.size() - 1;
                } else {
                    selectedIndex--;
                }
                requestUpdate();
            }
            break;

        case Input::LONG_PRESS:
            // Long-press main button = send. Mirrors MapModule pattern
            // where BACK activates the highlighted item.
            sendSelected();
            break;

        default:
            break;
    }
}

void CannedMessageModule::sendSelected() {
#if HAS_SCREEN
    if (messages.empty()) {
        if (menuModule) menuModule->showAlert("No canned messages.\nConfigure in app.");
        return;
    }
    if (selectedIndex >= messages.size()) return;
    if (!router || !service) return;

    const std::string& text = messages[selectedIndex];
    if (text.empty()) return;

    meshtastic_MeshPacket* p = router->allocForSending();
    if (!p) return;

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->want_ack = true;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.dest = NODENUM_BROADCAST;

    size_t len = text.size();
    if (len > sizeof(p->decoded.payload.bytes)) {
        len = sizeof(p->decoded.payload.bytes);
    }
    memcpy(p->decoded.payload.bytes, text.data(), len);
    p->decoded.payload.size = len;

    service->sendToMesh(p, RX_SRC_LOCAL, true);

    if (menuModule) menuModule->showAlert("Sent!");
#endif
}

void CannedMessageModule::onRender(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();

    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    // === StatusBar ===
    // Title shows target channel: "Canned -> #LongFast" (truncates to "Canned" if narrow).
    static char titleBuf[40];
#if HAS_SCREEN
    const char* chName = channels.getName(channels.getPrimaryIndex());
    if (!chName || !*chName) chName = "Primary";
    snprintf(titleBuf, sizeof(titleBuf), "Canned -> #%s", chName);
#else
    snprintf(titleBuf, sizeof(titleBuf), "Canned");
#endif
    HeaderText header(titleBuf, "Canned");

    Rect batRect = layout->batteryRect();
    uint16_t iconW = layout->lineHeight();
    uint16_t maxTitleW = batRect.x - layout->margin() - padding - iconW - padding * 2;
    const char* title = header.getText(&textRenderer, maxTitleW, Layout::headerScale);

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, title, StatusBar::Icon::ENVELOPE);

    int16_t contentBottom = ctx.height();
    ContentArea content = calculateContentArea(layout, contentTop, contentBottom);

    renderList(ctx, content);
}

void CannedMessageModule::renderList(RenderContext& ctx, const ContentArea& content) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    if (messages.empty()) {
        visibleRowCount = 0;
        ctx.text(ctx.width() / 2, content.top() + content.h / 2,
                 "No canned messages", Align::CENTER, Color::BLACK);
        return;
    }

    float scale = Layout::bodyScale;
    uint16_t lineH = layout->bodyLineHeight();
    uint16_t spacing = layout->elementSpacing();
    uint16_t rowH = lineH + spacing;

    uint16_t margin = layout->margin();
    uint16_t textInset = layout->textInset();

    visibleRowCount = (rowH > 0) ? (content.h / rowH) : 0;
    if (visibleRowCount == 0) visibleRowCount = 1;

    // Keep selected row in view.
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + visibleRowCount) {
        scrollOffset = selectedIndex - visibleRowCount + 1;
    }

    int16_t y = content.top();
    uint16_t maxTextW = content.w - 2 * textInset;

    char truncBuf[128];
    for (size_t i = scrollOffset;
         i < messages.size() && y + rowH <= content.bottom();
         ++i) {
        bool selected = (i == selectedIndex);

        if (selected) {
            // Filled rect with white text — high-contrast highlight.
            ctx.fillRect(margin, y, content.w, lineH + spacing - 1, Color::BLACK);
        }

        TextUtils::truncate(ctx, messages[i].c_str(), maxTextW, scale, truncBuf, sizeof(truncBuf));
        ctx.text(margin + textInset, y, truncBuf, Align::LEFT, selected ? Color::WHITE : Color::BLACK, scale);

        y += rowH;
    }

    // Hint at bottom: "Hold MAIN to send"
    if (content.h - (y - content.top()) >= layout->bodyLineHeight()) {
        const char* hint = "Hold MAIN to send";
        ctx.text(content.x + content.w / 2, content.bottom() - layout->bodyLineHeight(), hint, Align::CENTER, Color::BLACK, Layout::bodyScale);
    }
}

} // namespace InkHUD2
