#pragma once

#include "Module.h"
#include "../UI/MenuItem.h"
#include "../UI/MenuList.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace InkHUD2 {

// Forward declaration so we don't pull MenuModule.h into this header.
class MenuModule;

// Special channel index for DM. Used as a sentinel by Events.cpp to flag
// that a message is a direct message rather than a broadcast on a channel.
static constexpr uint8_t CHANNEL_DM = 255;

// Single message inside a configured channel's history.
//
// `snr` and `hopsAway` are populated from the originating packet when the
// message is incoming. For outgoing (mirrored) messages they're sentinels
// (snr = 0.0f, hopsAway = HOPS_UNKNOWN) and are not rendered.
struct ChannelMessage {
    static constexpr uint8_t HOPS_UNKNOWN = 255;

    uint32_t from;          // Node number of the sender
    uint32_t timestamp;     // Unix epoch
    uint8_t channel;        // Channel index
    float snr;              // packet->rx_snr (0.0 = unknown / outgoing)
    uint8_t hopsAway;       // packet->hop_start - packet->hop_limit (255 = unknown)
    char text[237];         // UTF-8 text
};

// Single DM message. Direction is encoded by `from`:
//   from == myNodeNum  → outgoing (sent by us)
//   from != myNodeNum  → incoming (received from the peer)
struct DMMessage {
    static constexpr uint8_t HOPS_UNKNOWN = 255;

    uint32_t from;
    uint32_t timestamp;
    float snr;              // 0.0 = unknown / outgoing
    uint8_t hopsAway;       // 255 = unknown
    char text[237];
};

// One DM thread = our conversation history with a single peer.
struct DMThread {
    uint32_t nodeNum;                  // Peer's nodeNum (the thread key)
    bool hasUnread;
    std::deque<DMMessage> messages;    // newest at front
};

// Per-channel state. No longer rendered as a tab strip — this is just the
// per-channel storage container. DMs live in a separate `dmThreads` vector
// because they are per-peer, not per-channel.
struct ChannelTab {
    uint8_t channelIndex;
    bool hasUnread;
    bool chatStyle;
    char name[16];
};

// What the MAIN screen is currently showing.
enum class ViewKind : uint8_t {
    CHANNEL,    // a configured channel's chat
    DM_THREAD,  // a specific peer's DM thread
};

// Message module with channel selection via an internal menu, and a separate
// DM list submenu. Two screen states drive what's drawn:
//
//   MAIN     — content of the current view (channel or DM thread).
//              SHORT_PRESS reserved for a future "send canned message" flow.
//              LONG_PRESS falls through to HUD-level cycle.
//              DOUBLE_TAP enters MENU.
//   MENU     — top-level: Back / DM / configured channels.
//              SHORT_PRESS = next item, LONG_PRESS = activate.
//   DM_LIST  — sub-menu of DM peers: Back / peer A / peer B / ...
//              Same input semantics as MENU.
class MessageModule : public Module {
public:
    MessageModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;
    void onInput(Input input) override;

    // Add an inbound message OR mirror an outbound one. `to` distinguishes
    // direction for DMs (from == myNodeNum means outbound). `snr` and
    // `hopsAway` are extracted from the packet when called from
    // Events.cpp's text-message observer; outbound mirrors pass the
    // unknown sentinels (0.0f / 255) since those signals don't apply to
    // our own sends.
    void setMessage(uint32_t from, uint32_t to, const char* text,
                    uint8_t channel, uint32_t timestamp = 0,
                    float snr = 0.0f, uint8_t hopsAway = 255);

    // Mark a configured channel as read.
    void markChannelAsRead(uint8_t channel);

    // Clear all messages (channel + DM).
    void clear();

    // Configure which channels to track (call at setup).
    void addChannel(uint8_t channelIndex, const char* name, bool useChatStyle = true);

    // Get the channel index of the currently displayed view, or CHANNEL_DM
    // if showing a DM thread.
    uint8_t getCurrentChannel() const;

    // Set own node number for outgoing detection.
    void setMyNodeNum(uint32_t nodeNum) { myNodeNum = nodeNum; }

    // Optional MenuModule reference — used to flash transient alerts
    // ("Position sent.", etc.) from quick-menu actions.
    void setMenuModule(MenuModule* m) { menuModule = m; }

    // Node-name resolvers (provided by Events.cpp via NodeDB).
    using NodeNameCallback = const char* (*)(uint32_t nodeNum);
    void setShortNameCallback(NodeNameCallback cb) { getShortName = cb; }
    void setLongNameCallback(NodeNameCallback cb) { getLongName = cb; }

private:
    enum class State { MAIN, MENU, DM_LIST, QUICK_MENU, CANNED_LIST };

