#pragma once

#include "Module.h"
#include "../UI/ContentArea.h"
#include <climits>
#include <cstdint>
#include <string>
#include <vector>

namespace InkHUD2 {

// Forward declaration
class MenuModule;

// Node list filter/sort modes
enum class NodeListView : uint8_t {
    ALL,        // All known nodes
    RECENT,     // Recently heard (sorted by last heard)
    FAVORITES   // User-marked favorites
};

// Node entry
struct NodeEntry {
    static constexpr uint8_t HOPS_UNKNOWN = 255;
    static constexpr int32_t DISTANCE_UNKNOWN = INT32_MIN;

    uint32_t nodeNum = 0;
    char shortName[5] = {0};    // 4 chars + null
    char longName[40] = {0};    // Display name
    int16_t snr = 0;            // Signal-to-noise ratio (dB * 4)
    uint32_t lastHeard = 0;     // Unix timestamp
    bool hasPosition = false;   // Has GPS position
    int32_t latitude = 0;       // Scaled latitude
    int32_t longitude = 0;      // Scaled longitude
    int32_t distanceMeters = DISTANCE_UNKNOWN;  // Distance from us
    uint8_t hopsAway = HOPS_UNKNOWN;            // Number of hops
    uint8_t batteryLevel = 255; // 0-100, 255 = unknown
    bool isFavorite = false;
};

// Node list module - replaces Heard, Recents
class NodeListModule : public Module {
public:
    NodeListModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;
    void onInput(Input input) override;

    // View control
    void setView(NodeListView view);
    NodeListView getView() const { return currentView; }
    void cycleView();

    // Menu integration (long press opens menu)
    void setMenuModule(MenuModule* menu) { menuModule = menu; }

    // Node management
    void updateNode(const NodeEntry& node);
    void removeNode(uint32_t nodeNum);
    void clearNodes();
    void toggleFavorite(uint32_t nodeNum);

private:
    void renderNodeList(RenderContext& ctx, const ContentArea& content);
    void renderNodeRow(RenderContext& ctx, const NodeEntry& node, int16_t y);
    void renderSignalBars(RenderContext& ctx, const Layout* layout, int16_t x, int16_t y, int16_t snr, uint16_t height);
    void hatchRegion(RenderContext& ctx, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t spacing, Color color);
    std::string formatDistance(int32_t meters) const;
    std::string truncateWithEllipsis(const RenderContext& ctx, const std::string& text, uint16_t maxWidth, float scale = 1.0f) const;

    // Font scale - use Layout::smallScale

    std::vector<NodeEntry> sortedNodes() const;

    NodeListView currentView = NodeListView::RECENT;
    std::vector<NodeEntry> nodes;

    // Scroll only (no selection)
    uint16_t scrollOffset = 0;
    uint16_t visibleNodeCount = 0;  // Updated during render

    // Menu module for long press action
    MenuModule* menuModule = nullptr;
};

} // namespace InkHUD2
