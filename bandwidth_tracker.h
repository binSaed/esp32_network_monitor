#ifndef BANDWIDTH_TRACKER_H
#define BANDWIDTH_TRACKER_H

#include <Arduino.h>
#include <vector>
#include "config.h"

struct DeviceStats {
    uint8_t mac[6];
    uint64_t uploadBytes;
    uint64_t downloadBytes;
    uint32_t lastSeen;
    bool active;

    uint64_t totalBytes() const {
        return uploadBytes + downloadBytes;
    }
};

class BandwidthTracker {
public:
    BandwidthTracker();

    // Control
    void begin();
    void update();  // Call periodically to save stats

    // Packet counting (called by NAT engine)
    void recordPacket(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload);

    // Stats access
    std::vector<DeviceStats> getAllStats();
    DeviceStats* getDeviceStats(const uint8_t* mac);
    int getActiveDeviceCount();

    // Reset
    void resetDeviceStats(const uint8_t* mac);
    void resetAllStats();

    // Persistence
    void saveStats();
    void loadStats();

private:
    std::vector<DeviceStats> devices;
    uint32_t lastSaveTime;

    DeviceStats* findOrCreateDevice(const uint8_t* mac);
    bool isLocalMAC(const uint8_t* mac);
    bool macEqual(const uint8_t* mac1, const uint8_t* mac2);
    void macCopy(uint8_t* dst, const uint8_t* src);
};

extern BandwidthTracker bandwidthTracker;

#endif // BANDWIDTH_TRACKER_H
