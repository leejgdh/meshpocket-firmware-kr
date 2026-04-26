/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "MessageModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/StatusBar.h"
#include "../UI/ContentArea.h"
#include "../Views/ChatView.h"

#include "configuration.h"

// Send-canned-message support uses the standard mesh routing globals.
// They're available regardless of HAS_SCREEN.
#include "MeshService.h"
#include "Channels.h"
#include "NodeDB.h"
#include "Router.h"
#include "modules/PositionModule.h"
#include "MenuModule.h"

// Canned-message storage. Two backends, picked at compile time:
//
//   - HAS_SCREEN builds use upstream CannedMessageModule's flat protobuf
//     (cannedMessageModuleConfig.messages) which we split here on demand.
//   - Niche-graphics builds (InkHUD/InkHUD2) suppress upstream Screen.cpp
//     and with it the CannedMessageModule. That build path uses the
//     niche-graphics CannedMessageStore singleton, which mirrors the same
//     on-disk file but parses into a vector<string> and owns the admin
//     handler so app/CLI reads-and-writes work without the upstream module.
#if HAS_SCREEN
#include "mesh/generated/meshtastic/cannedmessages.pb.h"
extern meshtastic_CannedMessageModuleConfig cannedMessageModuleConfig;
#elif defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)
#include "graphics/niche/Utils/CannedMessageStore.h"
#endif

#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace InkHUD2 {

// ======================================================================
// Static helpers
// ======================================================================

// Used by ChatView's time-formatter callback. Pinned during render so the
// static formatter can reach this module's myNodeNum / millis() context.
static const MessageModule* g_currentModule = nullptr;

static const char* staticFormatTime(uint32_t timestamp) {
    if (g_currentModule) {
        if (timestamp == 0) return "";
        static char buf[16];
        uint32_t nowSec = millis() / 1000;
        uint32_t ageSec = (nowSec > timestamp) ? (nowSec - timestamp) : 0;
        if (ageSec < 60) {
            snprintf(buf, sizeof(buf), "%lus", (unsigned long)ageSec);
        } else if (ageSec < 3600) {
            snprintf(buf, sizeof(buf), "%lum", (unsigned long)(ageSec / 60));
        } else if (ageSec < 86400) {
            snprintf(buf, sizeof(buf), "%luh", (unsigned long)(ageSec / 3600));
        } else {
            snprintf(buf, sizeof(buf), "%lud", (unsigned long)(ageSec / 86400));
        }
        return buf;
    }
    return "";
}

// ======================================================================
// Construction
// ======================================================================

MessageModule::MessageModule() {
    channels.reserve(8);
    channelMessages.reserve(8);
    dmThreads.reserve(8);
}

// ======================================================================
// Render
// ======================================================================

void MessageModule::onRender(RenderContext& ctx) {
    switch (state) {
        case State::MENU:        renderMenu(ctx);        break;
        case State::DM_LIST:     renderDMList(ctx);      break;
        case State::QUICK_MENU:  renderQuickMenu(ctx);   break;
        case State::CANNED_LIST: renderCannedList(ctx);  break;
        case State::MAIN:
        default:                 renderMain(ctx);        break;
    }
}

