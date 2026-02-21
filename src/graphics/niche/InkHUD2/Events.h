#pragma once

#include "Observer.h"
#include "mesh/MeshTypes.h"

// Forward declaration for meshtastic namespace
namespace meshtastic {
    class Status;
    class NodeStatus;
}

namespace InkHUD2 {

// Forward declarations
class MessageModule;
class BatteryModule;
class NodeListModule;
class MapModule;

// Events - hooks into Meshtastic event system
class Events {
public:
    Events() = default;

    // Initialize and start observing
    void begin(MessageModule* msgModule, BatteryModule* batModule, NodeListModule* nodeModule, MapModule* mapMod = nullptr);

    // Stop observing
    void stop();

    // Sync nodes from NodeDB (call periodically or on demand)
    void syncNodes();

private:
    // Observer callbacks
    int onReceiveTextMessage(const meshtastic_MeshPacket* packet);
    int onPowerStatusUpdate(const meshtastic::Status* status);
    int onNodeStatusUpdate(const meshtastic::NodeStatus* status);
    int beforeDeepSleep(void* unused);

    // Observers
    CallbackObserver<Events, const meshtastic_MeshPacket*> textMessageObserver =
        CallbackObserver<Events, const meshtastic_MeshPacket*>(this, &Events::onReceiveTextMessage);

    CallbackObserver<Events, const meshtastic::Status*> powerStatusObserver =
        CallbackObserver<Events, const meshtastic::Status*>(this, &Events::onPowerStatusUpdate);

    CallbackObserver<Events, const meshtastic::NodeStatus*> nodeStatusObserver =
        CallbackObserver<Events, const meshtastic::NodeStatus*>(this, &Events::onNodeStatusUpdate);

    // Get notified when the system is shutting down
    CallbackObserver<Events, void*> deepSleepObserver =
        CallbackObserver<Events, void*>(this, &Events::beforeDeepSleep);

    // Module references
    MessageModule* messageModule = nullptr;
    BatteryModule* batteryModule = nullptr;
    NodeListModule* nodeListModule = nullptr;
    MapModule* mapModule = nullptr;
};

} // namespace InkHUD2
