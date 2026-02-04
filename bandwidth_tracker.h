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

// Lock-free ring buffer entry for cross-task packet events
struct PacketEvent {
    uint8_t mac[6];
    uint16_t length;
    bool isUpload;
};

class BandwidthTracker {
public:
    BandwidthTracker();

    // Control
    void begin();
    void update();  // Call periodically to save stats

    // Packet counting (called from WiFi task via promiscuous callback - ISR-safe)
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

    // Cached AP MAC to avoid WiFi calls from callback context
    uint8_t _apMac[6];

    // Lock-free SPSC ring buffer (producer: WiFi task, consumer: main loop)
    static const int RING_SIZE = 512;
    PacketEvent _ring[RING_SIZE];
    volatile int _head;
    volatile int _tail;

    void processPacketQueue();
    DeviceStats* findOrCreateDevice(const uint8_t* mac);
    bool isLocalMAC(const uint8_t* mac);
    bool macEqual(const uint8_t* mac1, const uint8_t* mac2);
    void macCopy(uint8_t* dst, const uint8_t* src);
};

extern BandwidthTracker bandwidthTracker;

#endif // BANDWIDTH_TRACKER_H