void MessageModule::renderMain(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;
    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    g_currentModule = this;

    uint16_t padding = layout->padding();

    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    StatusBar statusBar(buffer, layout, &textRenderer);

    // Resolve title and locate the message source for the active view.
    static char titleBuf[40];
    const char* title = "Messages";
    const std::deque<ChannelMessage>* channelMsgs = nullptr;
    const std::deque<DMMessage>* dmMsgs = nullptr;
    bool isChatStyle = true;

    if (currentView == ViewKind::CHANNEL) {
        if (currentChannelIndex < channels.size()) {
            const char* chName = channels[currentChannelIndex].name;
            if (!chName || !*chName) chName = "Channel";
            strncpy(titleBuf, chName, sizeof(titleBuf) - 1);
            titleBuf[sizeof(titleBuf) - 1] = '\0';
            title = titleBuf;
            isChatStyle = channels[currentChannelIndex].chatStyle;
            if (currentChannelIndex < channelMessages.size()) {
                channelMsgs = &channelMessages[currentChannelIndex];
            }
        }
    } else { // DM_THREAD
        DMThread* th = (currentDMNodeNum != 0) ? findDMThread(currentDMNodeNum) : nullptr;
        const char* shortName = nullptr;
        if (currentDMNodeNum != 0 && getShortName) {
            shortName = getShortName(currentDMNodeNum);
        } else if (currentDMNodeNum != 0) {
            shortName = defaultNodeName(currentDMNodeNum);
        }
        if (shortName && *shortName) {
            snprintf(titleBuf, sizeof(titleBuf), "DM: %s", shortName);
        } else {
            snprintf(titleBuf, sizeof(titleBuf), "DM");
        }
        title = titleBuf;
        if (th) {
            dmMsgs = &th->messages;
        }
    }

    int16_t contentTop = statusBar.render(layout->margin() + padding, title, StatusBar::Icon::ENVELOPE);
    int16_t contentBottom = ctx.height();
    ContentArea content = calculateContentArea(layout, contentTop, contentBottom);

    // Empty states.
    if (currentView == ViewKind::DM_THREAD && (currentDMNodeNum == 0 || !dmMsgs || dmMsgs->empty())) {
        textRenderer.text(ctx.width() / 2, content.top() + content.h / 2,
                         "No DM Yet", Align::CENTER, Color::BLACK);
        return;
    }
    if (currentView == ViewKind::CHANNEL && (!channelMsgs || channelMsgs->empty())) {
        textRenderer.text(ctx.width() / 2, content.top() + content.h / 2,
                         "No Messages", Align::CENTER, Color::BLACK);
        return;
    }

    // Render via ChatView. ChatView already supports own-vs-other rendering
    // by comparing each message's `from` against the configured myNodeNum.
    ChatView chatView(buffer, layout, &textRenderer);
    chatView.setCallbacks(
        getShortName ? getShortName : defaultNodeName,
        staticFormatTime,
        myNodeNum
    );

    std::vector<ChatMessage> view;
    if (channelMsgs) {
        view.reserve(channelMsgs->size());
        for (const auto& m : *channelMsgs) {
            view.push_back({m.from, m.timestamp, m.text});
        }
    } else if (dmMsgs) {
        view.reserve(dmMsgs->size());
        for (const auto& m : *dmMsgs) {
            view.push_back({m.from, m.timestamp, m.text});
        }
    }
    (void)isChatStyle; // ChatView always renders threaded; chatStyle preserved for future
    chatView.render(content, view);
}

void MessageModule::renderMenu(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;
    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, "Channels", StatusBar::Icon::ENVELOPE);
    int16_t contentBottom = ctx.height();
    uint16_t margin = layout->margin();
    menuList.render(ctx, contentTop + padding, contentBottom - padding, margin);
}

void MessageModule::renderDMList(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;
    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, "DM List", StatusBar::Icon::ENVELOPE);
    int16_t contentBottom = ctx.height();
    uint16_t margin = layout->margin();

    // Always render the menu (which has at least the Back item). When there
    // are no DM threads, also paint a "No DM Yet" hint above the Back row.
    if (dmThreads.empty()) {
        textRenderer.text(ctx.width() / 2,
                          contentTop + (contentBottom - contentTop) / 3,
                          "No DM Yet", Align::CENTER, Color::BLACK);
    }
    menuList.render(ctx, contentTop + padding, contentBottom - padding, margin);
}

