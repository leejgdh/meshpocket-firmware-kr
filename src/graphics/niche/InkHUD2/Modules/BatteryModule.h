#pragma once

#include "Module.h"

namespace InkHUD2 {

// Battery icon overlay - renders in top-right corner
class BatteryModule : public SystemModule {
public:
    BatteryModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;

    // Set battery level (0-100)
    void setLevel(uint8_t level);
    void setCharging(bool charging);

private:
    uint8_t level = 100;
    bool charging = false;
};

} // namespace InkHUD2
