#pragma once

#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include "../Text/TextRenderer.h"
#include "../UI/ContentArea.h"
#include <cstdint>
#include <vector>

namespace InkHUD2 {

// Message data for rendering. `snr` and `hopsAway` are populated for
// inbound messages and rendered inline next to the timestamp; both are
// silenced for outgoing messages (sentinels: snr == 0.0f, hopsAway == 255).
//
// Note: kept as a plain aggregate (no default member initializers) so
// brace-init `{a, b, c, d, e}` works under C++14, which the platformio
// gcc-9.3.1 toolchain compiles with by default. Callers pass all five
// fields explicitly.
struct ChatMessage {
    static constexpr uint8_t HOPS_UNKNOWN = 255;

    uint32_t from;
    uint32_t timestamp;
    const char* text;
    float snr;
    uint8_t hopsAway;
};

// Callback type for getting node names
using GetNodeNameFn = const char* (*)(uint32_t nodeNum);
using FormatTimeFn = const char* (*)(uint32_t timestamp);

// ChatView - renders chat-style messages (bottom-up, multiple messages)
class ChatView {
public:
    ChatView(Buffer* buffer, const Layout* layout, TextRenderer* text);

    // Set callbacks for node info
    void setCallbacks(GetNodeNameFn getName, FormatTimeFn formatTime, uint32_t myNode);

    // Render messages in content area
    // Messages should be sorted newest-first
    void render(const ContentArea& area, const std::vector<ChatMessage>& messages);

private:
    void renderMessage(const ChatMessage& msg, bool outgoing,
                       int16_t& cursorY, const ContentArea& area,
                       int16_t msgL, int16_t msgR, uint16_t msgW);

    const char* getSenderName(uint32_t from, bool outgoing) const;
    const char* getTimeString(uint32_t timestamp) const;

    Buffer* buffer;
    const Layout* layout;
    TextRenderer* textRenderer;

    GetNodeNameFn getNodeName;
    FormatTimeFn formatTimeFn;
    uint32_t myNodeNum;

    // Info line scale
    static constexpr float INFO_SCALE = 0.78f;
};

} // namespace InkHUD2
