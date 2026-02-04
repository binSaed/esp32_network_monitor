# ESP32 Home Network Monitor & DNS Blocker

A complete home network monitoring and control system built on ESP32 that tracks per-device bandwidth usage and provides DNS-based domain blocking.

![ESP32](https://img.shields.io/badge/ESP32-supported-green)
![Arduino](https://img.shields.io/badge/Arduino-IDE-blue)
![License](https://img.shields.io/badge/license-MIT-brightgreen)

## Features

- **Per-Device Bandwidth Tracking** - Monitor upload/download usage for each connected device
- **DNS-Based Domain Blocking** - Block unwanted domains (ads, social media, etc.)
- **DNS Response Cache** - Cached lookups for faster repeat visits (16 entries, 60s TTL)
- **Web Dashboard** - Mobile-friendly interface accessible from any browser
- **Device Naming** - Assign friendly names to devices (e.g., "iPhone", "Laptop")
- **Auto Device Discovery** - Automatically detects device names via DHCP hostname
- **mDNS Support** - Access via `http://networkmonitor.local` from any device
- **Persistent Storage** - All settings and stats survive reboots
- **Real-time Updates** - Dashboard auto-refreshes every 5 seconds
- **No Cloud Required** - Everything runs locally on your network

---

## Table of Contents

- [Network Architecture](#network-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Technical Details](#technical-details)
- [Troubleshooting](#troubleshooting)
- [Known Limitations](#known-limitations)
- [License](#license)

---

## Network Architecture

```
┌──────────┐      ┌──────────────┐      ┌─────────────────┐      ┌──────────────┐
│ Internet │ ←──→ │ Home Router  │ ←──→ │  ESP32 (STA)    │ ←──→ │ Your Devices │
└──────────┘      └──────────────┘      │  ESP32 (AP)     │      └──────────────┘
                                        │  DNS Server     │
                                        │  Web Dashboard  │
                                        └─────────────────┘
```

The ESP32 operates in **dual-mode**:
- **Station (STA)**: Connects to your home router for internet access
- **Access Point (AP)**: Creates a WiFi network for your devices

All traffic flows through the ESP32, enabling bandwidth monitoring and DNS filtering.

---

## Hardware Requirements

| Component | Requirement |
|-----------|-------------|
| Board | ESP32-WROOM, ESP32-DevKit, or similar |
| WiFi | 2.4 GHz (ESP32 does not support 5 GHz) |
| USB | Cable for programming |
| Power | 5V via USB or external supply |

**Note**: No additional components needed - just the ESP32 board!

---

## Software Requirements

### Arduino IDE Setup

1. **Install Arduino IDE 2.x** from [arduino.cc](https://www.arduino.cc/en/software)

2. **Add ESP32 Board Support**:
   - Go to `File` → `Preferences`
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to `Tools` → `Board` → `Boards Manager`
   - Search "esp32" and install **"esp32 by Espressif Systems"** (v2.0.0+)

3. **Install Required Libraries** (`Sketch` → `Include Library` → `Manage Libraries`):

   | Library | Author | Version |
   |---------|--------|---------|
   | ESPAsyncWebServer | me-no-dev | Latest |
   | AsyncTCP | me-no-dev | Latest |
   | ArduinoJson | Benoit Blanchon | 7.x |

### Manual Library Installation

If libraries aren't in Library Manager, download from GitHub:
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) → Download ZIP
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) → Download ZIP

Install via `Sketch` → `Include Library` → `Add .ZIP Library`

---

## Installation

1. **Clone or Download** this repository

2. **Open** `esp32_network_monitor.ino` in Arduino IDE

3. **Configure** your router credentials in `config.h`:
   ```cpp
   #define DEFAULT_STA_SSID     "YourRouterSSID"
   #define DEFAULT_STA_PASSWORD "YourRouterPassword"
   ```

4. **Select Board**: `Tools` → `Board` → `ESP32 Arduino` → `ESP32 Dev Module`

5. **Select Port**: `Tools` → `Port` → (your ESP32 COM port)

6. **Upload**: Click Upload button or `Ctrl+U`

7. **Monitor**: Open Serial Monitor (`115200 baud`) to see status

---

## Configuration

### config.h Options

```cpp
// ─── Router Connection (STA) ───
#define DEFAULT_STA_SSID     "YourRouterSSID"      // Your home WiFi name
#define DEFAULT_STA_PASSWORD "YourRouterPassword"  // Your home WiFi password

// ─── Access Point Settings ───
#define AP_SSID              "ESP32_Monitor"       // Network name for devices
#define AP_PASSWORD          "monitor123"          // Password (min 8 characters)
#define AP_CHANNEL           6                     // WiFi channel (1-13)
#define AP_MAX_CONNECTIONS   8                     // Max connected devices

// ─── Network ───
#define AP_IP                IPAddress(192, 168, 4, 1)   // Dashboard address
#define DEFAULT_UPSTREAM_DNS IPAddress(1, 1, 1, 1)       // Cloudflare DNS

// ─── mDNS ───
#define MDNS_HOSTNAME        "networkmonitor"            // Access via http://networkmonitor.local

// ─── Debug ───
#define DEBUG_SERIAL         true                  // Enable serial output
#define SERIAL_BAUD          115200                // Serial baud rate
```

---

## Usage

### Initial Setup

1. **Power on** the ESP32
2. **Open Serial Monitor** (115200 baud) to see boot status
3. **Connect** your phone/laptop to ESP32's WiFi:
   - **SSID**: `ESP32_Monitor`
   - **Password**: `monitor123`
4. **Open browser** and go to: `http://192.168.4.1`

### Access from Main WiFi

You can also access the dashboard from your main WiFi network:
- **Via mDNS**: `http://networkmonitor.local`
- **Via IP**: Use the IP shown in Serial Monitor (e.g., `http://192.168.1.105`)

### Dashboard Features

#### Device Monitoring
- View all connected devices with MAC addresses
- See real-time upload/download/total bandwidth
- Click device name to rename it
- Reset stats per-device or globally

#### DNS Blocking
- Enter domain to block (e.g., `youtube.com`)
- Subdomains are automatically blocked (`*.youtube.com`)
- Unblock domains with one click

#### Settings
- Change upstream DNS server
- Update router WiFi credentials
- View system status (uptime, memory, DNS stats)

### Screenshots

```
┌─────────────────────────────────────────────────────────────┐
│  ESP32 Network Monitor                                      │
├─────────────────────────────────────────────────────────────┤
│  Router: Connected (192.168.1.105)    Devices: 3    Up: 2h │
├─────────────────────────────────────────────────────────────┤
│  DEVICES                                    [Reset All]     │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ iPhone         │ 245 MB │ 12 MB  │ 257 MB │ [Reset] │   │
│  │ AA:BB:CC:DD:EE │        │        │        │         │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │ Laptop         │ 1.2 GB │ 89 MB  │ 1.3 GB │ [Reset] │   │
│  │ 11:22:33:44:55 │        │        │        │         │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  DNS BLOCKING                              SETTINGS         │
│  [youtube.com        ] [Block]    DNS: [1.1.1.1   ] [Save] │
│  ┌─────────────────────────┐      WiFi: [SSID     ]        │
│  │ facebook.com  [Unblock] │            [Password ] [Save] │
│  │ tiktok.com    [Unblock] │                               │
│  └─────────────────────────┘      Queries: 1,234           │
│                                   Blocked: 89              │
└─────────────────────────────────────────────────────────────┘
```

---

## API Reference

All endpoints return JSON.

### Devices

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/devices` | List all devices with stats |
| POST | `/api/devices/{mac}/name` | Set device name |
| POST | `/api/devices/{mac}/reset` | Reset device stats |
| POST | `/api/stats/reset` | Reset all device stats |

**Example Response** (`GET /api/devices`):
```json
[
  {
    "mac": "AA:BB:CC:DD:EE:FF",
    "name": "iPhone",
    "upload": 12582912,
    "download": 256901120,
    "total": 269484032,
    "active": true
  }
]
```

### DNS Blocking

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/blockedDomains` | List blocked domains |
| POST | `/api/blockDomain` | Block a domain |
| POST | `/api/unblockDomain` | Unblock a domain |

**Example Request** (`POST /api/blockDomain`):
```json
{ "domain": "youtube.com" }
```

### Settings

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | System status |
| GET | `/api/settings` | Current settings |
| POST | `/api/settings/dns` | Set upstream DNS |
| POST | `/api/settings/wifi` | Set router WiFi credentials |

**Example Response** (`GET /api/status`):
```json
{
  "connected": true,
  "staIP": "192.168.1.105",
  "apIP": "192.168.4.1",
  "clients": 3,
  "uptime": 7200,
  "freeHeap": 180000,
  "dnsQueries": 1234,
  "dnsBlocked": 89
}
```

---

## Technical Details

### Bandwidth Tracking

The system uses **WiFi promiscuous mode** to capture all 802.11 data frames:

1. ESP32 enables promiscuous mode on WiFi interface
2. All data frames are intercepted via callback
3. 802.11 frame headers are parsed to extract:
   - Source/Destination MAC addresses
   - Frame direction (To-DS/From-DS flags)
   - Payload length
4. Events are pushed to a lock-free ring buffer (512 slots)
5. Main loop drains the buffer and attributes traffic by MAC address
6. Direction determines upload vs download

**Accuracy**: ±5% (excludes WiFi protocol overhead)

### DNS Filtering

DNS queries are handled without blocking the main loop:

```
Client Request: api.youtube.com
        │
        ▼
┌──────────────────────────┐
│ ESP32 DNS Server         │
│                          │
│ 1. Check blocklist:      │
│    youtube.com? ─── Yes ─┼──→ Return 0.0.0.0
│                          │
│ 2. Check cache:          │
│    Cached? ──────── Yes ─┼──→ Return cached IP (instant)
│                          │
│ 3. Queue for forwarding: │
│    (non-blocking)        │
│         │                │
└─────────┼────────────────┘
          ▼
┌──────────────────────────┐
│ Forwarding Task (Core 0) │
│                          │
│ Forward to 1.1.1.1       │
│ Wait for response (1s)   │
│ Queue response + cache   │
└──────────────────────────┘
```

DNS forwarding runs on a dedicated FreeRTOS task on core 0, so upstream
latency never stalls the main loop. Responses are cached (16 entries,
60-second TTL) so repeat lookups are served instantly.

### File Structure

```
esp32_network_monitor/
├── esp32_network_monitor.ino  # Main sketch entry point
├── config.h                    # Configuration constants
├── storage_manager.h/cpp       # NVS persistence layer
├── wifi_manager.h/cpp          # WiFi AP+STA management
├── nat_engine.h/cpp            # NAT routing & packet capture
├── dns_server.h/cpp            # DNS server with async forwarding & cache
├── bandwidth_tracker.h/cpp     # Per-device traffic counting (512-slot ring buffer)
├── device_manager.h/cpp        # Device naming & tracking
├── network_scanner.h/cpp       # ARP & mDNS device discovery
├── oui_lookup.h/cpp            # MAC vendor identification (OUI database)
├── web_server.h/cpp            # HTTP server & REST API
├── web_content.h               # Embedded HTML/JS/CSS
└── README.md                   # This file
```

### Memory Usage

| Component | RAM Usage |
|-----------|-----------|
| WiFi Stack | ~50 KB |
| Web Server | ~10 KB |
| Device Tracking (16 devices) | ~1 KB |
| DNS Forwarding Queues + Cache | ~19 KB |
| DNS Forwarding Task Stack | ~8 KB |
| Bandwidth Ring Buffer (512 slots) | ~6 KB |
| Blocked Domains (100) | ~4 KB |
| **Total** | **~98 KB** |

ESP32 has ~320 KB available RAM - plenty of headroom.

---

## Troubleshooting

### No Internet on Connected Devices

1. Check Serial Monitor for "NAT: NAPT enabled" message
2. Verify router WiFi credentials in `config.h`
3. Ensure ESP32 shows "Router: Connected" on boot
4. Check if router's 2.4 GHz band is enabled

### Devices Not Appearing in Dashboard

1. Wait 10-30 seconds after device connects
2. Generate some traffic (open a website)
3. Ensure device got IP in 192.168.4.x range
4. Check Serial Monitor for "Client connected" messages

### DNS Blocking Not Working

1. Verify device is using ESP32 as DNS server
2. Clear browser DNS cache (`chrome://net-internals/#dns`)
3. Try blocking from a fresh incognito window
4. Some apps use hardcoded DNS (can't be blocked)

### Dashboard Not Loading

1. Verify you're connected to ESP32 AP, not home router
2. Try `http://192.168.4.1` directly (not HTTPS)
3. Disable VPN if active
4. Check Serial Monitor for web server errors

### mDNS (networkmonitor.local) Not Working

1. **Windows/macOS/iOS**: Should work natively
2. **Linux**: Install avahi-daemon (`sudo apt install avahi-daemon`)
3. **Android**: Limited support - use IP address instead
4. Try using the IP address shown in Serial Monitor
5. Ensure you're on the same network as the ESP32

### Compilation Errors

| Error | Solution |
|-------|----------|
| `ESPAsyncWebServer.h not found` | Install ESPAsyncWebServer library |
| `AsyncTCP.h not found` | Install AsyncTCP library |
| `ArduinoJson.h not found` | Install ArduinoJson library |
| `lwip/lwip_napt.h not found` | Update ESP32 Arduino Core to 2.0.0+ |

---

## Known Limitations

| Limitation | Reason |
|------------|--------|
| **VPN traffic** | Encrypted tunnel hides actual destinations |
| **DNS-over-HTTPS (DoH)** | Browsers can bypass local DNS |
| **DNS-over-TLS (DoT)** | Encrypted DNS bypasses filtering |
| **Hardcoded DNS** | Apps using 8.8.8.8 directly bypass blocking |
| **HTTPS inspection** | Cannot see encrypted content |
| **5 GHz WiFi** | ESP32 hardware limitation (except ESP32-C6) |
| **Throughput** | ~15-20 Mbps practical limit as router |
| **Max devices** | Designed for 8, may support more |

### Workarounds

- **DoH/DoT**: Disable in browser settings
- **Hardcoded DNS**: Block at router level or use firewall rules
- **Higher throughput**: Use ESP32 only for monitoring, not as primary router

---

## License

MIT License - Feel free to use, modify, and distribute.

---

## Contributing

Issues and pull requests are welcome!

---

## Acknowledgments

- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) by me-no-dev
- [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon
- ESP32 Arduino Core by Espressif Systems
