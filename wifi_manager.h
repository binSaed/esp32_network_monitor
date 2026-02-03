#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

enum WiFiStatus {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_AP_STARTED
};

class WiFiManager {
public:
    WiFiManager();

    // Setup
    bool begin();
    bool connectToRouter(const String& ssid, const String& password);
    bool startAccessPoint();

    // Status
    bool isConnectedToRouter();
    bool isAPRunning();
    WiFiStatus getStatus();

    // Info
    IPAddress getSTAIP();
    IPAddress getAPIP();
    String getSTASSID();
    int getConnectedClients();

    // Management
    void disconnect();
    void reconnect();

    // Event callbacks
    void onClientConnect(void (*callback)(uint8_t* mac, IPAddress ip));
    void onClientDisconnect(void (*callback)(uint8_t* mac));

private:
    String staSSID;
    String staPassword;
    WiFiStatus status;

    void (*clientConnectCallback)(uint8_t* mac, IPAddress ip);
    void (*clientDisconnectCallback)(uint8_t* mac);

    static void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);
    static WiFiManager* instance;
};

extern WiFiManager wifiMgr;

#endif // WIFI_MANAGER_H
