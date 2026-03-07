/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "DMView.h"
#include <algorithm>

namespace InkHUD2 {

// Scale for elongated screens (~2px smaller than 18px base)
constexpr float ELONGATED_BODY_SCALE = 0.89f;

DMView::DMView(Buffer* buffer, const Layout* layout, TextRenderer* text)
    : buffer(buffer), layout(layout), textRenderer(text) {}

void DMView::render(const ContentArea& area, const char* text) {
    if (!buffer || !layout || !textRenderer || !text) return;

    uint16_t padding = layout->padding();
    uint16_t maxW = area.innerWidth(padding);

    // Detect elongated screen (aspect ratio > 1.5)
    uint16_t maxDim = std::max(buffer->width(), buffer->height());
    uint16_t minDim = std::min(buffer->width(), buffer->height());
    bool isElongated = (minDim > 0) && (maxDim * 10 / minDim > 15);

    // Use truncated version with height limit to prevent overflow
    uint16_t maxH = area.h;

    if (isElongated) {
        textRenderer->textWrappedTruncatedScaled(area.innerLeft(padding), area.top(), maxW, maxH, text, ELONGATED_BODY_SCALE, Color::BLACK);
    } else {
        textRenderer->textWrappedTruncated(area.innerLeft(padding), area.top(), maxW, maxH, text, Color::BLACK);
    }
}

} // namespace InkHUD2
