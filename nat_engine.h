#ifndef NAT_ENGINE_H
#define NAT_ENGINE_H

#include <Arduino.h>
#include "config.h"

// Callback for packet interception (for bandwidth tracking)
typedef void (*PacketCallback)(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload);

class NATEngine {
public:
    NATEngine();

    // Initialize NAT/routing
    bool begin();

    // Register callback for packet counting
    void setPacketCallback(PacketCallback callback);

    // Status
    bool isEnabled();

    // Public for callback access
    static PacketCallback packetCallback;

private:
    bool enabled;
};

extern NATEngine natEngine;

#endif // NAT_ENGINE_H
