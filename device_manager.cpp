#include "device_manager.h"
#include "storage_manager.h"

DeviceManager deviceManager;

DeviceManager::DeviceManager() {}

void DeviceManager::updateDevice(const uint8_t* mac, const IPAddress& ip) {
    DeviceInfo* device = findOrCreateDevice(mac);
    if (device) {
        device->ip = ip;
    }
}

void DeviceManager::updateDeviceHostname(const uint8_t* mac, const String& hostname) {
    DeviceInfo* device = findOrCreateDevice(mac);
    if (device && !device->hasCustomName) {
        strncpy(device->autoName, hostname.c_str(), MAX_DEVICE_NAME - 1);
        device->autoName[MAX_DEVICE_NAME - 1] = '\0';
        DEBUG_PRINTF("Device: Auto-name for %s: %s\n",
                     device->getMACString().c_str(), hostname.c_str());
    }
}

DeviceInfo* DeviceManager::findOrCreateDevice(const uint8_t* mac) {
    // Find existing
    for (auto& device : devices) {
        if (macEqual(device.mac, mac)) {
            return &device;
        }
    }

    // Create new
    if (devices.size() >= MAX_DEVICES) {
        return nullptr;
    }

    DeviceInfo newDevice;
    memcpy(newDevice.mac, mac, 6);
    newDevice.ip = IPAddress(0, 0, 0, 0);
    newDevice.customName[0] = '\0';
    newDevice.autoName[0] = '\0';
    newDevice.hasCustomName = false;

    // Load custom name from storage
    String savedName = storage.loadDeviceName(mac);
    if (savedName.length() > 0) {
        strncpy(newDevice.customName, savedName.c_str(), MAX_DEVICE_NAME - 1);
        newDevice.customName[MAX_DEVICE_NAME - 1] = '\0';
        newDevice.hasCustomName = true;
    }

    devices.push_back(newDevice);
    DEBUG_PRINTF("Device: New device tracked %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return &devices.back();
}

DeviceInfo* DeviceManager::getDevice(const uint8_t* mac) {
    for (auto& device : devices) {
        if (macEqual(device.mac, mac)) {
            return &device;
        }
    }
    return nullptr;
}

std::vector<DeviceInfo> DeviceManager::getAllDevices() {
    return devices;
}

bool DeviceManager::setDeviceName(const uint8_t* mac, const String& name) {
    DeviceInfo* device = findOrCreateDevice(mac);
    if (!device) {
        return false;
    }

    strncpy(device->customName, name.c_str(), MAX_DEVICE_NAME - 1);
    device->customName[MAX_DEVICE_NAME - 1] = '\0';
    device->hasCustomName = true;

    storage.saveDeviceName(mac, name);
    DEBUG_PRINTF("Device: Set name for %s: %s\n",
                 device->getMACString().c_str(), name.c_str());

    return true;
}

String DeviceManager::getDeviceName(const uint8_t* mac) {
    DeviceInfo* device = getDevice(mac);
    if (device) {
        return device->getDisplayName();
    }
    return "Unknown Device";
}

void DeviceManager::clearDeviceName(const uint8_t* mac) {
    DeviceInfo* device = getDevice(mac);
    if (device) {
        device->customName[0] = '\0';
        device->hasCustomName = false;
        storage.deleteDeviceName(mac);
        DEBUG_PRINTF("Device: Cleared name for %s\n", device->getMACString().c_str());
    }
}

bool DeviceManager::parseMAC(const String& macStr, uint8_t* mac) {
    // Parse MAC from string like "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF"
    String clean = macStr;
    clean.replace(":", "");
    clean.replace("-", "");
    clean.toUpperCase();

    if (clean.length() != 12) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        String byteStr = clean.substring(i * 2, i * 2 + 2);
        mac[i] = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
    }

    return true;
}

String DeviceManager::macToString(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

bool DeviceManager::macEqual(const uint8_t* mac1, const uint8_t* mac2) {
    return memcmp(mac1, mac2, 6) == 0;
}
