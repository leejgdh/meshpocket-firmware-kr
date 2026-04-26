#pragma once

#include <cstdint>

namespace InkHUD2 {

// Input types from hardware (buttons, joystick, touch)
enum class Input : uint8_t {
    NONE = 0,

    // D-pad / Joystick
    UP,
    DOWN,
    LEFT,
    RIGHT,

    // Buttons
    // Naming reflects the physical gesture, not a semantic role — a module
    // assigns its own meaning. Modules MUST NOT assume SHORT_PRESS == "OK"
    // or LONG_PRESS == "cancel". On single-button devices (MeshPocket),
    // these are the only inputs that ever fire from the slot module's POV
    // (and SHORT_PRESS is consumed by InkHUD2 to cycle layouts unless the
    //  module sets handlesInput = true).
    SHORT_PRESS,  // Single short click
    DOUBLE_TAP,   // Double click
    LONG_PRESS,   // Held past the long-press threshold
    AUX,          // Auxiliary button (device-specific)

    // Touch (future)
    TAP,
    LONG_TAP,
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT
};

// Event types from Meshtastic firmware
enum class EventType : uint8_t {
    NONE = 0,

    // Node events
    NODE_DISCOVERED,
    NODE_UPDATED,
    NODE_LOST,

    // Message events
    MESSAGE_RECEIVED,
    MESSAGE_SENT,
    MESSAGE_FAILED,

    // Channel events
    CHANNEL_CHANGED,

    // System events
    BOOT_COMPLETE,
    SHUTDOWN_REQUESTED,
    LOW_BATTERY,

    // Connection events
    BLUETOOTH_CONNECTED,
    BLUETOOTH_DISCONNECTED,
    GPS_FIX_ACQUIRED,
    GPS_FIX_LOST,

    // Mesh events
    PACKET_RECEIVED,
    PACKET_SENT
};

// Event payload - union for different event data
struct EventData {
    union {
        struct {
            uint32_t nodeNum;
        } node;

        struct {
            uint32_t from;
            uint32_t to;
            uint8_t channel;
        } message;

        struct {
            uint8_t level;    // 0-100
            bool charging;
        } battery;

        struct {
            int32_t latitude;
            int32_t longitude;
        } gps;
    };
};

struct Event {
    EventType type = EventType::NONE;
    EventData data = {};

    // Convenience constructors
    static Event nodeDiscovered(uint32_t nodeNum) {
        Event e;
        e.type = EventType::NODE_DISCOVERED;
        e.data.node.nodeNum = nodeNum;
        return e;
    }

    static Event messageReceived(uint32_t from, uint32_t to, uint8_t channel) {
        Event e;
        e.type = EventType::MESSAGE_RECEIVED;
        e.data.message.from = from;
        e.data.message.to = to;
        e.data.message.channel = channel;
        return e;
    }

    static Event lowBattery(uint8_t level) {
        Event e;
        e.type = EventType::LOW_BATTERY;
        e.data.battery.level = level;
        return e;
    }
};

} // namespace InkHUD2
