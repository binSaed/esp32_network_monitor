#include "network_scanner.h"
#include "device_manager.h"
#include "config.h"
#include <ESPmDNS.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_ap_get_sta_list.h"

extern "C" {
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
}

NetworkScanner networkScanner;

// Timing constants
static const unsigned long ARP_SCAN_INTERVAL = 30000;   // Full scan every 30s
static const unsigned long ARP_BATCH_DELAY   = 200;     // 200ms between batches
static const unsigned long ARP_WAIT_DELAY    = 500;     // Wait for ARP replies
static const unsigned long MDNS_INTERVAL     = 45000;   // mDNS browse every 45s
static const int           ARP_BATCH_SIZE    = 16;

// mDNS service types to browse (one per cycle)
static const char* MDNS_SERVICES[] = {
    "http", "workstation", "airplay", "googlecast",
    "smb", "raop", "spotify-connect"
};
static const int MDNS_SERVICE_COUNT = sizeof(MDNS_SERVICES) / sizeof(MDNS_SERVICES[0]);

void NetworkScanner::begin() {
    DEBUG_PRINTLN("Scanner: Network scanner initialized");
}

void NetworkScanner::update() {
    if (WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();

    // ARP scan state machine
    switch (_state) {
        case IDLE:
            if (now - _lastScan >= ARP_SCAN_INTERVAL) {
                _state = SCANNING;
                _scanIP = 1;
                _lastBatch = 0;
            }
            break;

        case SCANNING:
            if (now - _lastBatch >= ARP_BATCH_DELAY) {
                _lastBatch = now;
                sendArpBatch();
                if (_scanIP > 254) {
                    _state = WAITING;
                    _waitStart = now;
                }
            }
            break;

        case WAITING:
            if (now - _waitStart >= ARP_WAIT_DELAY) {
                readArpTable();
                readApClients();
                _state = IDLE;
                _lastScan = now;
            }
            break;
    }

    // Periodic mDNS browsing
    if (now - _lastMdns >= MDNS_INTERVAL) {
        _lastMdns = now;
        mdnsBrowse();
    }
}

void NetworkScanner::sendArpBatch() {
    IPAddress staIP = WiFi.localIP();
    IPAddress subnet = WiFi.subnetMask();

    uint32_t ip_raw   = (uint32_t)staIP;
    uint32_t mask_raw = (uint32_t)subnet;
    uint32_t net_raw  = ip_raw & mask_raw;

    // Only scan /24 or smaller subnets
    uint32_t host_bits = ntohl(~mask_raw);
    if (host_bits > 254) {
        _scanIP = 255;
        return;
    }

    LOCK_TCPIP_CORE();

    // Find STA network interface
    struct netif *sta_nif = NULL;
    for (struct netif *nif = netif_list; nif != NULL; nif = nif->next) {
        if (netif_is_up(nif) && ip4_addr_get_u32(netif_ip4_addr(nif)) == ip_raw) {
            sta_nif = nif;
            break;
        }
    }

    if (!sta_nif) {
        UNLOCK_TCPIP_CORE();
        _scanIP = 255;
        return;
    }

    // Send ARP requests for this batch
    uint32_t net_host = ntohl(net_raw);
    int sent = 0;

    while (_scanIP <= 254 && sent < ARP_BATCH_SIZE) {
        ip4_addr_t target;
        target.addr = htonl(net_host + _scanIP);

        // Don't ARP ourselves
        if (target.addr != ip_raw) {
            etharp_request(sta_nif, &target);
        }

        _scanIP++;
        sent++;
    }

    UNLOCK_TCPIP_CORE();
}

void NetworkScanner::readArpTable() {
    LOCK_TCPIP_CORE();

    for (size_t i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t *ipaddr = NULL;
        struct netif *netif = NULL;
        struct eth_addr *ethaddr = NULL;

        if (etharp_get_entry(i, &ipaddr, &netif, &ethaddr) == 0) {
            if (ethaddr == NULL || ipaddr == NULL) continue;
            uint8_t mac[6];
            memcpy(mac, ethaddr->addr, 6);
            // Skip entries with no valid MAC (all zeros = pending ARP)
            if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 &&
                mac[3] == 0 && mac[4] == 0 && mac[5] == 0) continue;
            IPAddress ip(
                ip4_addr1_16(ipaddr), ip4_addr2_16(ipaddr),
                ip4_addr3_16(ipaddr), ip4_addr4_16(ipaddr)
            );
            deviceManager.updateDevice(mac, ip);
        }
    }

    UNLOCK_TCPIP_CORE();
}

void NetworkScanner::readApClients() {
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) != ESP_OK) return;

    wifi_sta_mac_ip_list_t mac_ip_list;
    if (esp_wifi_ap_get_sta_list_with_ip(&sta_list, &mac_ip_list) != ESP_OK) return;

    for (int i = 0; i < mac_ip_list.num; i++) {
        esp_netif_pair_mac_ip_t &info = mac_ip_list.sta[i];
        IPAddress ip(
            esp_ip4_addr_get_byte(&info.ip, 0),
            esp_ip4_addr_get_byte(&info.ip, 1),
            esp_ip4_addr_get_byte(&info.ip, 2),
            esp_ip4_addr_get_byte(&info.ip, 3)
        );
        deviceManager.updateDevice(info.mac, ip);
    }
}

void NetworkScanner::mdnsBrowse() {
    const char* svc = MDNS_SERVICES[_mdnsIdx];
    _mdnsIdx = (_mdnsIdx + 1) % MDNS_SERVICE_COUNT;

    int n = MDNS.queryService(svc, "tcp");
    if (n <= 0) return;

    DEBUG_PRINTF("Scanner: Found %d %s._tcp services\n", n, svc);

    for (int i = 0; i < n; i++) {
        String hostname = MDNS.hostname(i);
        IPAddress ip = MDNS.address(i);

        if (hostname.length() == 0 || ip == IPAddress(0, 0, 0, 0)) continue;

        // Find device by IP and update name
        std::vector<DeviceInfo> devs = deviceManager.getAllDevices();
        for (auto& d : devs) {
            if (d.ip == ip && !d.hasCustomName) {
                deviceManager.updateDeviceHostname(d.mac, hostname);
                DEBUG_PRINTF("Scanner: mDNS name for %s -> %s\n",
                             d.getMACString().c_str(), hostname.c_str());
                break;
            }
        }
    }
}
