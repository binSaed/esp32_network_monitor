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
    cacheHits(0),
    running(false),
    _requestQueue(NULL),
    _responseQueue(NULL),
    _forwardTask(NULL) {
    memset(_cache, 0, sizeof(_cache));
}

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

    // Create forwarding queues
    _requestQueue = xQueueCreate(DNS_FORWARD_QUEUE_SIZE, sizeof(DNSForwardRequest));
    _responseQueue = xQueueCreate(DNS_FORWARD_QUEUE_SIZE, sizeof(DNSForwardResponse));

    if (!_requestQueue || !_responseQueue) {
        DEBUG_PRINTLN("DNS: Failed to create forwarding queues");
        udp.stop();
        return false;
    }

    // Create forwarding task on core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        forwardTaskFunc,
        "dns_fwd",
        DNS_TASK_STACK_SIZE,
        this,
        DNS_TASK_PRIORITY,
        &_forwardTask,
        0  // Core 0 (keeps main loop on core 1 free)
    );

    if (result != pdPASS) {
        DEBUG_PRINTLN("DNS: Failed to create forwarding task");
        udp.stop();
        vQueueDelete(_requestQueue);
        vQueueDelete(_responseQueue);
        return false;
    }

    running = true;
    DEBUG_PRINTF("DNS: Server started on port %d, upstream: %s\n",
                 DNS_PORT, upstreamDNS.toString().c_str());
    DEBUG_PRINTF("DNS: %d blocked domains loaded\n", blockedDomains.size());
    DEBUG_PRINTF("DNS: Async forwarding enabled (cache: %d entries, TTL: %ds)\n",
                 DNS_CACHE_SIZE, DNS_CACHE_TTL_MS / 1000);

    return true;
}

void DNSBlockingServer::stop() {
    running = false;

    if (_forwardTask) {
        vTaskDelete(_forwardTask);
        _forwardTask = NULL;
    }
    if (_requestQueue) {
        vQueueDelete(_requestQueue);
        _requestQueue = NULL;
    }
    if (_responseQueue) {
        vQueueDelete(_responseQueue);
        _responseQueue = NULL;
    }

    udp.stop();
    DEBUG_PRINTLN("DNS: Server stopped");
}

void DNSBlockingServer::processRequests() {
    if (!running) return;

    // First, send any completed forward responses back to clients
    drainResponseQueue();

    // Then check for new incoming DNS queries
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        handleDNSRequest();
    }
}