void MessageModule::renderCannedList(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;
    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    // Title encodes the destination so the user can confirm where the
    // canned will go before pressing long-press to send.
    static char cannedTitleBuf[40];
    if (currentView == ViewKind::CHANNEL && currentChannelIndex < channels.size()) {
        const char* chName = channels[currentChannelIndex].name;
        if (!chName || !*chName) chName = "Channel";
        snprintf(cannedTitleBuf, sizeof(cannedTitleBuf), "Send -> #%s", chName);
    } else if (currentView == ViewKind::DM_THREAD && currentDMNodeNum != 0) {
        const char* shortName = getShortName ? getShortName(currentDMNodeNum) : defaultNodeName(currentDMNodeNum);
        if (!shortName || !*shortName) shortName = "?";
        snprintf(cannedTitleBuf, sizeof(cannedTitleBuf), "Send -> @%s", shortName);
    } else {
        snprintf(cannedTitleBuf, sizeof(cannedTitleBuf), "Send");
    }

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, cannedTitleBuf, StatusBar::Icon::ENVELOPE);
    int16_t contentBottom = ctx.height();
    uint16_t margin = layout->margin();

    // When the canned config has no messages, hint the user to add some.
    // cannedListItems.size() == 1 means only Back is present.
    if (cannedListItems.size() <= 1) {
        textRenderer.text(ctx.width() / 2,
                          contentTop + (contentBottom - contentTop) / 3,
                          "No canned messages", Align::CENTER, Color::BLACK);
    }
    menuList.render(ctx, contentTop + padding, contentBottom - padding, margin);
}

// ======================================================================
// Events
// ======================================================================

void MessageModule::onEvent(const Event& e) {
    if (e.type == EventType::MESSAGE_RECEIVED) {
        requestUpdate();
        requestAutoshow();
    }
}

// ======================================================================
// Input
// ======================================================================

void MessageModule::onInput(Input input) {
    if (state == State::MENU) {
        switch (input) {
            case Input::SHORT_PRESS:
                menuList.selectNext();
                requestUpdate();
                break;
            case Input::LONG_PRESS:
                activateSelectedTopMenuItem();
                break;
            default:
                break;
        }
        return;
    }

    if (state == State::DM_LIST) {
        switch (input) {
            case Input::SHORT_PRESS:
                menuList.selectNext();
                requestUpdate();
                break;
            case Input::LONG_PRESS:
                activateSelectedDMListItem();
                break;
            default:
                break;
        }
        return;
    }

    if (state == State::QUICK_MENU) {
        switch (input) {
            case Input::SHORT_PRESS:
                menuList.selectNext();
                requestUpdate();
                break;
            case Input::LONG_PRESS:
                activateSelectedQuickMenuItem();
                break;
            default:
                break;
        }
        return;
    }

    if (state == State::CANNED_LIST) {
        switch (input) {
            case Input::SHORT_PRESS:
                menuList.selectNext();
                requestUpdate();
                break;
            case Input::LONG_PRESS:
                activateSelectedCannedItem();
                break;
            default:
                break;
        }
        return;
    }

    // MAIN
    switch (input) {
        case Input::SHORT_PRESS:
            // Open the quick menu (Send Canned / Share Position / ...).
            // Available items are computed at entry time based on context,
            // so this is always reachable even when there's no active
            // destination (Share Position works regardless).
            enterQuickMenu();
            break;
        // LONG_PRESS falls through to the HUD-level handler (cycle layout).
        case Input::DOUBLE_TAP:
            enterMenu();
            break;
        default:
            break;
    }
}

// ======================================================================
// View transitions
// ======================================================================

void MessageModule::switchToChannel(size_t channelTabIndex) {
    if (channelTabIndex >= channels.size()) return;
    currentView = ViewKind::CHANNEL;
    currentChannelIndex = channelTabIndex;
    channels[channelTabIndex].hasUnread = false;
    requestUpdate();
}

void MessageModule::switchToDMThread(uint32_t peerNodeNum) {
    if (peerNodeNum == 0) return;
    currentView = ViewKind::DM_THREAD;
    currentDMNodeNum = peerNodeNum;
    if (DMThread* th = findDMThread(peerNodeNum)) {
        th->hasUnread = false;
    }
    requestUpdate();
}

// ======================================================================
// Top-level menu (Back / DM / channels)
// ======================================================================

