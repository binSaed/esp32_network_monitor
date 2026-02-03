#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <vector>
#include "config.h"

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

private:
    WiFiUDP udp;
    IPAddress upstreamDNS;
    std::vector<String> blockedDomains;
    uint32_t queryCount;
    uint32_t blockedCount;
    bool running;

    // DNS packet handling
    void handleDNSRequest();
    bool parseDomainName(const uint8_t* buffer, int bufferLen, int& offset, String& domain);
    void sendBlockedResponse(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort);
    void forwardRequest(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort);

    // Domain matching
    bool domainMatches(const String& queryDomain, const String& blockedDomain);
    String normalizeDomain(const String& domain);
};

extern DNSBlockingServer dnsServer;

#endif // DNS_SERVER_H
