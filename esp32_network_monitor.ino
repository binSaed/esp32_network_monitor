/*
 * ESP32 Home Network Monitor & DNS Blocker
 *
 * Features:
 * - Per-device bandwidth tracking
 * - DNS-based domain blocking
 * - Web dashboard for monitoring and control
 *
 * Hardware: ESP32
 * Framework: Arduino
 *
 * Required Libraries:
 * - ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)
 * - AsyncTCP (https://github.com/me-no-dev/AsyncTCP)
 * - ArduinoJson (https://arduinojson.org/)
 *
 * Network Architecture:
 * Internet <-> Router <-> ESP32 (STA) <-> ESP32 (AP) <-> Home Devices
 *
 * Usage:
 * 1. Update config.h with your router's WiFi credentials
 * 2. Upload to ESP32
 * 3. Connect devices to ESP32's WiFi (default: ESP32_Monitor / monitor123)
 * 4. Access dashboard at http://192.168.4.1
 */

#include "config.h"
#include "storage_manager.h"
#include "wifi_manager.h"
#include "nat_engine.h"
#include "dns_server.h"
#include "bandwidth_tracker.h"
#include "device_manager.h"
#include "web_server.h"

// Forward declarations
void onClientConnect(uint8_t* mac, IPAddress ip);
void onClientDisconnect(uint8_t* mac);
void onPacketReceived(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload);

void setup() {
    // Initialize serial
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("  ESP32 Network Monitor & DNS Blocker");
    DEBUG_PRINTLN("========================================\n");

    // Print chip info
    DEBUG_PRINTF("Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    DEBUG_PRINTF("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
    DEBUG_PRINTF("Free Heap: %d bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTF("Flash Size: %d bytes\n", ESP.getFlashChipSize());
    DEBUG_PRINTLN("");

    // Step 1: Initialize storage
    DEBUG_PRINTLN("[1/6] Initializing storage...");
    // Storage is automatically initialized when used

    // Step 2: Initialize WiFi (AP + STA)
    DEBUG_PRINTLN("[2/6] Starting WiFi...");
    wifiMgr.onClientConnect(onClientConnect);
    wifiMgr.onClientDisconnect(onClientDisconnect);

    if (!wifiMgr.begin()) {
        DEBUG_PRINTLN("ERROR: WiFi initialization failed!");
        DEBUG_PRINTLN("Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }

    // Step 3: Initialize NAT/routing
    DEBUG_PRINTLN("[3/6] Enabling NAT...");
    delay(1000);  // Wait for interfaces to stabilize

    if (!natEngine.begin()) {
        DEBUG_PRINTLN("WARNING: NAT initialization failed");
        DEBUG_PRINTLN("Internet access may not work for connected devices");
    }

    // Register packet callback for bandwidth tracking
    natEngine.setPacketCallback(onPacketReceived);

    // Step 4: Initialize bandwidth tracker
    DEBUG_PRINTLN("[4/6] Starting bandwidth tracker...");
    bandwidthTracker.begin();

    // Step 5: Initialize DNS server
    DEBUG_PRINTLN("[5/6] Starting DNS server...");
    if (!dnsServer.begin()) {
        DEBUG_PRINTLN("WARNING: DNS server failed to start");
    }

    // Step 6: Initialize web server
    DEBUG_PRINTLN("[6/6] Starting web server...");
    if (!webDashboard.begin()) {
        DEBUG_PRINTLN("WARNING: Web server failed to start");
    }

    // Print summary
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("  System Ready!");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("AP SSID:     %s\n", AP_SSID);
    DEBUG_PRINTF("AP Password: %s\n", AP_PASSWORD);
    DEBUG_PRINTF("Dashboard:   http://%s\n", wifiMgr.getAPIP().toString().c_str());
    DEBUG_PRINTLN("");

    if (wifiMgr.isConnectedToRouter()) {
        DEBUG_PRINTF("Router:      Connected (%s)\n", wifiMgr.getSTAIP().toString().c_str());
    } else {
        DEBUG_PRINTLN("Router:      Not connected");
        DEBUG_PRINTLN("             Configure via dashboard");
    }

    DEBUG_PRINTLN("========================================\n");
}

void loop() {
    // Process DNS requests
    dnsServer.processRequests();

    // Update bandwidth tracker (periodic save, cleanup)
    bandwidthTracker.update();

    // Small delay to prevent watchdog issues
    delay(1);
}

// Callback when a client connects to AP
void onClientConnect(uint8_t* mac, IPAddress ip) {
    DEBUG_PRINTF("Client connected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Update device manager
    deviceManager.updateDevice(mac, ip);
}

// Callback when a client disconnects from AP
void onClientDisconnect(uint8_t* mac) {
    DEBUG_PRINTF("Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Callback for packet interception (from NAT engine)
void onPacketReceived(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload) {
    bandwidthTracker.recordPacket(srcMac, dstMac, length, isUpload);
}
