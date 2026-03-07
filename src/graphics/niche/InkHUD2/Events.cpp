/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Events.h"
#include "InkHUD2.h"
#include "Modules/MessageModule.h"
#include "Modules/BatteryModule.h"
#include "Modules/NodeListModule.h"
#include "Modules/MapModule.h"
#include "Modules/MenuModule.h"
#include "modules/TextMessageModule.h"
#include "NodeDB.h"
#include "RTC.h"
#include "PowerStatus.h"
#include "NodeStatus.h"
#include "power.h"
#include "sleep.h"
#include "buzz.h"
#include "main.h"
#include "power/PowerHAL.h"
#include "gps/GeoCoord.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace InkHUD2 {

// Callback to get short node name from NodeDB
static const char* getShortNameFromDB(uint32_t nodeNum) {
    static char nameBuf[16];

    meshtastic_NodeInfoLite* node = nodeDB->getMeshNode(nodeNum);
    if (node && node->has_user && node->user.short_name[0] != '\0') {
        return node->user.short_name;
    }

    // Fallback: hex ID
    snprintf(nameBuf, sizeof(nameBuf), "!%08lx", (unsigned long)nodeNum);
    return nameBuf;
}

// Callback to get long node name from NodeDB
static const char* getLongNameFromDB(uint32_t nodeNum) {
    meshtastic_NodeInfoLite* node = nodeDB->getMeshNode(nodeNum);
    if (node && node->has_user && node->user.long_name[0] != '\0') {
        return node->user.long_name;
    }
    return nullptr;  // No long name available
}

void Events::begin(MessageModule* msgModule, BatteryModule* batModule, NodeListModule* nodeModule,
                   MapModule* mapMod, MenuModule* menuMod) {
    messageModule = msgModule;
    batteryModule = batModule;
    nodeListModule = nodeModule;
    mapModule = mapMod;
    menuModule = menuMod;

    // Set up node name callbacks
    if (messageModule) {
        messageModule->setShortNameCallback(getShortNameFromDB);
        messageModule->setLongNameCallback(getLongNameFromDB);
    }

    // Start observing text messages
    textMessageObserver.observe(textMessageModule);

    // Start observing power status
    powerStatusObserver.observe(&powerStatus->onNewStatus);

    // Start observing node status changes
    nodeStatusObserver.observe(&nodeDB->newStatus);

    // Start observing deep sleep (shutdown)
    deepSleepObserver.observe(&notifyDeepSleep);

    // Start observing reboot (from app settings change etc.)
    rebootObserver.observe(&notifyReboot);

    // No initial syncNodes() here - Meshtastic calls nodeDB->notifyObservers(true)
    // after setupNicheGraphics() completes (main.cpp line ~1024), which will trigger
    // onNodeStatusUpdate() and syncNodes() with fully loaded NodeDB.
}

void Events::stop() {
    textMessageObserver.unobserve(textMessageModule);
    powerStatusObserver.unobserve(&powerStatus->onNewStatus);
    nodeStatusObserver.unobserve(&nodeDB->newStatus);
    deepSleepObserver.unobserve(&notifyDeepSleep);
    rebootObserver.unobserve(&notifyReboot);
    messageModule = nullptr;
    batteryModule = nullptr;
    nodeListModule = nullptr;
    mapModule = nullptr;
    menuModule = nullptr;
}

void Events::tick() {
    // Reserved for future periodic operations
    // Currently unused - syncNodes() is triggered by nodeDB->notifyObservers()
}

