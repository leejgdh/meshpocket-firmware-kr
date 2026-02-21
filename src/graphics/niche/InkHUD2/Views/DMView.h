#pragma once

#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include "../Text/TextRenderer.h"
#include "../UI/ContentArea.h"
#include <cstdint>

namespace InkHUD2 {

// DMView - renders single message (DM style)
class DMView {
public:
    DMView(Buffer* buffer, const Layout* layout, TextRenderer* text);

    // Render single message in content area
    void render(const ContentArea& area, const char* text);

private:
    Buffer* buffer;
    const Layout* layout;
    TextRenderer* textRenderer;
};

} // namespace InkHUD2