void MessageModule::rebuildTopMenuItems() {
    topMenuItems.clear();
    topMenuLabels.clear();
    topMenuActions.clear();

    size_t total = 2 /*Back + DM*/ + channels.size();
    topMenuLabels.reserve(total);
    topMenuActions.reserve(total);
    topMenuItems.reserve(total);

    // 1) Back
    topMenuLabels.emplace_back("< Back");
    {
        MenuItem item;
        item.label = topMenuLabels.back().c_str();
        item.type = MenuItemType::BACK;
        topMenuItems.push_back(item);
    }

    // 2) DM (always present, opens DM list — even when empty)
    topMenuLabels.emplace_back("DM");
    topMenuActions.emplace_back([this]() { this->enterDMList(); });
    // We add the MenuItem entry below (after all label/action pushes) so
    // pointers into the vectors are stable.

    // 3) Configured channels
    for (size_t i = 0; i < channels.size(); ++i) {
        const char* name = channels[i].name;
        topMenuLabels.emplace_back((name && *name) ? name : "Channel");
        size_t idx = i;
        topMenuActions.emplace_back([this, idx]() {
            this->switchToChannel(idx);
            this->exitMenu();
        });
    }

    // Now build the MenuItem entries pointing into stable storage.
    // DM item — index 1 in labels, index 0 in actions.
    {
        MenuItem item;
        item.label = topMenuLabels[1].c_str();
        item.type = MenuItemType::ACTION;
        item.action = &topMenuActions[0];
        topMenuItems.push_back(item);
    }
    // Channel items — labels[2..], actions[1..].
    for (size_t i = 0; i < channels.size(); ++i) {
        MenuItem item;
        item.label = topMenuLabels[2 + i].c_str();
        item.type = MenuItemType::ACTION;
        item.action = &topMenuActions[1 + i];
        topMenuItems.push_back(item);
    }
}

void MessageModule::enterMenu() {
    rebuildTopMenuItems();
    menuList.setItems(topMenuItems.data(), static_cast<uint8_t>(topMenuItems.size()));
    // Highlight the row that matches the current view, if any.
    if (currentView == ViewKind::CHANNEL && currentChannelIndex < channels.size()) {
        // Channel rows start at index 2 (after Back, DM)
        menuList.setSelectedIndex(static_cast<uint8_t>(2 + currentChannelIndex));
    } else if (currentView == ViewKind::DM_THREAD) {
        menuList.setSelectedIndex(1); // "DM"
    } else {
        menuList.setSelectedIndex(0);
    }
    state = State::MENU;
    handlesInput = true;
    requestUpdate();
}

void MessageModule::exitMenu() {
    state = State::MAIN;
    handlesInput = false;
    requestUpdate();
}

void MessageModule::activateSelectedTopMenuItem() {
    MenuItem* item = menuList.getSelectedItem();
    if (!item) return;
    switch (item->type) {
        case MenuItemType::BACK:
            exitMenu();
            break;
        case MenuItemType::ACTION:
            if (item->action && *item->action) {
                (*item->action)();
            }
            break;
        default:
            break;
    }
}

// ======================================================================
// DM list sub-menu
// ======================================================================

void MessageModule::rebuildDMListItems() {
    dmListItems.clear();
    dmListLabels.clear();
    dmListActions.clear();

    size_t total = 1 /*Back*/ + dmThreads.size();
    dmListLabels.reserve(total);
    dmListActions.reserve(dmThreads.size());
    dmListItems.reserve(total);

    // Back item
    dmListLabels.emplace_back("< Back");
    {
        MenuItem item;
        item.label = dmListLabels.back().c_str();
        item.type = MenuItemType::BACK;
        dmListItems.push_back(item);
    }

    // Per-peer entries. Labels include a leading "* " when the thread has
    // unread messages — keeps the unread cue visible without needing custom
    // glyph drawing in MenuList.
    for (size_t i = 0; i < dmThreads.size(); ++i) {
        const DMThread& th = dmThreads[i];
        const char* shortName = getShortName ? getShortName(th.nodeNum) : defaultNodeName(th.nodeNum);
        if (!shortName || !*shortName) shortName = "?";

        std::string label;
        label.reserve(8 + strlen(shortName));
        label += th.hasUnread ? "* " : "  ";
        label += shortName;
        dmListLabels.emplace_back(std::move(label));

        uint32_t peer = th.nodeNum;
        dmListActions.emplace_back([this, peer]() {
            this->switchToDMThread(peer);
            this->exitMenu();   // DM_THREAD shows on MAIN; bring us back there
        });
    }

    for (size_t i = 0; i < dmThreads.size(); ++i) {
        MenuItem item;
        item.label = dmListLabels[1 + i].c_str();
        item.type = MenuItemType::ACTION;
        item.action = &dmListActions[i];
        dmListItems.push_back(item);
    }
}

