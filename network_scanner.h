#ifndef NETWORK_SCANNER_H
#define NETWORK_SCANNER_H

#include <Arduino.h>
#include <WiFi.h>

class NetworkScanner {
public:
    void begin();
    void update();  // Call in loop()

private:
    enum State { IDLE, SCANNING, WAITING };

    State _state = IDLE;
    unsigned long _lastScan = 0;
    unsigned long _lastBatch = 0;
    unsigned long _waitStart = 0;
    unsigned long _lastMdns = 0;
    uint16_t _scanIP = 1;
    uint8_t _mdnsIdx = 0;

    void sendArpBatch();
    void readArpTable();
    void readApClients();
    void mdnsBrowse();
};

extern NetworkScanner networkScanner;

#endif // NETWORK_SCANNER_H
