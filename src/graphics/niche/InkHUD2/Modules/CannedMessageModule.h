#pragma once

#include "Module.h"
#include "../UI/ContentArea.h"
#include <cstdint>
#include <string>
#include <vector>

namespace InkHUD2 {

// Forward declaration
class MenuModule;

// Canned message module - browse and broadcast pre-configured messages.
// Reads messages from the firmware's `cannedMessageModuleConfig` (the
// '|'-separated list managed by the existing CannedMessageModule global).
//
// Input mapping (matches MeshPocket two-button scheme):
//   DOWN   - next message in list
//   UP     - previous message
//   SELECT - cycles to next layout (handled by InkHUD2, not us)
//   BACK   - send the highlighted message as broadcast on primary channel
class CannedMessageModule : public Module {
public:
    CannedMessageModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;
    void onInput(Input input) override;
    void onActivate() override;

    // Used to surface "Sent!" / "No messages" feedback to the user.
    void setMenuModule(MenuModule* menu) { menuModule = menu; }

private:
    void reloadMessagesFromConfig();
    void sendSelected();
    void renderList(RenderContext& ctx, const ContentArea& content);

    MenuModule* menuModule = nullptr;

    // Cached split of cannedMessageModuleConfig.messages.
    std::vector<std::string> messages;

    uint16_t selectedIndex = 0;
    uint16_t scrollOffset = 0;
    uint16_t visibleRowCount = 0;  // Updated during render
};

} // namespace InkHUD2