void MessageModule::enterDMList() {
    rebuildDMListItems();
    menuList.setItems(dmListItems.data(), static_cast<uint8_t>(dmListItems.size()));

    // If the user is currently viewing a DM thread, pre-highlight that peer.
    uint8_t highlight = 0;
    if (currentView == ViewKind::DM_THREAD && currentDMNodeNum != 0) {
        for (size_t i = 0; i < dmThreads.size(); ++i) {
            if (dmThreads[i].nodeNum == currentDMNodeNum) {
                highlight = static_cast<uint8_t>(1 + i);
                break;
            }
        }
    }
    menuList.setSelectedIndex(highlight);

    state = State::DM_LIST;
    handlesInput = true;
    requestUpdate();
}

void MessageModule::exitDMListToMenu() {
    enterMenu();   // rebuilds top menu and switches state back to MENU
}

void MessageModule::activateSelectedDMListItem() {
    MenuItem* item = menuList.getSelectedItem();
    if (!item) return;
    switch (item->type) {
        case MenuItemType::BACK:
            exitDMListToMenu();
            break;
        case MenuItemType::ACTION:
            if (item->action && *item->action) {
                (*item->action)();
            }
            break;
        default:
            break;
    }
}

// ======================================================================
// Quick menu (parent of Send Canned / Share Position / ...)
// ======================================================================

void MessageModule::rebuildQuickMenuItems() {
    quickMenuItems.clear();
    quickMenuLabels.clear();
    quickMenuActions.clear();

    // Decide which items to expose, based on the current MAIN view.
    bool haveTarget =
        (currentView == ViewKind::CHANNEL && currentChannelIndex < channels.size()) ||
        (currentView == ViewKind::DM_THREAD && currentDMNodeNum != 0);

    // Reserve generously to keep the storage stable while we wire pointers.
    size_t maxItems = 1 /*Back*/ + 2 /*Send Canned + Share Position*/;
    quickMenuLabels.reserve(maxItems);
    quickMenuActions.reserve(maxItems);
    quickMenuItems.reserve(maxItems);

    // Back
    quickMenuLabels.emplace_back("< Back");
    {
        MenuItem item;
        item.label = quickMenuLabels.back().c_str();
        item.type = MenuItemType::BACK;
        quickMenuItems.push_back(item);
    }

    // Send Canned — only when the current view has an addressable target.
    if (haveTarget) {
        quickMenuLabels.emplace_back("Send Canned");
        quickMenuActions.emplace_back([this]() { this->enterCannedList(); });
    }

    // Share Position — always available; defaults to broadcast on primary
    // when the current view doesn't suggest a different destination.
    quickMenuLabels.emplace_back("Share Position");
    quickMenuActions.emplace_back([this]() {
        this->shareCurrentPosition();
        this->exitQuickMenu();
    });

    // Build MenuItems pointing into stable storage above.
    for (size_t i = 0; i < quickMenuActions.size(); ++i) {
        MenuItem item;
        item.label = quickMenuLabels[1 + i].c_str();
        item.type = MenuItemType::ACTION;
        item.action = &quickMenuActions[i];
        quickMenuItems.push_back(item);
    }
}

void MessageModule::enterQuickMenu() {
    rebuildQuickMenuItems();
    menuList.setItems(quickMenuItems.data(), static_cast<uint8_t>(quickMenuItems.size()));
    // Default-highlight the first action item (after Back) so a quick
    // long-press triggers the most obvious action without extra navigation.
    if (quickMenuItems.size() > 1) {
        menuList.setSelectedIndex(1);
    } else {
        menuList.setSelectedIndex(0);
    }
    state = State::QUICK_MENU;
    handlesInput = true;
    requestUpdate();
}

void MessageModule::exitQuickMenu() {
    state = State::MAIN;
    handlesInput = false;
    requestUpdate();
}

void MessageModule::activateSelectedQuickMenuItem() {
    MenuItem* item = menuList.getSelectedItem();
    if (!item) return;
    switch (item->type) {
        case MenuItemType::BACK:
            exitQuickMenu();
            break;
        case MenuItemType::ACTION:
            if (item->action && *item->action) {
                (*item->action)();
            }
            break;
        default:
            break;
    }
}

