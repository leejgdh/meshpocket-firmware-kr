/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "NodeListModule.h"
#include "MenuModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/StatusBar.h"
#include "../UI/ContentArea.h"
#include "../UI/HeaderText.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace InkHUD2 {

NodeListModule::NodeListModule() {
    nodes.reserve(64);
}

void NodeListModule::onRender(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();

    // Create TextRenderer
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    // Build header text with smart truncation
    HeaderText header(HeaderText::Type::CUSTOM);
    switch (currentView) {
        case NodeListView::ALL:
            header = HeaderText(HeaderText::Type::ALL_NODES, static_cast<int>(nodes.size()));
            break;
        case NodeListView::RECENT:
            header = HeaderText(HeaderText::Type::HEARD, static_cast<int>(sortedNodes().size()));
            break;
        case NodeListView::FAVORITES: {
            size_t count = 0;
            for (const auto& n : nodes) if (n.isFavorite) count++;
            header = HeaderText(HeaderText::Type::FAVORITES, static_cast<int>(count));
            break;
        }
    }

    // === Render StatusBar ===
    // Calculate max title width (leave space for battery icon)
    Rect batRect = layout->batteryRect();
    uint16_t iconW = layout->lineHeight();
    uint16_t maxTitleW = batRect.x - layout->margin() - padding - iconW - padding * 2;
    const char* title = header.getText(&textRenderer, maxTitleW, StatusBar::TITLE_SCALE);

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, title, StatusBar::Icon::USERS);

    // === Calculate content area (no footer for NodeList) ===
    int16_t contentBottom = ctx.height();
    ContentArea content = calculateContentArea(layout, contentTop, contentBottom);

    // === Render node list ===
    renderNodeList(ctx, content);
}

void NodeListModule::onEvent(const Event& e) {
    switch (e.type) {
        case EventType::NODE_DISCOVERED:
        case EventType::NODE_UPDATED:
            requestUpdate();
            break;

        case EventType::NODE_LOST:
            removeNode(e.data.node.nodeNum);
            break;

        default:
            break;
    }
}

void NodeListModule::onInput(Input input) {
    switch (input) {
        case Input::LEFT:
        case Input::RIGHT:
            cycleView();
            break;

        case Input::DOWN:
            // Scroll down one node (only if there are hidden nodes below)
            {
                auto sorted = sortedNodes();
                size_t totalNodes = sorted.size();
                // Only scroll if there are more nodes than fit on screen
                if (totalNodes > visibleNodeCount && scrollOffset < totalNodes - visibleNodeCount) {
                    scrollOffset++;
                    requestUpdate();
                }
            }
            break;

        case Input::UP:
            // Return to beginning
            if (scrollOffset > 0) {
                scrollOffset = 0;
                requestUpdate();
            }
            break;

        // LONG_PRESS is handled at the HUD level (cycles to next layout) —
        // we just don't claim it, so it falls through with handlesInput=false.

        case Input::DOUBLE_TAP:
            // Double tap on a module's main screen opens that module's menu.
            // For NodeList the "module menu" is the global system menu.
            if (menuModule) {
                menuModule->open();
            }
            break;

        default:
            break;
    }
}

void NodeListModule::setView(NodeListView view) {
    if (currentView != view) {
        currentView = view;
        scrollOffset = 0;
        requestUpdate();
    }
}

void NodeListModule::cycleView() {
    switch (currentView) {
        case NodeListView::ALL:
            setView(NodeListView::RECENT);
            break;
        case NodeListView::RECENT:
            setView(NodeListView::FAVORITES);
            break;
        case NodeListView::FAVORITES:
            setView(NodeListView::ALL);
            break;
    }
}

void NodeListModule::updateNode(const NodeEntry& node) {
    // Update existing or add new
    for (auto& n : nodes) {
        if (n.nodeNum == node.nodeNum) {
            n = node;
            requestUpdate();
            return;
        }
    }
    nodes.push_back(node);
    requestUpdate();
}

void NodeListModule::removeNode(uint32_t nodeNum) {
    nodes.erase(
        std::remove_if(nodes.begin(), nodes.end(),
            [nodeNum](const NodeEntry& n) { return n.nodeNum == nodeNum; }),
        nodes.end()
    );
    requestUpdate();
}

void NodeListModule::clearNodes() {
    nodes.clear();
    scrollOffset = 0;
    requestUpdate();
}

void NodeListModule::toggleFavorite(uint32_t nodeNum) {
    for (auto& n : nodes) {
        if (n.nodeNum == nodeNum) {
            n.isFavorite = !n.isFavorite;
            requestUpdate();
            return;
        }
    }
}

