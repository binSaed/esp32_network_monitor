#include "dns_server.h"
#include "storage_manager.h"

DNSBlockingServer dnsServer;

// DNS Header structure
struct DNSHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

DNSBlockingServer::DNSBlockingServer() :
    upstreamDNS(DEFAULT_UPSTREAM_DNS),
    queryCount(0),
    blockedCount(0),
    running(false) {}

bool DNSBlockingServer::begin() {
    // Load upstream DNS from storage
    upstreamDNS = storage.loadUpstreamDNS();

    // Load blocked domains from storage
    blockedDomains = storage.loadBlockedDomains();

    // Start UDP listener on port 53
    if (!udp.begin(DNS_PORT)) {
        DEBUG_PRINTLN("DNS: Failed to start UDP server");
        return false;
    }

    running = true;
    DEBUG_PRINTF("DNS: Server started on port %d, upstream: %s\n",
                 DNS_PORT, upstreamDNS.toString().c_str());
    DEBUG_PRINTF("DNS: %d blocked domains loaded\n", blockedDomains.size());

    return true;
}

void DNSBlockingServer::stop() {
    udp.stop();
    running = false;
    DEBUG_PRINTLN("DNS: Server stopped");
}

void DNSBlockingServer::processRequests() {
    if (!running) return;

    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        handleDNSRequest();
    }
}

void DNSBlockingServer::handleDNSRequest() {
    uint8_t buffer[512];
    int len = udp.read(buffer, sizeof(buffer));

    if (len < (int)sizeof(DNSHeader)) {
        return;
    }

    queryCount++;

    // Parse DNS header
    DNSHeader* header = (DNSHeader*)buffer;

    // Only handle standard queries (QR=0, Opcode=0)
    uint16_t flags = ntohs(header->flags);
    if ((flags & 0x8000) != 0 || ((flags >> 11) & 0x0F) != 0) {
        return;
    }

    uint16_t qdcount = ntohs(header->qdcount);
    if (qdcount < 1) {
        return;
    }

    // Parse the query domain name
    int offset = sizeof(DNSHeader);
    String domain;
    if (!parseDomainName(buffer, len, offset, domain)) {
        return;
    }

    domain = normalizeDomain(domain);
    DEBUG_PRINTF("DNS: Query for '%s' from %s\n", domain.c_str(), udp.remoteIP().toString().c_str());

    // Check if domain is blocked
    if (isBlocked(domain)) {
        DEBUG_PRINTF("DNS: BLOCKED '%s'\n", domain.c_str());
        blockedCount++;
        sendBlockedResponse(buffer, len, udp.remoteIP(), udp.remotePort());
    } else {
        forwardRequest(buffer, len, udp.remoteIP(), udp.remotePort());
    }
}

bool DNSBlockingServer::parseDomainName(const uint8_t* buffer, int bufferLen, int& offset, String& domain) {
    domain = "";
    int labelLen;

    while (offset < bufferLen && (labelLen = buffer[offset]) != 0) {
        if (labelLen > 63 || offset + labelLen >= bufferLen) {
            return false;  // Invalid label
        }

        if (domain.length() > 0) {
            domain += ".";
        }

        offset++;
        for (int i = 0; i < labelLen; i++) {
            domain += (char)buffer[offset + i];
        }
        offset += labelLen;
    }

    offset++;  // Skip null terminator
    return domain.length() > 0;
}

void DNSBlockingServer::sendBlockedResponse(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort) {
    uint8_t response[512];
    memcpy(response, request, requestLen);

    DNSHeader* header = (DNSHeader*)response;

    // Set response flags: QR=1 (response), AA=1, RCODE=0
    uint16_t flags = ntohs(header->flags);
    flags |= 0x8400;  // QR=1, AA=1
    header->flags = htons(flags);

    // Set answer count to 1
    header->ancount = htons(1);

    // Find end of question section
    int offset = sizeof(DNSHeader);
    while (offset < requestLen && response[offset] != 0) {
        offset += response[offset] + 1;
    }
    offset++;      // Skip null
    offset += 4;   // Skip QTYPE and QCLASS

    // Add answer: pointer to name (0xC00C), type A, class IN, TTL 300, RDLENGTH 4, RDATA 0.0.0.0
    response[offset++] = 0xC0;  // Pointer to name
    response[offset++] = 0x0C;
    response[offset++] = 0x00;  // Type A
    response[offset++] = 0x01;
    response[offset++] = 0x00;  // Class IN
    response[offset++] = 0x01;
    response[offset++] = 0x00;  // TTL (300 seconds)
    response[offset++] = 0x00;
    response[offset++] = 0x01;
    response[offset++] = 0x2C;
    response[offset++] = 0x00;  // RDLENGTH (4 bytes)
    response[offset++] = 0x04;
    response[offset++] = 0x00;  // 0.0.0.0
    response[offset++] = 0x00;
    response[offset++] = 0x00;
    response[offset++] = 0x00;

    udp.beginPacket(client, clientPort);
    udp.write(response, offset);
    udp.endPacket();
}

