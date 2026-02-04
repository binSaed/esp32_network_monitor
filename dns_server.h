#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "config.h"

// DNS forward request (main loop -> forwarding task)
struct DNSForwardRequest {
    uint8_t packet[512];
    int packetLen;
    uint32_t clientIP;
    uint16_t clientPort;
    char domain[MAX_DOMAIN_LENGTH + 1];
};

// DNS forward response (forwarding task -> main loop)
struct DNSForwardResponse {
    uint8_t packet[512];
    int packetLen;
    uint32_t clientIP;
    uint16_t clientPort;
    char domain[MAX_DOMAIN_LENGTH + 1];
};

// DNS cache entry
struct DNSCacheEntry {
    char domain[MAX_DOMAIN_LENGTH + 1];
    uint8_t response[512];
    int responseLen;
    uint32_t timestamp;
    bool valid;
};

class DNSBlockingServer {
public:
    DNSBlockingServer();

    // Control
    bool begin();
    void stop();
    void processRequests();  // Call in loop()

    // Domain Blocking
    bool addBlockedDomain(const String& domain);
    bool removeBlockedDomain(const String& domain);
    bool isBlocked(const String& domain);
    std::vector<String> getBlockedDomains();
    void clearBlockedDomains();

    // DNS Configuration
    void setUpstreamDNS(const IPAddress& dns);
    IPAddress getUpstreamDNS();

    // Stats
    uint32_t getQueryCount();
    uint32_t getBlockedCount();
    uint32_t getCacheHits();

private:
    WiFiUDP udp;
    IPAddress upstreamDNS;
    std::vector<String> blockedDomains;
    uint32_t queryCount;
    uint32_t blockedCount;
    uint32_t cacheHits;
    bool running;

    // Async forwarding
    QueueHandle_t _requestQueue;
    QueueHandle_t _responseQueue;
    TaskHandle_t _forwardTask;

    // DNS cache
    DNSCacheEntry _cache[DNS_CACHE_SIZE];

    // DNS packet handling
    void handleDNSRequest();
    bool parseDomainName(const uint8_t* buffer, int bufferLen, int& offset, String& domain);
    void sendBlockedResponse(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort);
    void queueForwardRequest(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort, const String& domain);
    void drainResponseQueue();

    // Cache
    DNSCacheEntry* cacheLookup(const char* domain);
    void cacheStore(const char* domain, const uint8_t* response, int responseLen);

    // Domain matching
    bool domainMatches(const String& queryDomain, const String& blockedDomain);
    String normalizeDomain(const String& domain);

    // Forwarding task (runs on separate FreeRTOS task)
    static void forwardTaskFunc(void* param);
};

extern DNSBlockingServer dnsServer;

#endif // DNS_SERVER_H
