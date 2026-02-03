#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "config.h"

class StorageManager {
public:
    StorageManager();

    // WiFi Credentials
    bool saveSTACredentials(const String& ssid, const String& password);
    bool loadSTACredentials(String& ssid, String& password);

    // Upstream DNS
    bool saveUpstreamDNS(const IPAddress& dns);
    IPAddress loadUpstreamDNS();

    // Device Names
    bool saveDeviceName(const uint8_t* mac, const String& name);
    String loadDeviceName(const uint8_t* mac);
    bool deleteDeviceName(const uint8_t* mac);

    // Blocked Domains
    bool saveBlockedDomains(const std::vector<String>& domains);
    std::vector<String> loadBlockedDomains();

    // Bandwidth Stats (optional persistence)
    bool saveDeviceStats(const uint8_t* mac, uint64_t upload, uint64_t download);
    bool loadDeviceStats(const uint8_t* mac, uint64_t& upload, uint64_t& download);
    void clearAllStats();

    // Utility
    void clearAll();

private:
    Preferences prefs;
    String macToKey(const uint8_t* mac);
};

extern StorageManager storage;

#endif // STORAGE_MANAGER_H
