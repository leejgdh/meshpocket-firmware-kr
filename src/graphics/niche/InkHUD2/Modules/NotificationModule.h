#pragma once

#include "Module.h"
#include <cstring>

namespace InkHUD2 {

// Notification popup bar - shows temporary messages
class NotificationModule : public SystemModule {
public:
    NotificationModule();

    void onRender(RenderContext& ctx) override;

    // Show notification with auto-dismiss timeout (0 = manual dismiss)
    void show(const char* message, uint32_t durationMs = 3000);

    // Dismiss current notification
    void dismiss();

    // Update tick (for auto-dismiss)
    void tick(uint32_t currentMs);

private:
    static constexpr size_t MAX_MESSAGE_LEN = 64;

    char message[MAX_MESSAGE_LEN] = {0};
    bool visible = false;
    uint32_t showTime = 0;
    uint32_t duration = 0;
};

} // namespace InkHUD2