void DNSBlockingServer::forwardRequest(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort) {
    WiFiUDP forwardUdp;
    uint8_t response[512];

    // Forward to upstream DNS
    forwardUdp.begin(0);  // Random local port
    forwardUdp.beginPacket(upstreamDNS, 53);
    forwardUdp.write(request, requestLen);
    forwardUdp.endPacket();

    // Wait for response
    unsigned long start = millis();
    while (millis() - start < DNS_TIMEOUT_MS) {
        int packetSize = forwardUdp.parsePacket();
        if (packetSize > 0) {
            int len = forwardUdp.read(response, sizeof(response));
            forwardUdp.stop();

            // Forward response to original client
            udp.beginPacket(client, clientPort);
            udp.write(response, len);
            udp.endPacket();
            return;
        }
        delay(1);
    }

    forwardUdp.stop();
    DEBUG_PRINTLN("DNS: Upstream timeout");
}

bool DNSBlockingServer::addBlockedDomain(const String& domain) {
    String normalized = normalizeDomain(domain);

    // Check if already blocked
    for (const auto& d : blockedDomains) {
        if (d.equalsIgnoreCase(normalized)) {
            return false;
        }
    }

    if (blockedDomains.size() >= MAX_BLOCKED_DOMAINS) {
        DEBUG_PRINTLN("DNS: Max blocked domains reached");
        return false;
    }

    blockedDomains.push_back(normalized);
    storage.saveBlockedDomains(blockedDomains);
    DEBUG_PRINTF("DNS: Blocked domain added: %s\n", normalized.c_str());
    return true;
}

bool DNSBlockingServer::removeBlockedDomain(const String& domain) {
    String normalized = normalizeDomain(domain);

    for (auto it = blockedDomains.begin(); it != blockedDomains.end(); ++it) {
        if (it->equalsIgnoreCase(normalized)) {
            blockedDomains.erase(it);
            storage.saveBlockedDomains(blockedDomains);
            DEBUG_PRINTF("DNS: Blocked domain removed: %s\n", normalized.c_str());
            return true;
        }
    }

    return false;
}

bool DNSBlockingServer::isBlocked(const String& domain) {
    String queryDomain = normalizeDomain(domain);

    for (const auto& blocked : blockedDomains) {
        if (domainMatches(queryDomain, blocked)) {
            return true;
        }
    }

    return false;
}

bool DNSBlockingServer::domainMatches(const String& queryDomain, const String& blockedDomain) {
    // Exact match
    if (queryDomain.equalsIgnoreCase(blockedDomain)) {
        return true;
    }

    // Subdomain match: if blocked is "youtube.com", block "*.youtube.com"
    String suffix = "." + blockedDomain;
    if (queryDomain.endsWith(suffix)) {
        return true;
    }

    return false;
}

String DNSBlockingServer::normalizeDomain(const String& domain) {
    String normalized = domain;
    normalized.toLowerCase();
    normalized.trim();

    // Remove trailing dot
    while (normalized.endsWith(".")) {
        normalized = normalized.substring(0, normalized.length() - 1);
    }

    // Remove leading dot
    while (normalized.startsWith(".")) {
        normalized = normalized.substring(1);
    }

    return normalized;
}

std::vector<String> DNSBlockingServer::getBlockedDomains() {
    return blockedDomains;
}

void DNSBlockingServer::clearBlockedDomains() {
    blockedDomains.clear();
    storage.saveBlockedDomains(blockedDomains);
    DEBUG_PRINTLN("DNS: Cleared all blocked domains");
}

void DNSBlockingServer::setUpstreamDNS(const IPAddress& dns) {
    upstreamDNS = dns;
    storage.saveUpstreamDNS(dns);
    DEBUG_PRINTF("DNS: Upstream set to %s\n", dns.toString().c_str());
}

IPAddress DNSBlockingServer::getUpstreamDNS() {
    return upstreamDNS;
}

uint32_t DNSBlockingServer::getQueryCount() {
    return queryCount;
}

uint32_t DNSBlockingServer::getBlockedCount() {
    return blockedCount;
}
