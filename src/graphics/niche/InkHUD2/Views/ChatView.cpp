/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "ChatView.h"
#include <cstdio>
#include <cstring>

namespace InkHUD2 {

ChatView::ChatView(Buffer* buffer, const Layout* layout, TextRenderer* text)
    : buffer(buffer), layout(layout), textRenderer(text),
      getNodeName(nullptr), formatTimeFn(nullptr), myNodeNum(0) {}

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
    uint16_t lineH = layout->lineHeight();

    // Message area bounds
    constexpr uint16_t LINE_MARGIN = 2;
    const int16_t msgL = area.innerLeft(padding) + LINE_MARGIN;
    const int16_t msgR = area.innerRight(padding) - LINE_MARGIN;
    const uint16_t msgW = msgR - msgL;

    // Scaled heights
    uint8_t infoLineH = static_cast<uint8_t>(lineH * INFO_SCALE + 0.5f);
    uint8_t infoPadding = lineH / 5;

    // Vertical cursor - start at bottom, move up
    int16_t cursorY = area.bottom();
    // Reserve top padding so oldest message doesn't touch header
    int16_t contentTop = area.top() + infoPadding;

    // Render messages (newest first)
    for (size_t i = 0; i < messages.size(); ++i) {
        const ChatMessage& msg = messages[i];
        bool outgoing = (msg.from == 0) || (msg.from == myNodeNum);

        // Compose info string
        char info[48];
        const char* senderName = getSenderName(msg.from, outgoing);
        const char* timeStr = getTimeString(msg.timestamp);

        if (timeStr && timeStr[0] != '\0') {
            snprintf(info, sizeof(info), "%s - %s", senderName, timeStr);
        } else {
            strncpy(info, senderName, sizeof(info) - 1);
            info[sizeof(info) - 1] = '\0';
        }

        // Calculate message height
        uint16_t bodyH = textRenderer->getWrappedTextHeight(msgW, msg.text);
        uint16_t totalMsgH = infoLineH + infoPadding + bodyH;

        // Available space - need room for info line + padding + at least one line of body
        int16_t availableH = cursorY - contentTop;
        int16_t minMsgH = infoLineH + infoPadding + lineH;
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
            if (bodySpace < lineH) break;  // Not enough space for body
            maxBodyH = static_cast<uint16_t>(bodySpace);
        }

        // Draw info line
        uint16_t infoW = textRenderer->textWidthScaled(info, INFO_SCALE);
        if (!outgoing) {
            textRenderer->textScaled(msgL, infoY, info, INFO_SCALE, Align::LEFT, Color::BLACK);
        } else {
            textRenderer->textScaled(msgR, infoY, info, INFO_SCALE, Align::RIGHT, Color::BLACK);
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

        // Draw message body
        if (truncated) {
            textRenderer->textWrappedTruncated(msgL, bodyY, msgW, maxBodyH, msg.text, Color::BLACK);
        } else {
            if (!outgoing) {
                textRenderer->textWrapped(msgL, bodyY, msgW, msg.text, Color::BLACK);
            } else {
                uint16_t textW = textRenderer->textWidth(msg.text);
                if (textW < msgW) {
                    textRenderer->text(msgR, bodyY, msg.text, Align::RIGHT, Color::BLACK);
                } else {
                    textRenderer->textWrapped(msgL, bodyY, msgW, msg.text, Color::BLACK);
                }
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
