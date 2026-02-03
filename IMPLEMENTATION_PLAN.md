# ESP32 Home Network Monitor & DNS Blocker - Implementation Plan

## Project Overview
Build an ESP32-based system that acts as a WiFi access point with:
- Per-device bandwidth tracking
- DNS-based domain blocking
- Web dashboard for monitoring and control

## Architecture Analysis

### Network Mode: WIFI_AP_STA (Dual Mode)
The requirement "Internet → Router → ESP32 → Home Devices" requires **dual mode**:
- **STA (Station)**: ESP32 connects to home router for internet access
- **AP (Access Point)**: Home devices connect to ESP32's WiFi network
- **NAT/Routing**: ESP32 bridges traffic between the two networks

### Traffic Flow
```
[Internet] ←→ [Home Router] ←→ [ESP32 STA interface]
                                      ↓
                              [ESP32 NAT/Routing]
                                      ↓
                              [ESP32 AP interface] ←→ [Home Devices]
```

### Core Components
1. **WiFi Manager** - Dual mode AP+STA setup
2. **NAT Engine** - IP forwarding with lwIP NAPT
3. **DHCP Server** - Auto-configured with AP mode (sets ESP32 as DNS)
4. **DNS Server** - Local resolver with domain blocking
5. **Bandwidth Tracker** - Packet-level byte counting per device
6. **Web Server** - Dashboard + REST API
7. **Storage Manager** - NVS persistence for settings

---

## File Structure
```
esp32-wifi/
├── esp32_network_monitor.ino    # Main sketch
├── config.h                      # Configuration constants
├── wifi_manager.h/.cpp           # WiFi AP+STA setup
├── nat_engine.h/.cpp             # NAT/routing with lwIP
├── dns_server.h/.cpp             # DNS server with blocking
├── bandwidth_tracker.h/.cpp      # Per-device traffic counting
├── device_manager.h/.cpp         # Device naming & storage
├── web_server.h/.cpp             # Dashboard & API
├── storage_manager.h/.cpp        # NVS wrapper
└── web_content.h                 # Embedded HTML/JS/CSS
```

---

## Configuration Decisions (User Confirmed)

1. **Router WiFi Credentials**: Both - defaults in config.h, changeable via web dashboard
2. **Bandwidth Reset**: Both global reset and per-device reset buttons in dashboard
3. **Upstream DNS**: Configurable via web UI, default to Cloudflare (1.1.1.1)

---

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | / | Dashboard HTML |
| GET | /api/devices | Device list with bandwidth stats |
| POST | /api/devices/{mac}/name | Set device name |
| POST | /api/devices/{mac}/reset | Reset single device stats |
| POST | /api/stats/reset | Reset all device stats |
| GET | /api/blockedDomains | List blocked domains |
| POST | /api/blockDomain | Add blocked domain |
| POST | /api/unblockDomain | Remove blocked domain |
| GET | /api/settings | Get current settings (DNS, WiFi status) |
| POST | /api/settings/wifi | Update router WiFi credentials |
| POST | /api/settings/dns | Update upstream DNS server |
| GET | /api/status | System status (uptime, memory, connections) |

---

## Libraries Required
- **WiFi.h** - ESP32 Arduino Core (built-in)
- **ESPAsyncWebServer** - Async web server
- **AsyncTCP** - Dependency for ESPAsyncWebServer
- **Preferences.h** - NVS storage (built-in)
- **WiFiUdp.h** - UDP for DNS server (built-in)
- **lwip/napt.h** - NAT support (ESP-IDF)

---

## Known Limitations
1. **VPN traffic**: Encrypted, cannot track per-destination
2. **DNS-over-HTTPS (DoH)**: Bypasses DNS blocking
3. **DNS-over-TLS (DoT)**: Bypasses DNS blocking
4. **Hardcoded DNS**: Apps using 8.8.8.8 directly bypass blocking
5. **HTTPS content**: Cannot inspect encrypted payload
6. **Throughput limit**: ESP32 ~15-20 Mbps practical limit as router
7. **Client limit**: Tested for 8 devices, may handle more