std::vector<NodeEntry> NodeListModule::sortedNodes() const {
    std::vector<NodeEntry> result;

    // Filter based on view
    for (const auto& n : nodes) {
        switch (currentView) {
            case NodeListView::ALL:
            case NodeListView::RECENT:
                result.push_back(n);
                break;
            case NodeListView::FAVORITES:
                if (n.isFavorite) {
                    result.push_back(n);
                }
                break;
        }
    }

    // Sort
    switch (currentView) {
        case NodeListView::ALL:
            // Sort alphabetically by long name
            std::sort(result.begin(), result.end(),
                [](const NodeEntry& a, const NodeEntry& b) {
                    return strcmp(a.longName, b.longName) < 0;
                });
            break;

        case NodeListView::RECENT:
        case NodeListView::FAVORITES:
            // Sort by last heard (newest first)
            std::sort(result.begin(), result.end(),
                [](const NodeEntry& a, const NodeEntry& b) {
                    return a.lastHeard > b.lastHeard;
                });
            break;
    }

    return result;
}

void NodeListModule::renderNodeList(RenderContext& ctx, const ContentArea& content) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t lineH = layout->lineHeight();

    // Detect elongated screen (aspect ratio > 1.5)
    uint16_t maxDim = std::max(ctx.width(), ctx.height());
    uint16_t minDim = std::min(ctx.width(), ctx.height());
    bool isElongated = (minDim > 0) && (maxDim * 10 / minDim > 15);

    // Card height depends on screen type
    uint16_t shortLineH = isElongated ? layout->smallLineHeight() : lineH;
    uint16_t longLineH = isElongated ? static_cast<uint16_t>(lineH * 0.67f) : layout->smallLineHeight();
    uint16_t cardH = shortLineH + longLineH;
    uint16_t cardMargin = layout->nodeCardMargin();

    int16_t y = content.top();
    auto sorted = sortedNodes();

    if (sorted.empty()) {
        visibleNodeCount = 0;
        ctx.text(ctx.width() / 2, content.top() + content.h / 2, "No nodes", Align::CENTER, Color::BLACK);
        return;
    }

    // Calculate how many nodes fit on screen
    visibleNodeCount = (content.h + cardMargin) / (cardH + cardMargin);

    // Render visible nodes (two lines per node)
    for (size_t i = scrollOffset; i < sorted.size() && y < content.bottom() - cardH; ++i) {
        renderNodeRow(ctx, sorted[i], y);
        y += cardH + cardMargin;
    }
}

void NodeListModule::renderNodeRow(RenderContext& ctx, const NodeEntry& node, int16_t y) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t lineH = layout->lineHeight();

    // Use smaller fonts for elongated screens (aspect ratio > 1.5) to fit more content
    uint16_t maxDim = std::max(ctx.width(), ctx.height());
    uint16_t minDim = std::min(ctx.width(), ctx.height());
    bool isElongated = (minDim > 0) && (maxDim * 10 / minDim > 15);

    // For elongated: shortName uses smallScale, longName uses even smaller
    // For square: shortName full size, longName uses smallScale
    float shortNameScale = isElongated ? Layout::smallScale : 1.0f;
    float longNameScale = isElongated ? 0.67f : Layout::smallScale;  // ~12px equivalent
    uint16_t shortLineH = isElongated ? layout->smallLineHeight() : lineH;
    uint16_t longLineH = static_cast<uint16_t>(lineH * longNameScale);

    Color textColor = Color::BLACK;

    // Two-line layout:
    // Line A (top): shortName left, [hops] + signal bars right
    // Line B (bottom): longName left, distance right

    int16_t lineAY = y;
    int16_t lineBY = y + shortLineH;

    // Get layout values
    uint16_t textInset = layout->textInset();
    uint16_t nodeTextInset = layout->nodeTextInset();
    uint16_t elemSpacing = layout->elementSpacing();

    // === Line A: Short name + [hops] + signal bars ===
    const char* shortName = (node.shortName[0] != '\0') ? node.shortName : "?";
    if (isElongated) {
        ctx.textScaled(nodeTextInset, lineAY, shortName, shortNameScale, Align::LEFT, textColor);
    } else {
        ctx.text(nodeTextInset, lineAY, shortName, Align::LEFT, textColor);
    }

    // Right side of Line A: [hops] + signal bars
    bool hasHops = (node.hopsAway != NodeEntry::HOPS_UNKNOWN && node.hopsAway > 0);

    // Calculate signal bars position (3 bars now)
    uint16_t barW = layout->signalBarWidth();
    uint16_t barSpacing = layout->signalBarSpacing();
    uint16_t barsWidth = 3 * barW + 2 * barSpacing;  // 3 bars
    int16_t signalX = ctx.width() - nodeTextInset - barsWidth;

    // Draw hops before signal bars if present
    if (hasHops) {
        char hopsStr[8];
        snprintf(hopsStr, sizeof(hopsStr), "%dH", node.hopsAway);
        int16_t hopsX = signalX - elemSpacing;
        if (isElongated) {
            ctx.textScaled(hopsX, lineAY, hopsStr, shortNameScale, Align::RIGHT, textColor);
        } else {
            ctx.text(hopsX, lineAY, hopsStr, Align::RIGHT, textColor);
        }
    }

    // Signal bars (if SNR available)
    if (node.snr != 0) {
        renderSignalBars(ctx, layout, signalX, lineAY, node.snr, shortLineH);
    }

    // === Line B: Long name + distance (smaller font) ===
    bool hasDistance = (node.distanceMeters != NodeEntry::DISTANCE_UNKNOWN);

    // Divider X - where right-side info starts (only distance now)
    uint16_t dividerX;
    if (hasDistance) {
        dividerX = ctx.width() - ctx.textWidthScaled("999km", longNameScale) - nodeTextInset;
        ctx.textScaled(ctx.width() - nodeTextInset, lineBY, formatDistance(node.distanceMeters).c_str(),
                       longNameScale, Align::RIGHT, textColor);
    } else {
        dividerX = ctx.width() - nodeTextInset;
    }

    // Long name on left (truncated with ellipsis if needed)
    std::string longName = (node.longName[0] != '\0') ? node.longName : shortName;
    std::string truncatedName = truncateWithEllipsis(ctx, longName, dividerX - nodeTextInset - elemSpacing, longNameScale);

    ctx.textScaled(nodeTextInset, lineBY, truncatedName.c_str(), longNameScale, Align::LEFT, textColor);

    // Hatch fade effect for long names (only if truncated)
    if (longName.length() > truncatedName.length() - 3) {  // -3 for "..."
        int16_t hatchLeft = dividerX - longLineH - elemSpacing;
        int16_t hatchWidth = longLineH;
        hatchRegion(ctx, hatchLeft, lineBY, hatchWidth, longLineH, elemSpacing, Color::WHITE);
    }
}

