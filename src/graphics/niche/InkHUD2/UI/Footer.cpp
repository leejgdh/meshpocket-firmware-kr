/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Footer.h"
#include "TextUtils.h"
#include <cstdio>
#include <cstring>

namespace InkHUD2 {

Footer::Footer(Buffer* buffer, const Layout* layout, TextRenderer* text)
    : buffer(buffer), layout(layout), textRenderer(text) {}

void Footer::drawDottedLine(int16_t y) {
    if (!buffer) return;
    uint16_t w = buffer->width();
    // Dotted line (every 2 pixels) - same style as StatusBar separator
    for (int16_t x = 0; x < static_cast<int16_t>(w); x += 2) {
        buffer->setPixel(x, y, Color::BLACK);
    }
}

int16_t Footer::renderTabs(const std::vector<TabInfo>& tabs, size_t activeIndex, bool drawSeparator) {
    if (!buffer || !layout || tabs.empty()) return buffer ? buffer->height() : 0;

    uint16_t screenH = buffer->height();
    uint16_t screenW = buffer->width();
    uint16_t padding = layout->padding();

    uint16_t tabH = tabHeight();
    uint16_t tabSp = tabSpacing();

    int16_t tabY = screenH - tabH;
    int16_t contentBottom = tabY - tabSp;

    // Draw separator line above tabs (dotted)
    if (drawSeparator) {
        drawDottedLine(contentBottom + 1);
    }

    // Tab dimensions (dynamic)
    uint16_t tabW = tabSize();
    uint16_t tabGap = layout->tabGap();

    int16_t tabsStartX = padding;

    // Draw tabs
    for (size_t i = 0; i < tabs.size(); ++i) {
        int16_t x = tabsStartX + i * (tabW + tabGap);
        int16_t y = tabY + (tabH - tabW) / 2;

        if (i == activeIndex) {
            // Active tab - filled square
            for (int16_t dy = 0; dy < tabW; dy++) {
                for (int16_t dx = 0; dx < tabW; dx++) {
                    buffer->setPixel(x + dx, y + dy, Color::BLACK);
                }
            }
        } else if (tabs[i].hasUnread) {
            // Has unread - filled circle
            int16_t cx = x + tabW / 2;
            int16_t cy = y + tabW / 2;
            int16_t r = tabW / 2;
            for (int16_t dy = -r; dy <= r; dy++) {
                for (int16_t dx = -r; dx <= r; dx++) {
                    if (dx*dx + dy*dy <= r*r) {
                        buffer->setPixel(cx + dx, cy + dy, Color::BLACK);
                    }
                }
            }
        } else {
            // Inactive - outline square
            for (int16_t dx = 0; dx < tabW; dx++) {
                buffer->setPixel(x + dx, y, Color::BLACK);
                buffer->setPixel(x + dx, y + tabW - 1, Color::BLACK);
            }
            for (int16_t dy = 0; dy < tabW; dy++) {
                buffer->setPixel(x, y + dy, Color::BLACK);
                buffer->setPixel(x + tabW - 1, y + dy, Color::BLACK);
            }
        }
    }

    // Channel name on right (for active tab) - symmetric with tabs on left
    if (activeIndex < tabs.size() && tabs[activeIndex].name && textRenderer) {
        // Align with battery icon right edge
        int16_t nameX = screenW - padding - 2;
        // Center text vertically in tab area
        uint16_t scaledH = static_cast<uint16_t>(textRenderer->lineHeight() * TEXT_SCALE);
        int16_t nameY = tabY + (tabH - scaledH) / 2;

        // Prefix with # or @ based on name
        char displayName[32];
        const char* name = tabs[activeIndex].name;
        char prefix = '#';

        // Check if it's a DM (starts with !)
        if (name[0] == '!') {
            prefix = '@';
            name++;  // Skip the !
        }

        snprintf(displayName, sizeof(displayName), "%c%s", prefix, name);

        // Calculate max width (don't overlap tabs)
        int16_t tabsEndX = tabsStartX + tabs.size() * (tabW + tabGap);
        uint16_t maxNameW = nameX - tabsEndX - padding;

        // Truncate if needed
        char truncBuf[32];
        TextUtils::truncate(textRenderer, displayName, maxNameW, TEXT_SCALE, truncBuf, sizeof(truncBuf));
        textRenderer->textScaled(nameX, nameY, truncBuf, TEXT_SCALE, Align::RIGHT, Color::BLACK);
    }

    return contentBottom;
}

int16_t Footer::renderHint(const char* hint) {
    if (!buffer || !layout || !textRenderer || !hint) {
        return buffer ? buffer->height() : 0;
    }

    uint16_t screenW = buffer->width();
    uint16_t screenH = buffer->height();
    uint16_t hintLineH = layout->hintLineHeight();
    uint16_t spacing = layout->elementSpacing();
    float scale = layout->effectiveHintScale();

    // Check if hint fits on one line
    uint16_t hintWidth = textRenderer->textWidthScaled(hint, scale);
    uint16_t margin = layout->margin();

    if (hintWidth <= screenW - 2 * margin) {
        // Single line
        int16_t hintY = screenH - hintLineH - spacing;
        int16_t centerX = screenW / 2;
        textRenderer->textScaled(centerX, hintY, hint, scale, Align::CENTER, Color::BLACK);
        return hintY - spacing;
    } else {
        // Two lines for vertical mode - split at space before "Hold"
        const char* line1 = "Click: Next";
        const char* line2 = "Hold: Select";
        int16_t line2Y = screenH - hintLineH - spacing;
        int16_t line1Y = line2Y - hintLineH;
        int16_t centerX = screenW / 2;
        textRenderer->textScaled(centerX, line1Y, line1, scale, Align::CENTER, Color::BLACK);
        textRenderer->textScaled(centerX, line2Y, line2, scale, Align::CENTER, Color::BLACK);
        return line1Y - spacing;
    }
}

} // namespace InkHUD2
