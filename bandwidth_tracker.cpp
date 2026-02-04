#include "bandwidth_tracker.h"
#include "storage_manager.h"
#include <WiFi.h>

BandwidthTracker bandwidthTracker;

BandwidthTracker::BandwidthTracker() : lastSaveTime(0), _head(0), _tail(0) {
    memset(_apMac, 0, 6);
}

void BandwidthTracker::begin() {
    // Cache AP MAC so recordPacket doesn't need to call WiFi APIs
    WiFi.softAPmacAddress(_apMac);
    loadStats();
    lastSaveTime = millis();
    DEBUG_PRINTLN("Bandwidth: Tracker initialized");
}

void BandwidthTracker::update() {
    // Drain packet queue from WiFi task (must happen before anything else)
    processPacketQueue();

    // Periodic save
    if (millis() - lastSaveTime > STATS_SAVE_INTERVAL_MS) {
        saveStats();
        lastSaveTime = millis();
    }

    // Mark inactive devices
    uint32_t now = millis();
    for (auto& device : devices) {
        if (device.active && (now - device.lastSeen > DEVICE_TIMEOUT_MS)) {
            device.active = false;
        }
    }
}

void BandwidthTracker::recordPacket(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload) {
    // Called from WiFi task (promiscuous callback) - keep minimal, no allocations
    if (srcMac[0] & 0x01 || dstMac[0] & 0x01) {
        return;
    }

    const uint8_t* clientMac = isUpload ? srcMac : dstMac;

    // Use cached AP MAC instead of calling WiFi.softAPmacAddress()
    if (macEqual(clientMac, _apMac)) {
        return;
    }

    // Write to ring buffer (lock-free: single producer from WiFi task)
    int next = (_head + 1) % RING_SIZE;
    if (next != _tail) {  // Not full
        memcpy(_ring[_head].mac, clientMac, 6);
        _ring[_head].length = length;
        _ring[_head].isUpload = isUpload;
        _head = next;  // Publish (volatile write makes it visible to consumer)
    }
    // If full, drop the packet event (stats will be slightly off, but no crash)
}

void BandwidthTracker::processPacketQueue() {
    // Called from main loop only - safe to modify vectors
    while (_tail != _head) {
        PacketEvent& evt = _ring[_tail];
        DeviceStats* stats = findOrCreateDevice(evt.mac);
        if (stats) {
            if (evt.isUpload) {
                stats->uploadBytes += evt.length;
            } else {
                stats->downloadBytes += evt.length;
            }
            stats->lastSeen = millis();
            stats->active = true;
        }
        _tail = (_tail + 1) % RING_SIZE;
    }
}

DeviceStats* BandwidthTracker::findOrCreateDevice(const uint8_t* mac) {
    // Find existing
    for (auto& device : devices) {
        if (macEqual(device.mac, mac)) {
            return &device;
        }
    }

    // Create new if under limit
    if (devices.size() >= MAX_DEVICES) {
        // Remove oldest inactive device
        int oldestIdx = -1;
        uint32_t oldestTime = UINT32_MAX;
        for (int i = 0; i < devices.size(); i++) {
            if (!devices[i].active && devices[i].lastSeen < oldestTime) {
                oldestTime = devices[i].lastSeen;
                oldestIdx = i;
            }
        }
        if (oldestIdx >= 0) {
            devices.erase(devices.begin() + oldestIdx);
        } else {
            DEBUG_PRINTLN("Bandwidth: Max devices reached");
            return nullptr;
        }
    }

    DeviceStats newDevice;
    macCopy(newDevice.mac, mac);
    newDevice.uploadBytes = 0;
    newDevice.downloadBytes = 0;
    newDevice.lastSeen = millis();
    newDevice.active = true;

    // Try to load saved stats for this MAC
    uint64_t savedUp = 0, savedDown = 0;
    if (storage.loadDeviceStats(mac, savedUp, savedDown)) {
        newDevice.uploadBytes = savedUp;
        newDevice.downloadBytes = savedDown;
    }

    devices.push_back(newDevice);

    DEBUG_PRINTF("Bandwidth: New device %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return &devices.back();
}

std::vector<DeviceStats> BandwidthTracker::getAllStats() {
    // Return sorted by total bandwidth (descending)
    std::vector<DeviceStats> sorted = devices;
    std::sort(sorted.begin(), sorted.end(), [](const DeviceStats& a, const DeviceStats& b) {
        return a.totalBytes() > b.totalBytes();
    });
    return sorted;
}

DeviceStats* BandwidthTracker::getDeviceStats(const uint8_t* mac) {
    for (auto& device : devices) {
        if (macEqual(device.mac, mac)) {
            return &device;
        }
    }
    return nullptr;
}

int BandwidthTracker::getActiveDeviceCount() {
    int count = 0;
    for (const auto& device : devices) {
        if (device.active) count++;
    }
    return count;
}

void BandwidthTracker::resetDeviceStats(const uint8_t* mac) {
    for (auto& device : devices) {
        if (macEqual(device.mac, mac)) {
            device.uploadBytes = 0;
            device.downloadBytes = 0;
            storage.saveDeviceStats(mac, 0, 0);
            DEBUG_PRINTF("Bandwidth: Reset stats for %02X:%02X:%02X:%02X:%02X:%02X\n",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return;
        }
    }
}

void BandwidthTracker::resetAllStats() {
    for (auto& device : devices) {
        device.uploadBytes = 0;
        device.downloadBytes = 0;
    }
    storage.clearAllStats();
    DEBUG_PRINTLN("Bandwidth: All stats reset");
}

void BandwidthTracker::saveStats() {
    for (const auto& device : devices) {
        storage.saveDeviceStats(device.mac, device.uploadBytes, device.downloadBytes);
    }
    DEBUG_PRINTF("Bandwidth: Saved stats for %d devices\n", devices.size());
}

void BandwidthTracker::loadStats() {
    // Stats are loaded on-demand when devices are first seen
    DEBUG_PRINTLN("Bandwidth: Ready to load device stats on demand");
}

bool BandwidthTracker::macEqual(const uint8_t* mac1, const uint8_t* mac2) {
    return memcmp(mac1, mac2, 6) == 0;
}

void BandwidthTracker::macCopy(uint8_t* dst, const uint8_t* src) {
    memcpy(dst, src, 6);
}

bool BandwidthTracker::isLocalMAC(const uint8_t* mac) {
    uint8_t staMac[6];
    WiFi.macAddress(staMac);
    return macEqual(mac, _apMac) || macEqual(mac, staMac);
}
