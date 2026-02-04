#include "web_server.h"
#include "web_content.h"
#include "bandwidth_tracker.h"
#include "device_manager.h"
#include "dns_server.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include <ArduinoJson.h>

WebDashboard webDashboard;

WebDashboard::WebDashboard() : server(WEB_SERVER_PORT) {}

bool WebDashboard::begin() {
    setupRoutes();
    server.begin();
    DEBUG_PRINTF("Web: Server started on port %d\n", WEB_SERVER_PORT);
    return true;
}

void WebDashboard::stop() {
    server.end();
    DEBUG_PRINTLN("Web: Server stopped");
}

void WebDashboard::setupRoutes() {
    // Serve dashboard
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // API: Get devices
    server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetDevices(request);
    });

    // API: Set device name (with body handler)
    server.on("/api/devices/name", HTTP_POST, [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                handleSetDeviceName(request, data, len);
            }
        });

    // API: Reset device stats - use regex pattern
    server.on("^\\/api\\/devices\\/([A-Fa-f0-9:]+)\\/reset$", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleResetDevice(request);
    });

    // API: Set device name - use regex pattern
    server.on("^\\/api\\/devices\\/([A-Fa-f0-9:]+)\\/name$", HTTP_POST, [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                handleSetDeviceName(request, data, len);
            }
        });

    // API: Reset all stats
    server.on("/api/stats/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleResetAllStats(request);
    });

    // API: Get blocked domains
    server.on("/api/blockedDomains", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetBlockedDomains(request);
    });

    // API: Block domain
    server.on("/api/blockDomain", HTTP_POST, [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                handleBlockDomain(request, data, len);
            }
        });

    // API: Unblock domain
    server.on("/api/unblockDomain", HTTP_POST, [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                handleUnblockDomain(request, data, len);
            }
        });

    // API: Get status
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });

    // API: Get settings
    server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetSettings(request);
    });

    // API: Set DNS
    server.on("/api/settings/dns", HTTP_POST, [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                handleSetDNS(request, data, len);
            }
        });

    // API: Set WiFi
    server.on("/api/settings/wifi", HTTP_POST, [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                handleSetWiFi(request, data, len);
            }
        });

    // 404 handler
    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "application/json", "{\"error\":\"Not found\"}");
    });
}

void WebDashboard::handleGetDevices(AsyncWebServerRequest* request) {
    String response;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    {
        std::vector<DeviceStats> stats = bandwidthTracker.getAllStats();

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (const auto& s : stats) {
            JsonObject obj = arr.add<JsonObject>();
            obj["mac"] = deviceManager.macToString(s.mac);
            obj["name"] = deviceManager.getDeviceName(s.mac);
            obj["upload"] = s.uploadBytes;
            obj["download"] = s.downloadBytes;
            obj["total"] = s.totalBytes();
            obj["active"] = s.active;
        }

        serializeJson(doc, response);
    }
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", response);
}

void WebDashboard::handleSetDeviceName(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // Get MAC from URL path
    String url = request->url();
    int start = url.indexOf("/devices/") + 9;
    int end = url.indexOf("/name");
    String macStr = url.substring(start, end);

    // URL decode the MAC address
    macStr.replace("%3A", ":");

    uint8_t mac[6];
    if (!deviceManager.parseMAC(macStr, mac)) {
        request->send(400, "application/json", "{\"error\":\"Invalid MAC\"}");
        return;
    }

    String name = doc["name"] | "";

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    deviceManager.setDeviceName(mac, name);
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", "{\"success\":true}");
}

void WebDashboard::handleResetDevice(AsyncWebServerRequest* request) {
    // Get MAC from URL path
    String url = request->url();
    int start = url.indexOf("/devices/") + 9;
    int end = url.indexOf("/reset");
    String macStr = url.substring(start, end);

    // URL decode
    macStr.replace("%3A", ":");

    uint8_t mac[6];
    if (!deviceManager.parseMAC(macStr, mac)) {
        request->send(400, "application/json", "{\"error\":\"Invalid MAC\"}");
        return;
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    bandwidthTracker.resetDeviceStats(mac);
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", "{\"success\":true}");
}

void WebDashboard::handleResetAllStats(AsyncWebServerRequest* request) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    bandwidthTracker.resetAllStats();
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", "{\"success\":true}");
}

void WebDashboard::handleGetBlockedDomains(AsyncWebServerRequest* request) {
    String response;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    {
        std::vector<String> domains = dnsServer.getBlockedDomains();

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (const auto& d : domains) {
            arr.add(d);
        }

        serializeJson(doc, response);
    }
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", response);
}

void WebDashboard::handleBlockDomain(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String domain = doc["domain"] | "";
    if (domain.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"Domain required\"}");
        return;
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    bool ok = dnsServer.addBlockedDomain(domain);
    xSemaphoreGive(dataMutex);

    if (ok) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"error\":\"Failed to add domain\"}");
    }
}

void WebDashboard::handleUnblockDomain(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String domain = doc["domain"] | "";
    if (domain.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"Domain required\"}");
        return;
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    bool ok = dnsServer.removeBlockedDomain(domain);
    xSemaphoreGive(dataMutex);

    if (ok) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"error\":\"Domain not found\"}");
    }
}

void WebDashboard::handleGetStatus(AsyncWebServerRequest* request) {
    String response;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    {
        JsonDocument doc;

        doc["connected"] = wifiMgr.isConnectedToRouter();
        doc["staIP"] = wifiMgr.isConnectedToRouter() ? wifiMgr.getSTAIP().toString() : "";
        doc["apIP"] = wifiMgr.getAPIP().toString();
        doc["ssid"] = wifiMgr.getSTASSID();
        doc["clients"] = wifiMgr.getConnectedClients();
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["minFreeHeap"] = ESP.getMinFreeHeap();
        doc["cpuFreq"] = ESP.getCpuFreqMHz();
        doc["loopFreq"] = loopsPerSecond;
        doc["upstreamDNS"] = dnsServer.getUpstreamDNS().toString();
        doc["dnsQueries"] = dnsServer.getQueryCount();
        doc["dnsBlocked"] = dnsServer.getBlockedCount();
        doc["mdnsHost"] = MDNS_HOSTNAME;

        serializeJson(doc, response);
    }
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", response);
}

void WebDashboard::handleGetSettings(AsyncWebServerRequest* request) {
    String response;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    {
        JsonDocument doc;

        doc["upstreamDNS"] = dnsServer.getUpstreamDNS().toString();
        doc["staSSID"] = wifiMgr.getSTASSID();
        doc["connected"] = wifiMgr.isConnectedToRouter();

        serializeJson(doc, response);
    }
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", response);
}

void WebDashboard::handleSetDNS(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String dnsStr = doc["dns"] | "";
    if (dnsStr.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"DNS required\"}");
        return;
    }

    IPAddress dns;
    if (!dns.fromString(dnsStr)) {
        request->send(400, "application/json", "{\"error\":\"Invalid IP\"}");
        return;
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    dnsServer.setUpstreamDNS(dns);
    xSemaphoreGive(dataMutex);

    request->send(200, "application/json", "{\"success\":true}");
}

void WebDashboard::handleSetWiFi(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";

    if (ssid.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"SSID required\"}");
        return;
    }

    // Respond before connecting (connection takes time)
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Connecting...\"}");

    // Connect in next loop iteration
    delay(100);
    wifiMgr.connectToRouter(ssid, password);
}

String WebDashboard::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    return "text/plain";
}