void DNSBlockingServer::drainResponseQueue() {
    DNSForwardResponse resp;
    while (xQueueReceive(_responseQueue, &resp, 0) == pdTRUE) {
        // Cache the response for future lookups
        cacheStore(resp.domain, resp.packet, resp.packetLen);

        // Send response to original client via port 53
        IPAddress clientIP(resp.clientIP);
        udp.beginPacket(clientIP, resp.clientPort);
        udp.write(resp.packet, resp.packetLen);
        udp.endPacket();
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
        return;
    }

    // Check cache before forwarding upstream
    DNSCacheEntry* cached = cacheLookup(domain.c_str());
    if (cached) {
        DEBUG_PRINTF("DNS: Cache hit for '%s'\n", domain.c_str());
        cacheHits++;

        // Send cached response with updated transaction ID
        uint8_t response[512];
        memcpy(response, cached->response, cached->responseLen);
        response[0] = buffer[0];  // Copy transaction ID from query
        response[1] = buffer[1];

        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(response, cached->responseLen);
        udp.endPacket();
        return;
    }

    // Queue for async forwarding (non-blocking)
    queueForwardRequest(buffer, len, udp.remoteIP(), udp.remotePort(), domain);
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

void DNSBlockingServer::queueForwardRequest(const uint8_t* request, int requestLen, IPAddress client, uint16_t clientPort, const String& domain) {
    DNSForwardRequest req;
    memcpy(req.packet, request, requestLen);
    req.packetLen = requestLen;
    req.clientIP = (uint32_t)client;
    req.clientPort = clientPort;
    strncpy(req.domain, domain.c_str(), MAX_DOMAIN_LENGTH);
    req.domain[MAX_DOMAIN_LENGTH] = '\0';

    if (xQueueSend(_requestQueue, &req, 0) != pdTRUE) {
        DEBUG_PRINTLN("DNS: Forward queue full, dropping request");
    }
}

// Runs on core 0 as a separate FreeRTOS task
void DNSBlockingServer::forwardTaskFunc(void* param) {
    DNSBlockingServer* self = (DNSBlockingServer*)param;
    DNSForwardRequest req;
    WiFiUDP forwardUdp;
    forwardUdp.begin(0);  // Bind once, reuse for all queries

    for (;;) {
        // Block until a forward request is available
        if (xQueueReceive(self->_requestQueue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // Flush any stale responses from previous timed-out queries
        uint8_t discard[512];
        while (forwardUdp.parsePacket() > 0) {
            forwardUdp.read(discard, sizeof(discard));
        }

        // Forward to upstream DNS
        forwardUdp.beginPacket(self->upstreamDNS, 53);
        forwardUdp.write(req.packet, req.packetLen);
        forwardUdp.endPacket();

        // Extract transaction ID for response matching
        uint16_t queryId = (req.packet[0] << 8) | req.packet[1];

        // Wait for response with reduced timeout
        bool gotResponse = false;
        unsigned long start = millis();
        while (millis() - start < DNS_TIMEOUT_MS) {
            int packetSize = forwardUdp.parsePacket();
            if (packetSize > 0) {
                uint8_t response[512];
                int len = forwardUdp.read(response, sizeof(response));

                // Verify transaction ID matches our query
                if (len >= 2) {
                    uint16_t respId = (response[0] << 8) | response[1];
                    if (respId == queryId) {
                        // Queue response for main loop to send via port 53
                        DNSForwardResponse resp;
                        memcpy(resp.packet, response, len);
                        resp.packetLen = len;
                        resp.clientIP = req.clientIP;
                        resp.clientPort = req.clientPort;
                        strncpy(resp.domain, req.domain, MAX_DOMAIN_LENGTH);
                        resp.domain[MAX_DOMAIN_LENGTH] = '\0';

                        xQueueSend(self->_responseQueue, &resp, pdMS_TO_TICKS(100));
                        gotResponse = true;
                        break;
                    }
                    // Mismatched ID - stale response, discard and keep waiting
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        if (!gotResponse) {
            DEBUG_PRINTF("DNS: Upstream timeout for '%s'\n", req.domain);
        }
    }
}

// Cache lookup - returns entry if found and not expired, NULL otherwise
DNSCacheEntry* DNSBlockingServer::cacheLookup(const char* domain) {
    uint32_t now = millis();
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (_cache[i].valid &&
            strcasecmp(_cache[i].domain, domain) == 0 &&
            (now - _cache[i].timestamp) < DNS_CACHE_TTL_MS) {
            return &_cache[i];
        }
    }
    return nullptr;
}

// Cache store - replaces existing entry for same domain, or oldest entry
void DNSBlockingServer::cacheStore(const char* domain, const uint8_t* response, int responseLen) {
    int slot = -1;
    uint32_t oldestTime = UINT32_MAX;
    int oldestSlot = 0;

    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        // Prefer empty slot
        if (!_cache[i].valid) {
            slot = i;
            break;
        }
        // Update existing entry for same domain
        if (strcasecmp(_cache[i].domain, domain) == 0) {
            slot = i;
            break;
        }
        // Track oldest for eviction
        if (_cache[i].timestamp < oldestTime) {
            oldestTime = _cache[i].timestamp;
            oldestSlot = i;
        }
    }

    if (slot < 0) {
        slot = oldestSlot;  // Evict oldest
    }

    strncpy(_cache[slot].domain, domain, MAX_DOMAIN_LENGTH);
    _cache[slot].domain[MAX_DOMAIN_LENGTH] = '\0';
    memcpy(_cache[slot].response, response, responseLen);
    _cache[slot].responseLen = responseLen;
    _cache[slot].timestamp = millis();
    _cache[slot].valid = true;
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

    // Take mutex briefly - blockedDomains can be modified by web handlers on the async task
    bool result = false;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    for (const auto& blocked : blockedDomains) {
        if (domainMatches(queryDomain, blocked)) {
            result = true;
            break;
        }
    }
    xSemaphoreGive(dataMutex);

    return result;
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

uint32_t DNSBlockingServer::getCacheHits() {
    return cacheHits;
}