void MessageModule::shareCurrentPosition() {
    if (!positionModule) return;
    // Targeting strategy:
    //   - DM_THREAD with a real peer → send a position packet to that peer.
    //     PositionModule honors the dest argument and runs PKI when the
    //     peer has a public key.
    //   - CHANNEL view → broadcast on that channel.
    //   - Otherwise → default broadcast (matches "Ping" menu item).
    if (currentView == ViewKind::DM_THREAD && currentDMNodeNum != 0) {
        positionModule->sendOurPosition(currentDMNodeNum, false, 0);
    } else if (currentView == ViewKind::CHANNEL && currentChannelIndex < channels.size()) {
        positionModule->sendOurPosition(NODENUM_BROADCAST, false,
                                        channels[currentChannelIndex].channelIndex);
    } else {
        positionModule->sendOurPosition();
    }

    if (menuModule) {
        menuModule->showAlert("Position sent.");
    }
}

void MessageModule::renderQuickMenu(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;
    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, "Quick", StatusBar::Icon::ENVELOPE);
    int16_t contentBottom = ctx.height();
    uint16_t margin = layout->margin();
    menuList.render(ctx, contentTop + padding, contentBottom - padding, margin);
}

// ======================================================================
// Canned list sub-menu
// ======================================================================

void MessageModule::rebuildCannedListItems() {
    cannedListItems.clear();
    cannedListLabels.clear();
    cannedListActions.clear();

    // Collect canned messages from whichever backend is active for this
    // build. On every menu entry so app-side edits propagate without us
    // having to observe config-change events.
    std::vector<std::string> messages;
#if HAS_SCREEN
    const char* raw = cannedMessageModuleConfig.messages;
    if (raw && raw[0] != '\0') {
        const char* start = raw;
        for (const char* p = raw; ; ++p) {
            if (*p == '|' || *p == '\0') {
                if (p > start) messages.emplace_back(start, p - start);
                if (*p == '\0') break;
                start = p + 1;
            }
        }
    }
#elif defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)
    // CannedMessageStore is a singleton populated at boot from
    // /prefs/cannedConf.proto and kept up-to-date by an AdminModule
    // observer. It already exposes the parsed vector for us.
    auto* store = NicheGraphics::CannedMessageStore::getInstance();
    if (store) {
        uint8_t n = store->size();
        messages.reserve(n);
        for (uint8_t i = 0; i < n; ++i) {
            messages.emplace_back(store->at(i));
        }
    }
#endif

    size_t total = 1 /*Back*/ + messages.size();
    cannedListLabels.reserve(total);
    cannedListActions.reserve(messages.size());
    cannedListItems.reserve(total);

    // Back item
    cannedListLabels.emplace_back("< Back");
    {
        MenuItem item;
        item.label = cannedListLabels.back().c_str();
        item.type = MenuItemType::BACK;
        cannedListItems.push_back(item);
    }

    for (size_t i = 0; i < messages.size(); ++i) {
        cannedListLabels.emplace_back(messages[i]);
        std::string text = messages[i]; // captured by value
        cannedListActions.emplace_back([this, text]() {
            this->sendCanned(text);
            this->exitCannedList();
        });
    }

    for (size_t i = 0; i < messages.size(); ++i) {
        MenuItem item;
        item.label = cannedListLabels[1 + i].c_str();
        item.type = MenuItemType::ACTION;
        item.action = &cannedListActions[i];
        cannedListItems.push_back(item);
    }
}

void MessageModule::enterCannedList() {
    rebuildCannedListItems();
    menuList.setItems(cannedListItems.data(), static_cast<uint8_t>(cannedListItems.size()));
    // Default-highlight the first canned message (after Back) so a quick
    // long-press sends the topmost canned without extra navigation.
    if (cannedListItems.size() > 1) {
        menuList.setSelectedIndex(1);
    } else {
        menuList.setSelectedIndex(0);
    }
    state = State::CANNED_LIST;
    handlesInput = true;
    requestUpdate();
}

