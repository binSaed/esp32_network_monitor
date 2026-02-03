#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <map>
#include "config.h"

struct DeviceInfo {
    uint8_t mac[6];
    char customName[MAX_DEVICE_NAME];
    char autoName[MAX_DEVICE_NAME];
    IPAddress ip;
    bool hasCustomName;

    String getDisplayName() const {
        if (hasCustomName && customName[0] != '\0') {
            return String(customName);
        }
        if (autoName[0] != '\0') {
            return String(autoName);
        }
        return "Unknown Device";
    }

    String getMACString() const {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buf);
    }
};

class DeviceManager {
public:
    DeviceManager();

    // Device tracking
    void updateDevice(const uint8_t* mac, const IPAddress& ip);
    void updateDeviceHostname(const uint8_t* mac, const String& hostname);
    DeviceInfo* getDevice(const uint8_t* mac);
    std::vector<DeviceInfo> getAllDevices();

    // Name management
    bool setDeviceName(const uint8_t* mac, const String& name);
    String getDeviceName(const uint8_t* mac);
    void clearDeviceName(const uint8_t* mac);

    // Utility
    bool parseMAC(const String& macStr, uint8_t* mac);
    String macToString(const uint8_t* mac);

private:
    std::vector<DeviceInfo> devices;

    DeviceInfo* findOrCreateDevice(const uint8_t* mac);
    bool macEqual(const uint8_t* mac1, const uint8_t* mac2);
};

extern DeviceManager deviceManager;

#endif // DEVICE_MANAGER_H
