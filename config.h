#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// ESP32 Network Monitor Configuration
// ============================================

// --- WiFi Station (Connection to Home Router) ---
#define DEFAULT_STA_SSID     "Abdelrahman"
#define DEFAULT_STA_PASSWORD "Abdo546453@#@"

// --- WiFi Access Point (For Home Devices) ---
#define AP_SSID              "Abdelrahman_Monitor"
#define AP_PASSWORD          "Abdo546453@#@"
#define AP_CHANNEL           6
#define AP_MAX_CONNECTIONS   8

// --- Network Configuration ---
#define AP_IP                IPAddress(192, 168, 4, 1)
#define AP_GATEWAY           IPAddress(192, 168, 4, 1)
#define AP_SUBNET            IPAddress(255, 255, 255, 0)

// --- DNS Configuration ---
#define DNS_PORT             53
#define DEFAULT_UPSTREAM_DNS IPAddress(1, 1, 1, 1)  // Cloudflare
#define DNS_TIMEOUT_MS       1000  // Reduced from 3000ms for faster fallback

// --- Web Server ---
#define WEB_SERVER_PORT      80

// --- mDNS (Local Domain) ---
#define MDNS_HOSTNAME        "networkmonitor"  // Access via http://networkmonitor.local

// --- DNS Async Forwarding & Cache ---
#define DNS_CACHE_SIZE         16
#define DNS_CACHE_TTL_MS       60000   // Cache TTL: 60 seconds
#define DNS_FORWARD_QUEUE_SIZE 16
#define DNS_TASK_STACK_SIZE    8192
#define DNS_TASK_PRIORITY      2

// --- Device Limits ---
#define MAX_DEVICES          16
#define MAX_BLOCKED_DOMAINS  100
#define MAX_DOMAIN_LENGTH    64
#define MAX_DEVICE_NAME      32

// --- Timing ---
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define STATS_SAVE_INTERVAL_MS    300000  // Save stats every 5 minutes
#define DEVICE_TIMEOUT_MS         3600000 // Consider device offline after 1 hour

// --- NVS Namespaces ---
#define NVS_NAMESPACE_WIFI    "wifi"
#define NVS_NAMESPACE_DEVICES "devices"
#define NVS_NAMESPACE_DNS     "dns"
#define NVS_NAMESPACE_STATS   "stats"

// --- Debug ---
#define DEBUG_SERIAL          true
#define SERIAL_BAUD           115200

#if DEBUG_SERIAL
  #define DEBUG_PRINT(x)      Serial.print(x)
  #define DEBUG_PRINTLN(x)    Serial.println(x)
  #define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// --- Thread Safety ---
// Mutex protecting shared data (device lists, bandwidth stats, blocklists)
// accessed from both the main loop and the async web server task.
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
extern SemaphoreHandle_t dataMutex;

// --- Performance Metrics ---
// Loop frequency (loops/second) calculated in main loop, read by web server.
extern volatile uint32_t loopsPerSecond;

#endif // CONFIG_H
