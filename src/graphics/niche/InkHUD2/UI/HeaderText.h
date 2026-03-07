#pragma once

#include "../Core/RenderContext.h"
#include "../Text/TextRenderer.h"
#include <cstdint>

namespace InkHUD2 {

// Smart header text that adapts to screen width
// Tries full text first, then short version, then truncates
class HeaderText {
public:
    // Predefined header types with full/short variants
    enum class Type {
        HEARD,       // "Heard: X nodes" / "X heard"
        ALL_NODES,   // "All: X nodes" / "X nodes"
        FAVORITES,   // "Favorites: X" / "X favs"
        MAP_NODES,   // "Map: X nodes" / "X nodes"
        MAP_FAVS,    // "Map: X favs" / "X favs"
        SETTINGS,    // "Map Settings" / "Settings"
        GROUP_CHAT,  // "Group Chat" / "Group"
        DM,          // "Direct Message" / "DM"
        CUSTOM       // User-provided full/short text
    };

    // Create with predefined type and count
    HeaderText(Type type, int count = 0);

    // Create with custom full/short text
    HeaderText(const char* fullText, const char* shortText);

    // Get the best fitting text for given width
    // Returns pointer to internal buffer - valid until next call
    const char* getText(const RenderContext& ctx, uint16_t maxWidth, float scale) const;
    const char* getText(const TextRenderer* renderer, uint16_t maxWidth, float scale) const;

private:
    void buildTexts(Type type, int count);

    char fullText[48] = {0};
    char shortText[32] = {0};
    mutable char truncBuf[48] = {0};  // For truncated result
};

} // namespace InkHUD2