void MessageModule::exitCannedList() {
    state = State::MAIN;
    handlesInput = false;
    requestUpdate();
}

void MessageModule::activateSelectedCannedItem() {
    MenuItem* item = menuList.getSelectedItem();
    if (!item) return;
    switch (item->type) {
        case MenuItemType::BACK:
            // Parent of CANNED_LIST is QUICK_MENU — go back there, not all
            // the way to MAIN. Successful sends still reach MAIN via the
            // ACTION lambdas (which call exitCannedList → state=MAIN).
            enterQuickMenu();
            break;
        case MenuItemType::ACTION:
            if (item->action && *item->action) {
                (*item->action)();
            }
            break;
        default:
            break;
    }
}

void MessageModule::sendCanned(const std::string& text) {
    if (text.empty()) return;
    if (!router || !service) return;

    NodeNum dest;
    ChannelIndex chIdx;

    if (currentView == ViewKind::CHANNEL && currentChannelIndex < channels.size()) {
        dest = NODENUM_BROADCAST;
        chIdx = channels[currentChannelIndex].channelIndex;
    } else if (currentView == ViewKind::DM_THREAD && currentDMNodeNum != 0) {
        dest = currentDMNodeNum;
        chIdx = 0;  // PKI for DM, channel index ignored when pki_encrypted=true
    } else {
        return;  // No destination — should not normally hit this path.
    }

    meshtastic_MeshPacket* p = router->allocForSending();
    if (!p) return;

    p->to = dest;
    p->channel = chIdx;
    p->want_ack = true;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.dest = dest;

    // PKI encryption for DM, mirroring the upstream CannedMessageModule
    // pattern: if the peer has a 32-byte public key, encrypt and force
    // channel 0.
    if (dest != NODENUM_BROADCAST) {
        meshtastic_NodeInfoLite* node = nodeDB->getMeshNode(dest);
        if (node && node->num != nodeDB->getNodeNum() &&
            node->has_user && node->user.public_key.size == 32) {
            p->pki_encrypted = true;
            p->channel = 0;
        }
    }

    size_t len = text.size();
    if (len > sizeof(p->decoded.payload.bytes)) {
        len = sizeof(p->decoded.payload.bytes);
    }
    memcpy(p->decoded.payload.bytes, text.data(), len);
    p->decoded.payload.size = len;

    // ccToPhone=true so the app mirrors the sent message in its UI.
    // sendToMesh also fires the textMessageModule->notifyOutgoing hook,
    // which routes through Events::onReceiveTextMessage and lands back in
    // setMessage(...) — that triggers autoshow on the same thread, which
    // is exactly the "send and bounce back to MAIN of the same thread"
    // behavior we want.
    service->sendToMesh(p, RX_SRC_LOCAL, true);
}

// ======================================================================
// DM thread storage
// ======================================================================

DMThread* MessageModule::findDMThread(uint32_t peerNodeNum) {
    for (auto& th : dmThreads) {
        if (th.nodeNum == peerNodeNum) return &th;
    }
    return nullptr;
}

DMThread* MessageModule::findOrCreateDMThread(uint32_t peerNodeNum) {
    if (peerNodeNum == 0) return nullptr;
    if (DMThread* existing = findDMThread(peerNodeNum)) return existing;
    DMThread th;
    th.nodeNum = peerNodeNum;
    th.hasUnread = false;
    dmThreads.push_back(std::move(th));
    return &dmThreads.back();
}

// ======================================================================
// Message ingestion
// ======================================================================