    void switchToChannel(size_t channelTabIndex);
    void switchToDMThread(uint32_t peerNodeNum);

    const char* getChannelName(uint8_t channel) const;
    const char* formatTime(uint32_t timestamp) const;

    // Render entry points per state
    void renderMain(RenderContext& ctx);
    void renderMenu(RenderContext& ctx);
    void renderDMList(RenderContext& ctx);
    void renderQuickMenu(RenderContext& ctx);
    void renderCannedList(RenderContext& ctx);

    // Menu lifecycle (top-level menu)
    void rebuildTopMenuItems();
    void enterMenu();
    void exitMenu();
    void activateSelectedTopMenuItem();

    // DM list lifecycle (sub-menu)
    void rebuildDMListItems();
    void enterDMList();
    void exitDMListToMenu();
    void activateSelectedDMListItem();

    // Quick menu — reached from MAIN via SHORT_PRESS. Hub for
    // contextual actions like "Send Canned" and "Share Position".
    // Items are rebuilt on every entry so they can adapt to the current
    // view (e.g. "Send Canned" only appears when there's a destination).
    void rebuildQuickMenuItems();
    void enterQuickMenu();
    void exitQuickMenu();           // back to MAIN
    void activateSelectedQuickMenuItem();

    // Canned list lifecycle — reached from QUICK_MENU's "Send Canned" item.
    // Destination is implicit from `currentView`. On activate, sends the
    // chosen text and returns to MAIN. "Back" returns to QUICK_MENU.
    void rebuildCannedListItems();
    void enterCannedList();
    void exitCannedList();          // → MAIN (used after a successful send)
    void activateSelectedCannedItem();
    void sendCanned(const std::string& text);

    // Quick-menu actions
    void shareCurrentPosition();
    void requestCurrentPosition();   // DM-only: send ours + ask peer to reply

    // Find or create a DM thread for the given peer. Returns nullptr if
    // peerNodeNum is 0 (invalid). Pointer is stable between additions.
    DMThread* findDMThread(uint32_t peerNodeNum);
    DMThread* findOrCreateDMThread(uint32_t peerNodeNum);

    // === Channels ===
    std::vector<ChannelTab> channels;
    static constexpr size_t MAX_MESSAGES_PER_CHANNEL = 10;
    std::vector<std::deque<ChannelMessage>> channelMessages;

    // === DM ===
    std::vector<DMThread> dmThreads;
    static constexpr size_t MAX_MESSAGES_PER_DM = 20;

    // === Current MAIN view ===
    ViewKind currentView = ViewKind::DM_THREAD;
    size_t currentChannelIndex = 0;   // valid when currentView == CHANNEL
    uint32_t currentDMNodeNum = 0;    // valid when currentView == DM_THREAD; 0 = empty placeholder

    // === Identity / lookups ===
    uint32_t myNodeNum = 0;
    NodeNameCallback getShortName = nullptr;
    NodeNameCallback getLongName = nullptr;
    static const char* defaultNodeName(uint32_t nodeNum);

    // === Menu state ===
    State state = State::MAIN;
    MenuList menuList;

    // Top-level menu (Back / DM / channels)
    std::vector<MenuItem> topMenuItems;
    std::vector<std::string> topMenuLabels;
    std::vector<std::function<void()>> topMenuActions;

    // DM list sub-menu (Back / peer A / peer B / ...)
    std::vector<MenuItem> dmListItems;
    std::vector<std::string> dmListLabels;
    std::vector<std::function<void()>> dmListActions;

    // Quick-menu (Back / Send Canned / Share Position / ...). Items are
    // rebuilt on every entry so context-dependent items (like Send Canned
    // only when a destination exists) can be added/dropped dynamically.
    std::vector<MenuItem> quickMenuItems;
    std::vector<std::string> quickMenuLabels;
    std::vector<std::function<void()>> quickMenuActions;

    // Canned-message sub-menu (Back / canned A / canned B / ...). Source of
    // truth is the global `cannedMessageModuleConfig.messages` ('|'-split);
    // we re-read on every entry so app-side changes appear without us
    // needing to subscribe to config-change events.
    std::vector<MenuItem> cannedListItems;
    std::vector<std::string> cannedListLabels;
    std::vector<std::function<void()>> cannedListActions;

    // Optional reference to the system MenuModule, used to flash transient
    // alerts like "Position sent." Wired by Setup.cpp.
    MenuModule* menuModule = nullptr;
};

} // namespace InkHUD2
