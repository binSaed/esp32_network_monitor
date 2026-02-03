#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class WebDashboard {
public:
    WebDashboard();

    bool begin();
    void stop();

private:
    AsyncWebServer server;

    // Route handlers
    void setupRoutes();

    // API handlers
    void handleGetDevices(AsyncWebServerRequest* request);
    void handleSetDeviceName(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleResetDevice(AsyncWebServerRequest* request);
    void handleResetAllStats(AsyncWebServerRequest* request);
    void handleGetBlockedDomains(AsyncWebServerRequest* request);
    void handleBlockDomain(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleUnblockDomain(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleSetDNS(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleSetWiFi(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetSettings(AsyncWebServerRequest* request);

    // Utility
    String getContentType(const String& path);
};

extern WebDashboard webDashboard;

#endif // WEB_SERVER_H
