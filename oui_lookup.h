#ifndef OUI_LOOKUP_H
#define OUI_LOOKUP_H

#include <Arduino.h>
#include <stdint.h>

// Maximum length of vendor name
#define OUI_NAME_MAX_LEN 32

// OUI (Organizationally Unique Identifier) entry structure
struct OUIEntry {
    uint8_t prefix[3];      // First 3 bytes of MAC address
    const char* vendor;     // Vendor name
};

// OUI Lookup class - provides vendor identification from MAC addresses
class OUILookup {
public:
    OUILookup();

    // Look up vendor from MAC address
    // Returns vendor name if found, empty string if not found
    String lookupVendor(const uint8_t* mac);

    // Look up vendor from MAC address string
    String lookupVendor(const String& macStr);

    // Check if online lookup is available
    bool isOnlineAvailable() const { return _onlineEnabled; }

    // Enable/disable online lookups
    void setOnlineEnabled(bool enabled) { _onlineEnabled = enabled; }

    // Process online lookup results (call in loop)
    void update();

private:
    bool _onlineEnabled;
    unsigned long _lastOnlineCheck;
    uint8_t _pendingMAC[6];
    bool _hasPendingLookup;

    // Local OUI database lookup
    String lookupLocal(const uint8_t* mac);

    // Online API lookup (macvendors.com or similar)
    void lookupOnline(const uint8_t* mac);

    // Extract OUI prefix from MAC address
    void getOUIPrefix(const uint8_t* mac, char* prefix, size_t len);
};

extern OUILookup ouiLookup;

#endif // OUI_LOOKUP_H
