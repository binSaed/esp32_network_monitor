#include "nat_engine.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_idf_version.h"

// lwIP includes for NAPT
extern "C" {
#include "lwip/lwip_napt.h"
#include "lwip/tcpip.h"  // For LOCK_TCPIP_CORE
}

NATEngine natEngine;
PacketCallback NATEngine::packetCallback = nullptr;

// Promiscuous mode callback for packet sniffing - kept minimal for throughput
static void IRAM_ATTR promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA || !NATEngine::packetCallback) {
        return;
    }

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint16_t sig_len = pkt->rx_ctrl.sig_len;

    if (sig_len < 24) return;  // Minimum 802.11 header

    const uint8_t* frame = pkt->payload;
    uint16_t frameCtrl = frame[0] | (frame[1] << 8);

    // Data frames only (Type = 2)
    if (((frameCtrl >> 2) & 0x03) != 2) return;

    // Extract To DS and From DS bits
    bool toDS = (frameCtrl >> 8) & 0x01;
    bool fromDS = (frameCtrl >> 9) & 0x01;

    const uint8_t* srcMac;
    const uint8_t* dstMac;
    bool isUpload;

    if (toDS && !fromDS) {
        // Client -> AP (upload)
        dstMac = &frame[4];
        srcMac = &frame[10];
        isUpload = true;
    } else if (!toDS && fromDS) {
        // AP -> Client (download)
        dstMac = &frame[4];
        srcMac = &frame[10];
        isUpload = false;
    } else {
        return;
    }

    uint16_t payloadLen = sig_len > 36 ? sig_len - 36 : 0;
    NATEngine::packetCallback(srcMac, dstMac, payloadLen, isUpload);
}

NATEngine::NATEngine() : enabled(false) {}

bool NATEngine::begin() {
    DEBUG_PRINTLN("NAT: Initializing...");

    // Enable NAPT for internet forwarding
    // Must lock TCPIP core when calling lwIP functions in ESP-IDF 5.x
    IPAddress apIP = WiFi.softAPIP();

    LOCK_TCPIP_CORE();
    ip_napt_enable((u32_t)apIP, 1);
    UNLOCK_TCPIP_CORE();

    DEBUG_PRINTF("NAT: NAPT enabled on %s\n", apIP.toString().c_str());

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
