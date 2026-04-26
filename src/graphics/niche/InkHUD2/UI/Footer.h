#pragma once

#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include "../Text/TextRenderer.h"
#include <cstdint>
#include <vector>

namespace InkHUD2 {

// Tab info for Footer
struct TabInfo {
    const char* name;
    bool isActive;
    bool hasUnread;
};

// Footer - bottom bar with tabs/indicators
class Footer {
public:
    Footer(Buffer* buffer, const Layout* layout, TextRenderer* text);

    // Render tabs at bottom
    // Returns Y position of content bottom (above footer)
    int16_t renderTabs(const std::vector<TabInfo>& tabs, size_t activeIndex, bool drawSeparator = true);

    // Render simple hint text at bottom (for info/alert pages)
    // Returns Y position of content bottom (above hint)
    int16_t renderHint(const char* hint);

    // Get height of footer (derived from layout)
    uint16_t height() const {
        return tabHeight() + tabSpacing();
    }

    // Dynamic values from layout
    uint16_t tabHeight() const {
        return layout ? layout->footerHeight() : 14;
    }

    uint16_t tabSpacing() const {
        return layout ? layout->slotSpacing() : 2;
    }

    uint16_t tabSize() const {
        return layout ? layout->tabSize() : 8;
    }

private:
    void drawDottedLine(int16_t y);

    Buffer* buffer;
    const Layout* layout;
    TextRenderer* textRenderer;
};

} // namespace InkHUD2