void Events::syncNodes() {
    if (!nodeListModule) return;

    nodeListModule->clearNodes();

    uint32_t ourNodeNum = nodeDB->getNodeNum();
    meshtastic_NodeInfoLite* ourNode = nodeDB->getMeshNode(ourNodeNum);

    for (auto it = nodeDB->meshNodes->begin(); it != nodeDB->meshNodes->end(); ++it) {
        meshtastic_NodeInfoLite* node = &(*it);

        // Skip empty nodes and our own node
        if (node->num == 0 || node->num == ourNodeNum) continue;

        NodeEntry entry;
        entry.nodeNum = node->num;
        entry.lastHeard = node->last_heard;
        entry.snr = node->snr;
        // entry.isFavorite already = false by default

        // Hops away
        if (node->has_hops_away) {
            entry.hopsAway = node->hops_away;
        }

        // Position info
        entry.hasPosition = nodeDB->hasValidPosition(node);
        if (entry.hasPosition) {
            entry.latitude = node->position.latitude_i;
            entry.longitude = node->position.longitude_i;
        }
        // else: latitude/longitude already = 0 by default

        // Distance calculation (like old InkHUD)
        if (entry.hasPosition && ourNode && nodeDB->hasValidPosition(ourNode)) {
            float ourLat = ourNode->position.latitude_i * 1e-7;
            float ourLong = ourNode->position.longitude_i * 1e-7;
            float theirLat = node->position.latitude_i * 1e-7;
            float theirLong = node->position.longitude_i * 1e-7;

            entry.distanceMeters = (int32_t)GeoCoord::latLongToMeter(theirLat, theirLong, ourLat, ourLong);
        }

        // Copy names
        if (node->has_user && node->user.short_name[0] != '\0') {
            strncpy(entry.shortName, node->user.short_name, sizeof(entry.shortName) - 1);
            entry.shortName[sizeof(entry.shortName) - 1] = '\0';
        } else {
            // shortName is 5 chars: "!XXX\0" (3 hex digits max)
            snprintf(entry.shortName, sizeof(entry.shortName), "!%03x", (unsigned int)(node->num & 0xFFF));
        }

        if (node->has_user && node->user.long_name[0] != '\0') {
            strncpy(entry.longName, node->user.long_name, sizeof(entry.longName) - 1);
            entry.longName[sizeof(entry.longName) - 1] = '\0';
        } else {
            entry.longName[0] = '\0';
        }

        nodeListModule->updateNode(entry);

        // Also update MapModule if available
        if (mapModule && entry.hasPosition) {
            MapNode mapEntry;
            mapEntry.nodeNum = entry.nodeNum;
            strncpy(mapEntry.shortName, entry.shortName, sizeof(mapEntry.shortName));
            strncpy(mapEntry.longName, entry.longName, sizeof(mapEntry.longName));
            mapEntry.latitude = entry.latitude;
            mapEntry.longitude = entry.longitude;
            mapEntry.distanceMeters = entry.distanceMeters;
            mapEntry.hopsAway = entry.hopsAway;
            mapEntry.isFavorite = node->is_favorite;  // From Meshtastic NodeDB
            // mapEntry.isOwnNode already = false by default
            mapModule->updateNode(mapEntry);
        }
    }

    // Update MapModule with our own position
    if (mapModule && ourNode && nodeDB->hasValidPosition(ourNode)) {
        mapModule->setOwnPosition(ourNode->position.latitude_i, ourNode->position.longitude_i);
    }
}

int Events::onReceiveTextMessage(const meshtastic_MeshPacket* packet) {
    if (!messageModule || !packet) return 0;

    // Don't show our own outgoing messages
    if (packet->from == nodeDB->getNodeNum()) return 0;

    // Extract message text
    const char* text = reinterpret_cast<const char*>(packet->decoded.payload.bytes);
    size_t len = packet->decoded.payload.size;

    // Create null-terminated string (payload may not be null-terminated)
    char msgText[238];
    size_t copyLen = len < sizeof(msgText) - 1 ? len : sizeof(msgText) - 1;
    memcpy(msgText, text, copyLen);
    msgText[copyLen] = '\0';

    // Get timestamp
    uint32_t timestamp = getValidTime(RTCQuality::RTCQualityDevice, true);

    // Determine channel: DM if sent directly to us, otherwise use packet's channel
    uint8_t channel;
    if (packet->to == nodeDB->getNodeNum()) {
        // Direct message to us
        channel = CHANNEL_DM;
    } else {
        // Broadcast on a channel
        channel = packet->channel;
    }

    // Update message module
    messageModule->setMessage(packet->from, msgText, channel, timestamp);

    return 0;  // Continue notifying other observers
}

int Events::onPowerStatusUpdate(const meshtastic::Status* status) {
    if (!batteryModule || !status) return 0;

    // Verify it's a power status
    if (status->getStatusType() != STATUS_TYPE_POWER) return 0;

    const meshtastic::PowerStatus* pwrStatus = static_cast<const meshtastic::PowerStatus*>(status);

    // Update battery module with level and charging state
    int8_t percent = pwrStatus->getBatteryChargePercent();
    bool isCharging = pwrStatus->getIsCharging();
    bool hasUSB = pwrStatus->getHasUSB();

    // Set level (clamp to 0-100)
    if (percent >= 0 && percent <= 100) {
        batteryModule->setLevel(static_cast<uint8_t>(percent));
    }

    // Set charging state: charging if USB connected and isCharging flag is set
    // Also consider charging if USB connected and voltage is high (>4.15V = 4150mV)
    bool showCharging = isCharging || (hasUSB && pwrStatus->getBatteryVoltageMv() > 4150);
    batteryModule->setCharging(showCharging);

    return 0;  // Continue notifying other observers
}

int Events::onNodeStatusUpdate(const meshtastic::NodeStatus* status) {
    // Re-sync nodes when node DB changes
    syncNodes();
    return 0;
}

int Events::beforeDeepSleep(void* unused) {
    // Show shutdown screen if menuModule is available
    if (menuModule) {
        menuModule->showShutdownScreen();
    }

    // Save NodeDB before shutdown
    // Power::shutdown() calls doDeepSleep() with skipSaveNodeDb=true,
    // so we must save it here ourselves.
    nodeDB->saveToDisk();

    // Play shutdown sound
    playShutdownMelody();

    return 0;  // Agree to deep sleep
}

int Events::beforeReboot(void* unused) {
    // Show shutdown screen for reboot (triggered by app settings change etc.)
    if (menuModule) {
        menuModule->showShutdownScreen();
    }

    // Save NodeDB before reboot
    nodeDB->saveToDisk();

    return 0;  // Agree to reboot
}

} // namespace InkHUD2
