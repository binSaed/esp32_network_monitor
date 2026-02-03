#include "nat_engine.h"
#include <WiFi.h>
#include "esp_wifi.h"

// lwIP includes
extern "C" {
#include "lwip/netif.h"
#include "lwip/ip4.h"
}

NATEngine natEngine;
PacketCallback NATEngine::packetCallback = nullptr;

// Promiscuous mode callback for packet sniffing
static void IRAM_ATTR promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA || !NATEngine::packetCallback) {
        return;
    }

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    wifi_pkt_rx_ctrl_t& ctrl = pkt->rx_ctrl;

    // Only process data frames with sufficient length
    if (ctrl.sig_len < 24) {  // Minimum 802.11 header
        return;
    }

    // 802.11 frame header structure:
    // Bytes 0-1: Frame control
    // Bytes 2-3: Duration
    // Bytes 4-9: Address 1 (destination/receiver)
    // Bytes 10-15: Address 2 (source/transmitter)
    // Bytes 16-21: Address 3 (BSSID or other)
    // Bytes 22-23: Sequence control

    const uint8_t* frame = pkt->payload;
    uint16_t frameCtrl = frame[0] | (frame[1] << 8);

    // Check if it's a data frame (Type = 2)
    uint8_t frameType = (frameCtrl >> 2) & 0x03;
    if (frameType != 2) {
        return;
    }

    // Extract To DS and From DS bits
    bool toDS = (frameCtrl >> 8) & 0x01;
    bool fromDS = (frameCtrl >> 9) & 0x01;

    const uint8_t* srcMac = nullptr;
    const uint8_t* dstMac = nullptr;
    bool isUpload = false;

    // Determine source/destination based on DS bits
    // For AP mode:
    // ToDS=1, FromDS=0: Client to AP (upload from client perspective)
    // ToDS=0, FromDS=1: AP to client (download from client perspective)

    if (toDS && !fromDS) {
        // To AP (upload)
        dstMac = &frame[4];   // Address 1 = BSSID
        srcMac = &frame[10];  // Address 2 = Source
        isUpload = true;
    } else if (!toDS && fromDS) {
        // From AP (download)
        dstMac = &frame[4];   // Address 1 = Destination
        srcMac = &frame[10];  // Address 2 = BSSID
        isUpload = false;
    } else {
        return;  // Skip other frame types
    }

    // Calculate payload length (excluding 802.11 overhead)
    uint16_t payloadLen = ctrl.sig_len > 36 ? ctrl.sig_len - 36 : 0;

    NATEngine::packetCallback(srcMac, dstMac, payloadLen, isUpload);
}

NATEngine::NATEngine() : enabled(false) {}

bool NATEngine::begin() {
    DEBUG_PRINTLN("NAT: Initializing...");

    // Enable NAT using WiFi library method (ESP32 Arduino Core 3.x)
    // The WiFi.enableNAPT() or similar might be available
    // For now, we rely on the default routing behavior

    // In ESP32 AP+STA mode, basic routing works automatically
    // For full NAPT, you may need to enable it in sdkconfig or use esp-idf directly
    DEBUG_PRINTLN("NAT: Using default AP+STA routing");
    DEBUG_PRINTLN("NAT: Note: For full internet access, NAPT may need sdkconfig changes");

    // Enable promiscuous mode for packet counting
    // This works independently of NAPT
    esp_err_t err = esp_wifi_set_promiscuous(true);
    if (err != ESP_OK) {
        DEBUG_PRINTF("NAT: Failed to enable promiscuous mode: %d\n", err);
        return false;
    }

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA
    };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);

    DEBUG_PRINTLN("NAT: Promiscuous mode enabled for bandwidth tracking");

    enabled = true;
    return true;
}

void NATEngine::setPacketCallback(PacketCallback callback) {
    packetCallback = callback;
}

bool NATEngine::isEnabled() {
    return enabled;
}
