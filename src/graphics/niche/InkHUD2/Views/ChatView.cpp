/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "ChatView.h"
#include "../Core/Layout.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace InkHUD2 {

// Two-tier system: body for everything in a message bubble (info line
// + message body). Header tier is reserved for the StatusBar above.

ChatView::ChatView(Buffer* buffer, const Layout* layout, TextRenderer* text)
    : buffer(buffer), layout(layout), textRenderer(text), myNodeNum(0) {}

void ChatView::setCallbacks(GetNodeNameFn getName, FormatTimeFn formatTime, uint32_t myNode) {
    getNodeName = getName;
    formatTimeFn = formatTime;
    myNodeNum = myNode;
}

const char* ChatView::getSenderName(uint32_t from, bool outgoing) const {
    if (outgoing) return "Me";
    if (getNodeName) return getNodeName(from);

    // Fallback
    static char buf[12];
    snprintf(buf, sizeof(buf), "%04X", (uint16_t)(from & 0xFFFF));
    return buf;
}

const char* ChatView::getTimeString(uint32_t timestamp) const {
    if (formatTimeFn) return formatTimeFn(timestamp);
    return "";
}

void ChatView::render(const ContentArea& area, const std::vector<ChatMessage>& messages) {
    if (!buffer || !layout || !textRenderer || messages.empty()) return;

    uint16_t padding = layout->padding();
    uint16_t lineH = layout->bodyLineHeight();

    // Message area bounds
    constexpr uint16_t LINE_MARGIN = 2;
    const int16_t msgL = area.innerLeft(padding) + LINE_MARGIN;
    const int16_t msgR = area.innerRight(padding) - LINE_MARGIN;
    const uint16_t msgW = msgR - msgL;

    // Body and info both run at body tier; one-line height for either.
    uint16_t infoLineH = lineH;
    uint16_t bodyLineH = lineH;
    uint8_t infoPadding = lineH / 5;

    // Vertical cursor - start at bottom, move up
    int16_t cursorY = area.bottom();
    // Reserve top padding so oldest message doesn't touch header
    int16_t contentTop = area.top() + infoPadding;

    // Render messages (newest first)
    for (size_t i = 0; i < messages.size(); ++i) {
        const ChatMessage& msg = messages[i];
        bool outgoing = (msg.from == 0) || (msg.from == myNodeNum);

        // Compose info string. For inbound messages we also append hop count
        // and SNR (signal quality) from the original packet so the user can
        // glance at relay path and link health per-message. Outbound (own)
        // messages get just sender + time — there's no meaningful link
        // metric for our own sends.
        char info[64];
        const char* senderName = getSenderName(msg.from, outgoing);
        const char* timeStr = getTimeString(msg.timestamp);

        char metaBuf[24];
        metaBuf[0] = '\0';
        if (!outgoing) {
            char hopsPart[8] = "";
            char snrPart[12] = "";
            if (msg.hopsAway != ChatMessage::HOPS_UNKNOWN) {
                snprintf(hopsPart, sizeof(hopsPart), "%uH", (unsigned)msg.hopsAway);
            }
            if (msg.snr != 0.0f) {
                // Round to nearest dB — fractional precision rarely matters
                // here and uses up width.
                int snrInt = (int)(msg.snr >= 0 ? msg.snr + 0.5f : msg.snr - 0.5f);
                snprintf(snrPart, sizeof(snrPart), "%ddB", snrInt);
            }
            if (hopsPart[0] && snrPart[0]) {
                snprintf(metaBuf, sizeof(metaBuf), " - %s %s", hopsPart, snrPart);
            } else if (hopsPart[0]) {
                snprintf(metaBuf, sizeof(metaBuf), " - %s", hopsPart);
            } else if (snrPart[0]) {
                snprintf(metaBuf, sizeof(metaBuf), " - %s", snrPart);
            }
        }

        if (timeStr && timeStr[0] != '\0') {
            snprintf(info, sizeof(info), "%s - %s%s", senderName, timeStr, metaBuf);
        } else {
            snprintf(info, sizeof(info), "%s%s", senderName, metaBuf);
        }

        // Wrapped body height (body tier).
        uint16_t bodyH = textRenderer->getWrappedTextHeightScaled(msgW, msg.text, Layout::bodyScale);
        uint16_t totalMsgH = infoLineH + infoPadding + bodyH;

        // Available space - need room for info line + padding + at least one line of body
        int16_t availableH = cursorY - contentTop;
        int16_t minMsgH = infoLineH + infoPadding + bodyLineH;
        if (availableH < minMsgH) break;

        int16_t msgBottom = cursorY;

        // Check if message fully fits
        bool truncated = false;
        int16_t infoY, bodyY;
        uint16_t maxBodyH;

        if (totalMsgH <= availableH) {
            // Fully fits
            int16_t bodyTop = cursorY - bodyH;
            bodyY = bodyTop;
            infoY = bodyTop - infoPadding - infoLineH;
            maxBodyH = bodyH;
        } else {
            // Truncate
            truncated = true;
            infoY = contentTop;
            bodyY = infoY + infoLineH + infoPadding;
            int16_t bodySpace = cursorY - bodyY;
            if (bodySpace < bodyLineH) break;  // Not enough space for body
            maxBodyH = static_cast<uint16_t>(bodySpace);
        }

        // Draw info line (body tier).
        uint16_t infoW = textRenderer->textWidthScaled(info, Layout::bodyScale);
        if (!outgoing) {
            textRenderer->textScaled(msgL, infoY, info, Layout::bodyScale, Align::LEFT, Color::BLACK);
        } else {
            textRenderer->textScaled(msgR, infoY, info, Layout::bodyScale, Align::RIGHT, Color::BLACK);
        }

        // Dotted underline
        int16_t ulY = infoY + infoLineH + 1;
        int16_t ulL, ulR;
        if (!outgoing) {
            ulL = msgL;
            ulR = msgL + infoW + lineH / 2;
        } else {
            ulR = msgR;
            ulL = msgR - infoW - lineH / 2;
        }
        for (int16_t x = ulL; x <= ulR; x += 2) {
            buffer->setPixel(x, ulY, Color::BLACK);
        }

        // Draw message body (body tier; outgoing right-aligned if it fits
        // on a single line, otherwise wrap-left like inbound).
        if (truncated) {
            textRenderer->textWrappedTruncatedScaled(msgL, bodyY, msgW, maxBodyH, msg.text, Layout::bodyScale, Color::BLACK);
        } else if (!outgoing) {
            textRenderer->textWrappedScaled(msgL, bodyY, msgW, msg.text, Layout::bodyScale, Color::BLACK);
        } else {
            uint16_t textW = textRenderer->textWidthScaled(msg.text, Layout::bodyScale);
            if (textW < msgW) {
                textRenderer->textScaled(msgR, bodyY, msg.text, Layout::bodyScale, Align::RIGHT, Color::BLACK);
            } else {
                textRenderer->textWrappedScaled(msgL, bodyY, msgW, msg.text, Layout::bodyScale, Color::BLACK);
            }
        }

        // Vertical line alongside message
        int16_t lineX = outgoing ? (area.right() - 1) : area.left();
        for (int16_t y = infoY; y < msgBottom; y++) {
            buffer->setPixel(lineX, y, Color::BLACK);
        }

        if (truncated) break;

        cursorY = infoY - lineH / 2;
    }
}

} // namespace InkHUD2
