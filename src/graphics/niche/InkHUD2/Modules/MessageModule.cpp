/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "MessageModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/StatusBar.h"
#include "../UI/Footer.h"
#include "../UI/ContentArea.h"
#include "../Views/ChatView.h"
#include "../Views/DMView.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace InkHUD2 {

// Static wrapper for formatTime (needed for ChatView callback)
static const MessageModule* g_currentModule = nullptr;

static const char* staticFormatTime(uint32_t timestamp) {
    if (g_currentModule) {
        // Use millis-based relative time
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

MessageModule::MessageModule() {
    tabs.reserve(8);
    channelMessages.reserve(8);
}

void MessageModule::onRender(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();

    // Set global for static callback
    g_currentModule = this;

    // Create TextRenderer
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    // Check current tab style
    bool useChatStyle = (currentTabIndex < tabs.size()) && tabs[currentTabIndex].chatStyle;
    bool hasMessages = (currentTabIndex < channelMessages.size()) &&
                       !channelMessages[currentTabIndex].empty();

    // === Render StatusBar ===
    StatusBar statusBar(buffer, layout, &textRenderer);
    const char* title;
    if (useChatStyle) {
        title = "Group Chat";
    } else {
        // DM - show sender name
        if (hasMessages) {
            const ChannelMessage& msg = channelMessages[currentTabIndex].front();
            title = getShortName ? getShortName(msg.from) : defaultNodeName(msg.from);
        } else {
            title = "DM";
        }
    }
    int16_t contentTop = statusBar.render(padding, title, StatusBar::Icon::ENVELOPE);

    // === Render Footer (tabs) ===
    Footer footer(buffer, layout, &textRenderer);
    std::vector<TabInfo> tabInfos;
    for (size_t i = 0; i < tabs.size(); i++) {
        tabInfos.push_back({tabs[i].name, i == currentTabIndex, tabs[i].hasUnread});
    }
    int16_t contentBottom = footer.renderTabs(tabInfos, currentTabIndex);

    // === Calculate content area (uses safe area - respects battery) ===
    ContentArea content = calculateContentArea(layout, contentTop, contentBottom);

    // === Render content ===
    if (!hasMessages) {
        textRenderer.text(ctx.width() / 2, content.top() + content.h / 2,
                         "No Messages", Align::CENTER, Color::BLACK);
        return;
    }

    if (useChatStyle) {
        // === Chat View ===
        ChatView chatView(buffer, layout, &textRenderer);
        chatView.setCallbacks(
            getShortName ? getShortName : defaultNodeName,
            staticFormatTime,
            myNodeNum
        );

        // Convert messages
        std::vector<ChatMessage> chatMessages;
        const auto& msgs = channelMessages[currentTabIndex];
        for (const auto& m : msgs) {
            chatMessages.push_back({m.from, m.timestamp, m.text});
        }

        chatView.render(content, chatMessages);
    } else {
        // === DM View ===
        DMView dmView(buffer, layout, &textRenderer);
        const ChannelMessage& msg = channelMessages[currentTabIndex].front();
        dmView.render(content, msg.text);
    }
}

void MessageModule::onEvent(const Event& e) {
    if (e.type == EventType::MESSAGE_RECEIVED) {
        requestUpdate();
        requestAutoshow();
    }
}

void MessageModule::onInput(Input input) {
    if (input == Input::BACK) {
        switchToNextTab();
    }
}

// === Tab management ===

void MessageModule::switchToNextTab() {
    if (tabs.size() > 1) {
        currentTabIndex = (currentTabIndex + 1) % tabs.size();
        if (currentTabIndex < tabs.size()) {
            tabs[currentTabIndex].hasUnread = false;
        }
        requestUpdate();
    }
}

void MessageModule::switchToTab(size_t index) {
    if (index < tabs.size() && index != currentTabIndex) {
        currentTabIndex = index;
        tabs[currentTabIndex].hasUnread = false;
        requestUpdate();
    }
}

// === Message management ===

void MessageModule::setMessage(uint32_t from, const char* text, uint8_t channel, uint32_t timestamp) {
    size_t tabIndex = SIZE_MAX;
    for (size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].channelIndex == channel) {
            tabIndex = i;
            break;
        }
    }

    if (tabIndex == SIZE_MAX) return;

    while (channelMessages.size() <= tabIndex) {
        channelMessages.push_back(std::deque<ChannelMessage>());
    }

    ChannelMessage msg;
    msg.from = from;
    msg.timestamp = timestamp ? timestamp : (millis() / 1000);
    msg.channel = channel;
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    channelMessages[tabIndex].push_front(msg);

    while (channelMessages[tabIndex].size() > MAX_MESSAGES_PER_CHANNEL) {
        channelMessages[tabIndex].pop_back();
    }

    if (tabIndex != currentTabIndex) {
        tabs[tabIndex].hasUnread = true;
    }

    currentTabIndex = tabIndex;
    tabs[tabIndex].hasUnread = false;

    requestUpdate();
    requestAutoshow();
}

void MessageModule::markAsRead(uint8_t channel) {
    for (size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].channelIndex == channel) {
            tabs[i].hasUnread = false;
            requestUpdate();
            break;
        }
    }
}

void MessageModule::clear() {
    for (auto& msgs : channelMessages) {
        msgs.clear();
    }
    for (auto& tab : tabs) {
        tab.hasUnread = false;
    }
    currentTabIndex = 0;
    requestUpdate();
}

// === Channel configuration ===

void MessageModule::addChannel(uint8_t channelIndex, const char* name, bool useChatStyle) {
    ChannelTab tab;
    tab.channelIndex = channelIndex;
    tab.hasUnread = false;
    tab.chatStyle = useChatStyle;
    strncpy(tab.name, name, sizeof(tab.name) - 1);
    tab.name[sizeof(tab.name) - 1] = '\0';
    tabs.push_back(tab);
    channelMessages.push_back(std::deque<ChannelMessage>());
}

void MessageModule::addDMChannel() {
    addChannel(CHANNEL_DM, "DM", false);
}

uint8_t MessageModule::getCurrentChannel() const {
    if (currentTabIndex < tabs.size()) {
        return tabs[currentTabIndex].channelIndex;
    }
    return 0;
}

// === Helpers ===

const char* MessageModule::getChannelName(uint8_t channel) const {
    for (const auto& tab : tabs) {
        if (tab.channelIndex == channel) {
            return tab.name;
        }
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

// === Legacy methods removed ===
// renderChatMessages, renderSingleMessage, renderTabs
// Now handled by ChatView, DMView, StatusBar, Footer

} // namespace InkHUD2
