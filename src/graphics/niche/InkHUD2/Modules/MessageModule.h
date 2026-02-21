#pragma once

#include "Module.h"
#include <cstdint>
#include <vector>
#include <deque>

namespace InkHUD2 {

// Special channel index for DM
static constexpr uint8_t CHANNEL_DM = 255;

// Message entry with channel info
struct ChannelMessage {
    uint32_t from;          // Node number
    uint32_t timestamp;     // Unix timestamp
    uint8_t channel;        // Channel index or CHANNEL_DM
    char text[237];         // Message text (UTF-8)
};

// Tab info for display
struct ChannelTab {
    uint8_t channelIndex;   // Channel index or CHANNEL_DM
    bool hasUnread;         // Has unread messages
    bool chatStyle;         // true = threaded chat, false = single message
    char name[16];          // Short name for display
};

// Message module with channel tabs - chat-style display
class MessageModule : public Module {
public:
    MessageModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;
    void onInput(Input input) override;

    // Add message to a channel (newest at front)
    void setMessage(uint32_t from, const char* text, uint8_t channel, uint32_t timestamp = 0);

    // Mark channel as read
    void markAsRead(uint8_t channel);

    // Clear all messages
    void clear();

    // Configure which channels to track (call at setup)
    // useChatStyle=true for threaded view, false for single message
    void addChannel(uint8_t channelIndex, const char* name, bool useChatStyle = true);
    void addDMChannel();  // Add DM tab (single message, no chat style)

    // Get current tab channel
    uint8_t getCurrentChannel() const;

    // Set own node number for outgoing detection
    void setMyNodeNum(uint32_t nodeNum) { myNodeNum = nodeNum; }

    // For external integration - get node name callbacks
    using NodeNameCallback = const char* (*)(uint32_t nodeNum);
    void setShortNameCallback(NodeNameCallback cb) { getShortName = cb; }
    void setLongNameCallback(NodeNameCallback cb) { getLongName = cb; }

private:
    void switchToNextTab();
    void switchToTab(size_t index);
    const char* getChannelName(uint8_t channel) const;
    const char* formatTime(uint32_t timestamp) const;

    // Tabs configuration
    std::vector<ChannelTab> tabs;
    size_t currentTabIndex = 0;

    // Messages per channel (indexed by tab index) - deque for chat history
    static constexpr size_t MAX_MESSAGES_PER_CHANNEL = 10;
    std::vector<std::deque<ChannelMessage>> channelMessages;

    // Own node number for outgoing detection
    uint32_t myNodeNum = 0;

    // Callbacks to get node names
    NodeNameCallback getShortName = nullptr;
    NodeNameCallback getLongName = nullptr;

    // Default fallback for node name
    static const char* defaultNodeName(uint32_t nodeNum);
};

} // namespace InkHUD2
