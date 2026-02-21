/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "NodeListModule.h"
#include "MenuModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/StatusBar.h"
#include "../UI/ContentArea.h"
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

    // Build header text like old InkHUD: "Heard: N nodes"
    std::string title;
    switch (currentView) {
        case NodeListView::ALL: {
            size_t count = nodes.size();
            title = "All: " + std::to_string(count) + (count == 1 ? " node" : " nodes");
            break;
        }
        case NodeListView::RECENT: {
            size_t count = sortedNodes().size();
            title = "Heard: " + std::to_string(count) + (count == 1 ? " node" : " nodes");
            break;
        }
        case NodeListView::FAVORITES: {
            size_t count = 0;
            for (const auto& n : nodes) if (n.isFavorite) count++;
            title = "Favorites: " + std::to_string(count) + (count == 1 ? " node" : " nodes");
            break;
        }
    }

    // === Render StatusBar ===
    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(padding, title.c_str(), StatusBar::Icon::USERS);

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

        case Input::BACK:
            // Long press on NodeList opens menu
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
                result.push_back(n);
                break;
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
            // Sort by last heard (newest first)
            std::sort(result.begin(), result.end(),
                [](const NodeEntry& a, const NodeEntry& b) {
                    return a.lastHeard > b.lastHeard;
                });
            break;

        case NodeListView::FAVORITES:
            // Sort by last heard
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
    uint16_t smallLineH = layout->smallLineHeight();

    // Card height: full line + smaller line
    uint16_t cardH = lineH + smallLineH;
    uint16_t cardMargin = layout->nodeCardMargin();

    int16_t y = content.top();
    auto sorted = sortedNodes();

    if (sorted.empty()) {
        ctx.text(ctx.width() / 2, content.top() + content.h / 2, "No nodes", Align::CENTER, Color::BLACK);
        return;
    }

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
    uint16_t smallLineH = layout->smallLineHeight();

    Color textColor = Color::BLACK;

    // Two-line layout like old InkHUD:
    // Line A (top): shortName left, signal/hops right (full size)
    // Line B (bottom): longName left, distance right (smaller font)

    int16_t lineAY = y;
    int16_t lineBY = y + lineH;

    // Get layout values
    uint16_t textInset = layout->textInset();
    uint16_t nodeTextInset = layout->nodeTextInset();
    uint16_t elemSpacing = layout->elementSpacing();

    // Divider X - where right-side info starts (calculate with small font)
    uint16_t dividerX = ctx.width() - ctx.textWidthScaled("X Hops", Layout::smallScale) - textInset;

    // === Line A: Short name + signal/hops ===
    const char* shortName = (node.shortName[0] != '\0') ? node.shortName : "?";
    ctx.text(nodeTextInset, lineAY, shortName, Align::LEFT, textColor);

    // Right side of Line A: signal indicator (if direct) or hops (smaller font)
    if (node.hopsAway == 0 && node.snr != 0) {
        // Direct connection - draw signal bars (aligned with shortName line)
        int16_t signalX = ctx.width() - layout->signalBarsOffset();
        renderSignalBars(ctx, layout, signalX, lineAY, node.snr, lineH);
    } else if (node.hopsAway != NodeEntry::HOPS_UNKNOWN) {
        // Show hops (smaller font)
        std::string hopStr = std::to_string(node.hopsAway) + " Hop";
        if (node.hopsAway != 1) hopStr += "s";
        ctx.textScaled(ctx.width() - nodeTextInset, lineAY, hopStr.c_str(), Layout::smallScale, Align::RIGHT, textColor);
    }

    // === Line B: Long name + distance (smaller font) ===

    // Distance on right
    std::string distStr;
    if (node.distanceMeters != NodeEntry::DISTANCE_UNKNOWN) {
        distStr = formatDistance(node.distanceMeters);
        ctx.textScaled(ctx.width() - nodeTextInset, lineBY, distStr.c_str(), Layout::smallScale, Align::RIGHT, textColor);
    }

    // Long name on left (truncated with ellipsis if needed)
    std::string longName = (node.longName[0] != '\0') ? node.longName : shortName;
    std::string truncatedName = truncateWithEllipsis(ctx, longName, dividerX - textInset, Layout::smallScale);

    ctx.textScaled(nodeTextInset, lineBY, truncatedName.c_str(), Layout::smallScale, Align::LEFT, textColor);

    // Hatch fade effect for long names (only if truncated)
    if (longName.length() > truncatedName.length() - 3) {  // -3 for "..."
        int16_t hatchLeft = dividerX - smallLineH;
        int16_t hatchWidth = smallLineH;
        hatchRegion(ctx, hatchLeft, lineBY, hatchWidth, smallLineH, elemSpacing, Color::WHITE);
    }
}

void NodeListModule::renderSignalBars(RenderContext& ctx, const Layout* layout, int16_t x, int16_t y, int16_t snr, uint16_t height) {
    if (!layout) return;

    // SNR is in dB * 4, so divide by 4 for actual dB
    // Typical range: -20dB to +10dB
    // Map to 0-4 bars

    int8_t snrDb = snr / 4;
    uint8_t bars;

    if (snrDb < -15) bars = 0;
    else if (snrDb < -10) bars = 1;
    else if (snrDb < -5) bars = 2;
    else if (snrDb < 0) bars = 3;
    else bars = 4;

    uint16_t barW = layout->signalBarWidth();
    uint16_t barSpacing = layout->signalBarSpacing();
    uint16_t maxBarH = height - layout->signalBarPadding();
    uint16_t elemSpacing = layout->elementSpacing();

    // Bars aligned to bottom of the line (y + height)
    for (uint8_t i = 0; i < 4; ++i) {
        uint16_t barH = (maxBarH * (i + 1)) / 4;
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
