#include "storage_manager.h"

StorageManager storage;

StorageManager::StorageManager() {}

String StorageManager::macToKey(const uint8_t* mac) {
    char key[13];
    snprintf(key, sizeof(key), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(key);
}

// WiFi Credentials
bool StorageManager::saveSTACredentials(const String& ssid, const String& password) {
    prefs.begin(NVS_NAMESPACE_WIFI, false);
    bool ok = prefs.putString("sta_ssid", ssid) > 0;
    ok &= prefs.putString("sta_pass", password) > 0;
    prefs.end();
    DEBUG_PRINTF("Storage: Saved STA credentials for '%s'\n", ssid.c_str());
    return ok;
}

bool StorageManager::loadSTACredentials(String& ssid, String& password) {
    prefs.begin(NVS_NAMESPACE_WIFI, true);
    ssid = prefs.getString("sta_ssid", "");
    password = prefs.getString("sta_pass", "");
    prefs.end();
    return ssid.length() > 0;
}

// Upstream DNS
bool StorageManager::saveUpstreamDNS(const IPAddress& dns) {
    prefs.begin(NVS_NAMESPACE_DNS, false);
    bool ok = prefs.putUInt("upstream", (uint32_t)dns) > 0;
    prefs.end();
    DEBUG_PRINTF("Storage: Saved upstream DNS %s\n", dns.toString().c_str());
    return ok;
}

IPAddress StorageManager::loadUpstreamDNS() {
    prefs.begin(NVS_NAMESPACE_DNS, true);
    uint32_t dnsVal = prefs.getUInt("upstream", (uint32_t)DEFAULT_UPSTREAM_DNS);
    prefs.end();
    return IPAddress(dnsVal);
}

// Device Names
bool StorageManager::saveDeviceName(const uint8_t* mac, const String& name) {
    String key = macToKey(mac);
    prefs.begin(NVS_NAMESPACE_DEVICES, false);
    bool ok = prefs.putString(key.c_str(), name) > 0;
    prefs.end();
    DEBUG_PRINTF("Storage: Saved device name '%s' for %s\n", name.c_str(), key.c_str());
    return ok;
}

String StorageManager::loadDeviceName(const uint8_t* mac) {
    String key = macToKey(mac);
    prefs.begin(NVS_NAMESPACE_DEVICES, true);
    String name = prefs.getString(key.c_str(), "");
    prefs.end();
    return name;
}

bool StorageManager::deleteDeviceName(const uint8_t* mac) {
    String key = macToKey(mac);
    prefs.begin(NVS_NAMESPACE_DEVICES, false);
    bool ok = prefs.remove(key.c_str());
    prefs.end();
    return ok;
}

// Blocked Domains
bool StorageManager::saveBlockedDomains(const std::vector<String>& domains) {
    prefs.begin(NVS_NAMESPACE_DNS, false);

    // Store count
    prefs.putUInt("blocked_cnt", domains.size());

    // Store each domain
    for (size_t i = 0; i < domains.size() && i < MAX_BLOCKED_DOMAINS; i++) {
        String key = "bd_" + String(i);
        prefs.putString(key.c_str(), domains[i]);
    }

    prefs.end();
    DEBUG_PRINTF("Storage: Saved %d blocked domains\n", domains.size());
    return true;
}

std::vector<String> StorageManager::loadBlockedDomains() {
    std::vector<String> domains;
    prefs.begin(NVS_NAMESPACE_DNS, true);

    uint32_t count = prefs.getUInt("blocked_cnt", 0);
    for (uint32_t i = 0; i < count && i < MAX_BLOCKED_DOMAINS; i++) {
        String key = "bd_" + String(i);
        String domain = prefs.getString(key.c_str(), "");
        if (domain.length() > 0) {
            domains.push_back(domain);
        }
    }

    prefs.end();
    DEBUG_PRINTF("Storage: Loaded %d blocked domains\n", domains.size());
    return domains;
}

// Bandwidth Stats
bool StorageManager::saveDeviceStats(const uint8_t* mac, uint64_t upload, uint64_t download) {
    String key = macToKey(mac);
    prefs.begin(NVS_NAMESPACE_STATS, false);

    String upKey = key + "_up";
    String downKey = key + "_dn";

    prefs.putULong64(upKey.c_str(), upload);
    prefs.putULong64(downKey.c_str(), download);

    prefs.end();
    return true;
}

bool StorageManager::loadDeviceStats(const uint8_t* mac, uint64_t& upload, uint64_t& download) {
    String key = macToKey(mac);
    prefs.begin(NVS_NAMESPACE_STATS, true);

    String upKey = key + "_up";
    String downKey = key + "_dn";

    upload = prefs.getULong64(upKey.c_str(), 0);
    download = prefs.getULong64(downKey.c_str(), 0);

    prefs.end();
    return true;
}

void StorageManager::clearAllStats() {
    prefs.begin(NVS_NAMESPACE_STATS, false);
    prefs.clear();
    prefs.end();
    DEBUG_PRINTLN("Storage: Cleared all stats");
}

void StorageManager::clearAll() {
    prefs.begin(NVS_NAMESPACE_WIFI, false);
    prefs.clear();
    prefs.end();

    prefs.begin(NVS_NAMESPACE_DEVICES, false);
    prefs.clear();
    prefs.end();

    prefs.begin(NVS_NAMESPACE_DNS, false);
    prefs.clear();
    prefs.end();

    prefs.begin(NVS_NAMESPACE_STATS, false);
    prefs.clear();
    prefs.end();

    DEBUG_PRINTLN("Storage: Cleared all data");
}
