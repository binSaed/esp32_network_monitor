#include "wifi_manager.h"
#include "storage_manager.h"

WiFiManager wifiMgr;
WiFiManager* WiFiManager::instance = nullptr;

WiFiManager::WiFiManager() :
    status(WIFI_STATUS_DISCONNECTED),
    clientConnectCallback(nullptr),
    clientDisconnectCallback(nullptr) {
    instance = this;
}

bool WiFiManager::begin() {
    DEBUG_PRINTLN("WiFi: Initializing...");

    // Set dual mode (AP + STA)
    WiFi.mode(WIFI_AP_STA);

    // Register event handler
    WiFi.onEvent(wifiEventHandler);

    // Start Access Point first
    if (!startAccessPoint()) {
        DEBUG_PRINTLN("WiFi: Failed to start AP");
        return false;
    }

    // Load and connect to router
    String ssid, password;
    if (storage.loadSTACredentials(ssid, password) && ssid.length() > 0) {
        DEBUG_PRINTF("WiFi: Found stored credentials for '%s'\n", ssid.c_str());
        connectToRouter(ssid, password);
    } else {
        // Use default credentials
        DEBUG_PRINTLN("WiFi: Using default credentials");
        connectToRouter(DEFAULT_STA_SSID, DEFAULT_STA_PASSWORD);
    }

    return true;
}

bool WiFiManager::startAccessPoint() {
    DEBUG_PRINTF("WiFi: Starting AP '%s'...\n", AP_SSID);

    // Configure AP
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

    // Start AP
    bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);

    if (ok) {
        status = WIFI_STATUS_AP_STARTED;
        DEBUG_PRINTF("WiFi: AP started at %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        DEBUG_PRINTLN("WiFi: AP start failed");
    }

    return ok;
}

bool WiFiManager::connectToRouter(const String& ssid, const String& password) {
    if (ssid.length() == 0) {
        DEBUG_PRINTLN("WiFi: Empty SSID");
        return false;
    }

    staSSID = ssid;
    staPassword = password;
    status = WIFI_STATUS_CONNECTING;

    DEBUG_PRINTF("WiFi: Connecting to '%s'...\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        status = WIFI_STATUS_CONNECTED;
        DEBUG_PRINTF("WiFi: Connected! IP: %s\n", WiFi.localIP().toString().c_str());

        // Save working credentials
        storage.saveSTACredentials(ssid, password);
        return true;
    }

    DEBUG_PRINTLN("WiFi: Connection failed");
    status = WIFI_STATUS_DISCONNECTED;
    return false;
}

bool WiFiManager::isConnectedToRouter() {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isAPRunning() {
    return WiFi.getMode() & WIFI_AP;
}

WiFiStatus WiFiManager::getStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        return WIFI_STATUS_CONNECTED;
    }
    return status;
}

IPAddress WiFiManager::getSTAIP() {
    return WiFi.localIP();
}

IPAddress WiFiManager::getAPIP() {
    return WiFi.softAPIP();
}

String WiFiManager::getSTASSID() {
    return staSSID;
}

int WiFiManager::getConnectedClients() {
    return WiFi.softAPgetStationNum();
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    status = WIFI_STATUS_DISCONNECTED;
}

void WiFiManager::reconnect() {
    if (staSSID.length() > 0) {
        connectToRouter(staSSID, staPassword);
    }
}

void WiFiManager::onClientConnect(void (*callback)(uint8_t* mac, IPAddress ip)) {
    clientConnectCallback = callback;
}

void WiFiManager::onClientDisconnect(void (*callback)(uint8_t* mac)) {
    clientDisconnectCallback = callback;
}

void WiFiManager::wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
            DEBUG_PRINTF("WiFi: Client connected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);

            if (instance && instance->clientConnectCallback) {
                // Note: IP might not be assigned yet
                instance->clientConnectCallback(info.wifi_ap_staconnected.mac, IPAddress(0, 0, 0, 0));
            }
            break;
        }

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
            DEBUG_PRINTF("WiFi: Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);

            if (instance && instance->clientDisconnectCallback) {
                instance->clientDisconnectCallback(info.wifi_ap_stadisconnected.mac);
            }
            break;
        }

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            DEBUG_PRINTLN("WiFi: Disconnected from router");
            if (instance) {
                instance->status = WIFI_STATUS_DISCONNECTED;
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            DEBUG_PRINTF("WiFi: Got IP: %s\n", WiFi.localIP().toString().c_str());
            if (instance) {
                instance->status = WIFI_STATUS_CONNECTED;
            }
            break;

        default:
            break;
    }
}
