#pragma once

#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include "../Text/TextRenderer.h"
#include <cstdint>

namespace InkHUD2 {

// StatusBar - top bar with icon and title
// Used by: MessageModule (Group Chat, DM), potentially others
class StatusBar {
public:
    enum class Icon : uint8_t {
        NONE,
        ENVELOPE,     // Messages
        USERS,        // Node list
        GEAR,         // Settings
        INFO,         // Info/About
        MAP           // Map
    };

    StatusBar(Buffer* buffer, const Layout* layout, TextRenderer* text);

    // Render status bar at top of given area
    // Returns Y position below the separator (content start)
    // centered = true for alert/info pages (title centered, no icon)
    int16_t render(int16_t y, const char* title, Icon icon = Icon::ENVELOPE, bool centered = false);

    // Get height of status bar (for layout calculations)
    uint16_t height() const;

private:
    void drawEnvelopeIcon(int16_t x, int16_t y, uint8_t w, uint8_t h);
    void drawUsersIcon(int16_t x, int16_t y, uint8_t w, uint8_t h);
    void drawGearIcon(int16_t x, int16_t y, uint8_t w, uint8_t h);
    void drawInfoIcon(int16_t x, int16_t y, uint8_t w, uint8_t h);
    void drawMapIcon(int16_t x, int16_t y, uint8_t w, uint8_t h);
    void drawSeparator(int16_t y);

    Buffer* buffer;
    const Layout* layout;
    TextRenderer* textRenderer;

    // Scale for title text
    static constexpr float TITLE_SCALE = 0.78f;
};

} // namespace InkHUD2