void MessageModule::setMessage(uint32_t from, uint32_t to, const char* text,
                               uint8_t channel, uint32_t timestamp) {
    if (!text) return;
    uint32_t ts = timestamp ? timestamp : (millis() / 1000);

    if (channel == CHANNEL_DM) {
        // Pick the peer side of the conversation as the thread key.
        uint32_t peer = (from == myNodeNum) ? to : from;
        DMThread* th = findOrCreateDMThread(peer);
        if (!th) return;

        DMMessage m;
        m.from = from;
        m.timestamp = ts;
        strncpy(m.text, text, sizeof(m.text) - 1);
        m.text[sizeof(m.text) - 1] = '\0';
        th->messages.push_front(m);
        while (th->messages.size() > MAX_MESSAGES_PER_DM) th->messages.pop_back();

        // Mark unread only for inbound messages that aren't currently focused.
        bool isOutbound = (from == myNodeNum);
        if (!isOutbound &&
            !(currentView == ViewKind::DM_THREAD && currentDMNodeNum == peer)) {
            th->hasUnread = true;
        }

        // Auto-show the relevant thread when we're in MAIN. Don't yank focus
        // away from a menu the user has open.
        if (state == State::MAIN) {
            currentView = ViewKind::DM_THREAD;
            currentDMNodeNum = peer;
            th->hasUnread = false;
        }

        requestUpdate();
        requestAutoshow();
        return;
    }

    // Configured channel.
    size_t tabIndex = SIZE_MAX;
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].channelIndex == channel) { tabIndex = i; break; }
    }
    if (tabIndex == SIZE_MAX) return;

    while (channelMessages.size() <= tabIndex) {
        channelMessages.push_back(std::deque<ChannelMessage>());
    }

    ChannelMessage cm;
    cm.from = from;
    cm.timestamp = ts;
    cm.channel = channel;
    strncpy(cm.text, text, sizeof(cm.text) - 1);
    cm.text[sizeof(cm.text) - 1] = '\0';
    channelMessages[tabIndex].push_front(cm);
    while (channelMessages[tabIndex].size() > MAX_MESSAGES_PER_CHANNEL) {
        channelMessages[tabIndex].pop_back();
    }

    bool isOutbound = (from == myNodeNum);
    if (!isOutbound &&
        !(currentView == ViewKind::CHANNEL && currentChannelIndex == tabIndex)) {
        channels[tabIndex].hasUnread = true;
    }

    if (state == State::MAIN) {
        currentView = ViewKind::CHANNEL;
        currentChannelIndex = tabIndex;
        channels[tabIndex].hasUnread = false;
    }

    requestUpdate();
    requestAutoshow();
}

void MessageModule::markChannelAsRead(uint8_t channel) {
    for (auto& tab : channels) {
        if (tab.channelIndex == channel) {
            tab.hasUnread = false;
            requestUpdate();
            return;
        }
    }
}

void MessageModule::clear() {
    for (auto& q : channelMessages) q.clear();
    for (auto& tab : channels) tab.hasUnread = false;
    dmThreads.clear();
    currentView = ViewKind::DM_THREAD;
    currentDMNodeNum = 0;
    currentChannelIndex = 0;
    requestUpdate();
}

// ======================================================================
// Channel configuration
// ======================================================================

void MessageModule::addChannel(uint8_t channelIndex, const char* name, bool useChatStyle) {
    ChannelTab tab;
    tab.channelIndex = channelIndex;
    tab.hasUnread = false;
    tab.chatStyle = useChatStyle;
    strncpy(tab.name, name, sizeof(tab.name) - 1);
    tab.name[sizeof(tab.name) - 1] = '\0';
    channels.push_back(tab);
    channelMessages.push_back(std::deque<ChannelMessage>());

    // Default initial view to the first configured channel so the boot
    // experience isn't a "No DM Yet" placeholder.
    if (channels.size() == 1 && currentDMNodeNum == 0) {
        currentView = ViewKind::CHANNEL;
        currentChannelIndex = 0;
    }
}

uint8_t MessageModule::getCurrentChannel() const {
    if (currentView == ViewKind::CHANNEL && currentChannelIndex < channels.size()) {
        return channels[currentChannelIndex].channelIndex;
    }
    return CHANNEL_DM;
}

// ======================================================================
// Helpers
// ======================================================================

const char* MessageModule::getChannelName(uint8_t channel) const {
    for (const auto& tab : channels) {
        if (tab.channelIndex == channel) return tab.name;
    }
    return "Unknown";
}

const char* MessageModule::formatTime(uint32_t timestamp) const {
    return staticFormatTime(timestamp);
}

const char* MessageModule::defaultNodeName(uint32_t nodeNum) {
    static char buf[12];
    snprintf(buf, sizeof(buf), "%04X", (uint16_t)(nodeNum & 0xFFFF));
    return buf;
}

} // namespace InkHUD2