void NodeListModule::renderSignalBars(RenderContext& ctx, const Layout* layout, int16_t x, int16_t y, int16_t snr, uint16_t height) {
    if (!layout) return;

    // SNR comes as raw dB value (not multiplied)
    // Typical LoRa range: -20dB (barely works) to +10dB (excellent)
    // Map to 0-3 bars: 0=none, 1=weak, 2=good, 3=excellent
    uint8_t bars;

    if (snr < -12) bars = 0;       // Very weak (< -12 dB)
    else if (snr < -5) bars = 1;   // Weak (-12 to -5 dB)
    else if (snr < 3) bars = 2;    // Good (-5 to +3 dB)
    else bars = 3;                  // Excellent (> +3 dB)

    uint16_t barW = layout->signalBarWidth();
    uint16_t barSpacing = layout->signalBarSpacing();
    uint16_t maxBarH = height - layout->signalBarPadding();
    uint16_t elemSpacing = layout->elementSpacing();

    // 3 bars aligned to bottom of the line (y + height)
    for (uint8_t i = 0; i < 3; ++i) {
        uint16_t barH = (maxBarH * (i + 1)) / 3;
        int16_t barX = x + i * (barW + barSpacing);
        int16_t barY = y + height - elemSpacing - barH;

        if (i < bars) {
            ctx.fillRect(barX, barY, barW, barH, Color::BLACK);
        } else {
            ctx.rect(barX, barY, barW, barH, Color::BLACK);
        }
    }
}

std::string NodeListModule::formatDistance(int32_t meters) const {
    if (meters < 1000) {
        return std::to_string(meters) + "m";
    } else {
        int km = meters / 1000;
        return std::to_string(km) + "km";
    }
}

std::string NodeListModule::truncateWithEllipsis(const RenderContext& ctx, const std::string& text, uint16_t maxWidth, float scale) const {
    if (ctx.textWidthScaled(text.c_str(), scale) <= maxWidth) {
        return text;
    }

    std::string result = text;
    const char* ellipsis = "...";
    uint16_t ellipsisWidth = ctx.textWidthScaled(ellipsis, scale);

    while (!result.empty() && ctx.textWidthScaled(result.c_str(), scale) + ellipsisWidth > maxWidth) {
        // Handle UTF-8: check if last byte is continuation byte
        size_t len = result.length();
        while (len > 0 && (result[len - 1] & 0xC0) == 0x80) {
            len--;
        }
        if (len > 0) len--;  // Remove the start byte
        result = result.substr(0, len);
    }

    return result + ellipsis;
}

void NodeListModule::hatchRegion(RenderContext& ctx, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t spacing, Color color) {
    // Draw diagonal hatch lines for fade effect
    for (int16_t px = x; px < x + (int16_t)w; px++) {
        for (int16_t py = y; py < y + (int16_t)h; py++) {
            // Checkerboard pattern based on position
            if ((px + py) % (spacing * 2) < spacing) {
                ctx.pixel(px, py, color);
            }
        }
    }
}

} // namespace InkHUD2
