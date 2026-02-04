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
#include "network_scanner.h"
#include <ESPmDNS.h>

// Forward declarations
void onClientConnect(uint8_t* mac, IPAddress ip);
void onClientDisconnect(uint8_t* mac);
void onPacketReceived(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload);
void processEventQueues();

// Event queue for cross-task communication
// WiFi events run on a different task than loop().
// We queue connect events here and process them safely in loop().

struct ConnectEvent {
    uint8_t mac[6];
    IPAddress ip;
};

static const int EVENT_QUEUE_SIZE = 8;

static ConnectEvent connectQueue[EVENT_QUEUE_SIZE];
static volatile int connectHead = 0;
static volatile int connectTail = 0;

// Global mutex for shared data (defined here, declared extern in config.h)
SemaphoreHandle_t dataMutex = NULL;

// Performance metrics (defined here, declared extern in config.h)
volatile uint32_t loopsPerSecond = 0;
static uint32_t _loopCount = 0;
static uint32_t _lastLoopCalcTime = 0;

void setup() {
    // Initialize serial
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    // Create mutex before anything else
    dataMutex = xSemaphoreCreateMutex();

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
    DEBUG_PRINTLN("[1/8] Initializing storage...");
    // Storage is automatically initialized when used

    // Step 2: Initialize WiFi (AP + STA)
    DEBUG_PRINTLN("[2/8] Starting WiFi...");
    wifiMgr.onClientConnect(onClientConnect);
    wifiMgr.onClientDisconnect(onClientDisconnect);

    if (!wifiMgr.begin()) {
        DEBUG_PRINTLN("ERROR: WiFi initialization failed!");
        DEBUG_PRINTLN("Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }

    // Step 3: Initialize NAT/routing
    DEBUG_PRINTLN("[3/8] Enabling NAT...");
    delay(1000);  // Wait for interfaces to stabilize

    if (!natEngine.begin()) {
        DEBUG_PRINTLN("WARNING: NAT initialization failed");
        DEBUG_PRINTLN("Internet access may not work for connected devices");
    }

    // Register callback for bandwidth tracking
    natEngine.setPacketCallback(onPacketReceived);

    // Step 4: Initialize bandwidth tracker
    DEBUG_PRINTLN("[4/8] Starting bandwidth tracker...");
    bandwidthTracker.begin();

    // Step 5: Initialize DNS server
    DEBUG_PRINTLN("[5/8] Starting DNS server...");
    if (!dnsServer.begin()) {
        DEBUG_PRINTLN("WARNING: DNS server failed to start");
    }

    // Step 6: Initialize web server
    DEBUG_PRINTLN("[6/8] Starting web server...");
    if (!webDashboard.begin()) {
        DEBUG_PRINTLN("WARNING: Web server failed to start");
    }

    // Step 7: Initialize mDNS
    DEBUG_PRINTLN("[7/8] Starting mDNS...");
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        DEBUG_PRINTF("mDNS: http://%s.local\n", MDNS_HOSTNAME);
    } else {
        DEBUG_PRINTLN("WARNING: mDNS failed to start");
    }

    // Step 8: Initialize network scanner (ARP + mDNS device discovery)
    DEBUG_PRINTLN("[8/8] Starting network scanner...");
    networkScanner.begin();

    // Print summary
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("  System Ready!");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("AP SSID:     %s\n", AP_SSID);
    DEBUG_PRINTF("AP Password: %s\n", AP_PASSWORD);
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("Dashboard Access:");
    DEBUG_PRINTF("  From AP:     http://%s\n", wifiMgr.getAPIP().toString().c_str());

    if (wifiMgr.isConnectedToRouter()) {
        DEBUG_PRINTF("  From WiFi:   http://%s.local\n", MDNS_HOSTNAME);
        DEBUG_PRINTF("               http://%s\n", wifiMgr.getSTAIP().toString().c_str());
        DEBUG_PRINTF("Router:      Connected to '%s'\n", wifiMgr.getSTASSID().c_str());
    } else {
        DEBUG_PRINTLN("  From WiFi:   (not connected to router)");
        DEBUG_PRINTLN("Router:      Not connected");
        DEBUG_PRINTLN("             Configure via dashboard");
    }

    DEBUG_PRINTLN("========================================\n");
}

void loop() {
    // All data structure modifications happen under the mutex.
    // The async web server task also takes this mutex when reading data.
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    processEventQueues();
    bandwidthTracker.update();
    networkScanner.update();
    xSemaphoreGive(dataMutex);

    // DNS processing runs outside the mutex.
    // The DNS server internally takes the mutex only for brief blocklist checks,
    // so the potentially blocking DNS forwarding (up to 3s) doesn't hold the mutex.
    dnsServer.processRequests();

    // Calculate loop frequency (loops per second)
    _loopCount++;
    uint32_t now = millis();
    if (now - _lastLoopCalcTime >= 1000) {
        loopsPerSecond = _loopCount;
        _loopCount = 0;
        _lastLoopCalcTime = now;
    }

    // Yield to other tasks without artificial delay
    yield();
}

// Process queued events from other tasks (called from main loop only)
void processEventQueues() {
    // Process client connect events (from WiFi event task)
    while (connectTail != connectHead) {
        deviceManager.updateDevice(
            connectQueue[connectTail].mac,
            connectQueue[connectTail].ip
        );
        connectTail = (connectTail + 1) % EVENT_QUEUE_SIZE;
    }
}

// Callback when a client connects to AP (runs on WiFi event task - not main loop)
void onClientConnect(uint8_t* mac, IPAddress ip) {
    DEBUG_PRINTF("Client connected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Queue for main loop processing (don't modify DeviceManager here)
    int next = (connectHead + 1) % EVENT_QUEUE_SIZE;
    if (next != connectTail) {
        memcpy(connectQueue[connectHead].mac, mac, 6);
        connectQueue[connectHead].ip = ip;
        connectHead = next;
    }
}

// Callback when a client disconnects from AP
void onClientDisconnect(uint8_t* mac) {
    DEBUG_PRINTF("Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Callback for packet interception (from WiFi task via promiscuous mode)
void onPacketReceived(const uint8_t* srcMac, const uint8_t* dstMac, uint16_t length, bool isUpload) {
    // recordPacket uses its own ring buffer - safe from WiFi task
    bandwidthTracker.recordPacket(srcMac, dstMac, length, isUpload);
}
