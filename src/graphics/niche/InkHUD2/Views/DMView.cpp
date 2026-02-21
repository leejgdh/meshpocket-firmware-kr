/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "DMView.h"

namespace InkHUD2 {

DMView::DMView(Buffer* buffer, const Layout* layout, TextRenderer* text)
    : buffer(buffer), layout(layout), textRenderer(text) {}

void DMView::render(const ContentArea& area, const char* text) {
    if (!buffer || !layout || !textRenderer || !text) return;

    uint16_t padding = layout->padding();
    uint16_t maxW = area.innerWidth(padding);

    textRenderer->textWrapped(area.innerLeft(padding), area.top(), maxW, text, Color::BLACK);
}

} // namespace InkHUD2
