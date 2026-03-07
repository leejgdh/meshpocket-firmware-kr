/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "HeaderText.h"
#include "TextUtils.h"
#include <cstdio>
#include <cstring>

namespace InkHUD2 {

HeaderText::HeaderText(Type type, int count) {
    buildTexts(type, count);
}

HeaderText::HeaderText(const char* full, const char* shortVer) {
    if (full) {
        strncpy(fullText, full, sizeof(fullText) - 1);
        fullText[sizeof(fullText) - 1] = '\0';
    }
    if (shortVer) {
        strncpy(shortText, shortVer, sizeof(shortText) - 1);
        shortText[sizeof(shortText) - 1] = '\0';
    } else if (full) {
        // If no short version, use full as short too
        strncpy(shortText, full, sizeof(shortText) - 1);
        shortText[sizeof(shortText) - 1] = '\0';
    }
}

void HeaderText::buildTexts(Type type, int count) {
    const char* plural = (count == 1) ? "" : "s";

    switch (type) {
        case Type::HEARD:
            snprintf(fullText, sizeof(fullText), "Heard: %d node%s", count, plural);
            snprintf(shortText, sizeof(shortText), "%d heard", count);
            break;

        case Type::ALL_NODES:
            snprintf(fullText, sizeof(fullText), "All: %d node%s", count, plural);
            snprintf(shortText, sizeof(shortText), "%d node%s", count, plural);
            break;

        case Type::FAVORITES:
            snprintf(fullText, sizeof(fullText), "Favorites: %d", count);
            snprintf(shortText, sizeof(shortText), "%d fav%s", count, plural);
            break;

        case Type::MAP_NODES:
            snprintf(fullText, sizeof(fullText), "Map: %d node%s", count, plural);
            snprintf(shortText, sizeof(shortText), "%d node%s", count, plural);
            break;

        case Type::MAP_FAVS:
            snprintf(fullText, sizeof(fullText), "Map: %d fav%s", count, plural);
            snprintf(shortText, sizeof(shortText), "%d fav%s", count, plural);
            break;

        case Type::SETTINGS:
            strncpy(fullText, "Map Settings", sizeof(fullText));
            strncpy(shortText, "Settings", sizeof(shortText));
            break;

        case Type::GROUP_CHAT:
            strncpy(fullText, "Group Chat", sizeof(fullText));
            strncpy(shortText, "Group", sizeof(shortText));
            break;

        case Type::DM:
            strncpy(fullText, "Direct Message", sizeof(fullText));
            strncpy(shortText, "DM", sizeof(shortText));
            break;

        case Type::CUSTOM:
        default:
            // Should use the other constructor
            fullText[0] = '\0';
            shortText[0] = '\0';
            break;
    }
}

const char* HeaderText::getText(const RenderContext& ctx, uint16_t maxWidth, float scale) const {
    // Try full text first
    if (TextUtils::fits(ctx, fullText, maxWidth, scale)) {
        return fullText;
    }

    // Try short text
    if (TextUtils::fits(ctx, shortText, maxWidth, scale)) {
        return shortText;
    }

    // Truncate short text
    TextUtils::truncate(ctx, shortText, maxWidth, scale, truncBuf, sizeof(truncBuf));
    return truncBuf;
}

const char* HeaderText::getText(const TextRenderer* renderer, uint16_t maxWidth, float scale) const {
    // Try full text first
    if (TextUtils::fits(renderer, fullText, maxWidth, scale)) {
        return fullText;
    }

    // Try short text
    if (TextUtils::fits(renderer, shortText, maxWidth, scale)) {
        return shortText;
    }

    // Truncate short text
    TextUtils::truncate(renderer, shortText, maxWidth, scale, truncBuf, sizeof(truncBuf));
    return truncBuf;
}

} // namespace InkHUD2
