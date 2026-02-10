#include "BleManager.h"

#define SERVICE_UUID           "73616968-7334-2722-6616-737977756e67"
#define RX_CHARACTERISTIC_UUID "73616968-7334-2722-6616-737977756e68"
#define TX_CHARACTERISTIC_UUID "73616968-7334-2722-6616-737977756e69"

#define BATTERY_SERVICE_UUID   "180F"
#define BATTERY_LEVEL_UUID     "2A19"

BleManager& BleManager::getInstance() {
    static BleManager instance;
    return instance;
}

BleManager::BleManager() : 
    _pServer(nullptr), 
    _pTxCharacteristic(nullptr), 
    _pRxCharacteristic(nullptr), 
    _pBatteryCharacteristic(nullptr), 
    _deviceConnected(false), 
    _sequence(0), 
    _batteryLevel(100), 
    _serviceUUID(SERVICE_UUID),
    _rxUUID(RX_CHARACTERISTIC_UUID),
    _txUUID(TX_CHARACTERISTIC_UUID),
    _connectionCallback(nullptr), 
    _packetCallback(nullptr) {}

void BleManager::setCustomUUIDs(const char* serviceUUID, const char* rxUUID, const char* txUUID) {
    if (serviceUUID) _serviceUUID = serviceUUID;
    if (rxUUID) _rxUUID = rxUUID;
    if (txUUID) _txUUID = txUUID;
}

void BleManager::setServiceUUID(const char* uuid) {
    if (uuid) _serviceUUID = uuid;
}

void BleManager::setRxUUID(const char* uuid) {
    if (uuid) _rxUUID = uuid;
}

void BleManager::setTxUUID(const char* uuid) {
    if (uuid) _txUUID = uuid;
}

void BleManager::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks());

    // Custom Control Service
    NimBLEService* pService = _pServer->createService(_serviceUUID);

    _pTxCharacteristic = pService->createCharacteristic(
        _txUUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    _pRxCharacteristic = pService->createCharacteristic(
        _rxUUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    pService->start();

    // Battery Service
    NimBLEService* pBatteryService = _pServer->createService(BATTERY_SERVICE_UUID);
    _pBatteryCharacteristic = pBatteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pBatteryService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(_serviceUUID);
    
    // Set up Scan Response Data
    NimBLEAdvertisementData scanResponseData;
    
    // Add Current Battery Level to Scan Response
    std::string batteryData((char*)&_batteryLevel, 1);
    scanResponseData.setServiceData(NimBLEUUID((uint16_t)0x180F), batteryData);
    
    pAdvertising->setScanResponseData(scanResponseData);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
    
    Serial.printf("[BLE] Advertising started as %s (Initial Battery: %d%%)\n", deviceName, _batteryLevel);
}

void BleManager::update() {
    // Optional maintenance
}

void BleManager::setConnectionCallback(ConnectionCallback cb) {
    _connectionCallback = cb;
}

void BleManager::setPacketCallback(PacketCallback cb) {
    _packetCallback = cb;
}

bool BleManager::isConnected() {
    return _deviceConnected;
}

void BleManager::sendButtonEvent(uint8_t targetId, ButtonAction action) {
    if (!_deviceConnected) return;

    BcbpPacketV1 packet;
    packet.version = BCBP_V1;
    packet.command = CMD_BUTTON;
    packet.targetId = targetId;
    packet.action = (uint8_t)action;
    packet.sequence = _sequence++;
    packet.crc8 = BcbpProtocol::calculateCRC8((uint8_t*)&packet, 5);

    _pTxCharacteristic->setValue((uint8_t*)&packet, sizeof(packet));
    _pTxCharacteristic->notify();
    
    Serial.printf("[BLE] Sent Button Packet: ID=%d, ACT=%d, SEQ=%d\n", targetId, (uint8_t)action, packet.sequence);
}

void BleManager::sendDigitalReport(uint8_t channel, bool state) {
    sendDigitalReport(channel, (uint8_t)(state ? 1 : 0));
}

void BleManager::sendDigitalReport(uint8_t channel, uint8_t state) {
    if (!_deviceConnected) return;

    BcbpPacketV1 packet;
    packet.version = BCBP_V1;
    packet.command = CMD_DIGITAL;
    packet.targetId = channel;
    packet.action = state;
    packet.sequence = _sequence++;
    packet.crc8 = BcbpProtocol::calculateCRC8((uint8_t*)&packet, 5);

    _pTxCharacteristic->setValue((uint8_t*)&packet, sizeof(packet));
    _pTxCharacteristic->notify();
    
    Serial.printf("[BLE] Sent DI Packet: CH=%d, STATE=%d, SEQ=%d\n", channel, state, packet.sequence);
}

void BleManager::sendAnalogReport(uint8_t channel, uint16_t value) {
    if (!_deviceConnected) return;

    BcbpPacketV1 packet;
    packet.version = BCBP_V1;
    packet.command = CMD_ANALOG;
    packet.targetId = channel;
    packet.action = (uint8_t)(value >> 8);   // High byte
    packet.sequence = (uint8_t)(value & 0xFF); // Low byte (reusing sequence field)
    packet.crc8 = BcbpProtocol::calculateCRC8((uint8_t*)&packet, 5);

    _pTxCharacteristic->setValue((uint8_t*)&packet, sizeof(packet));
    _pTxCharacteristic->notify();
    
    Serial.printf("[BLE] Sent AI Packet: CH=%d, VALUE=%d\n", channel, value);
}

void BleManager::setBatteryLevel(uint8_t level) {
    if (level > 100) level = 100;
    _batteryLevel = level;
    
    if (_pBatteryCharacteristic) {
        _pBatteryCharacteristic->setValue(&_batteryLevel, 1);
        if (_deviceConnected) {
            _pBatteryCharacteristic->notify();
        } else {
            // Update advertising data with new battery level
            NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
            NimBLEAdvertisementData scanResponseData;
            
            std::string batteryData((char*)&_batteryLevel, 1);
            scanResponseData.setServiceData(NimBLEUUID((uint16_t)0x180F), batteryData);
            
            pAdvertising->setScanResponseData(scanResponseData);
        }
    }
}

void BleManager::ServerCallbacks::onConnect(NimBLEServer* pServer) {
    BleManager::getInstance()._deviceConnected = true;
    Serial.println("[BLE] Client connected");
    if (BleManager::getInstance()._connectionCallback) {
        BleManager::getInstance()._connectionCallback(true);
    }
}

void BleManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    BleManager::getInstance()._deviceConnected = false;
    Serial.println("[BLE] Client disconnected");
    if (BleManager::getInstance()._connectionCallback) {
        BleManager::getInstance()._connectionCallback(false);
    }
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising restarted");
}

void BleManager::CharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == BcbpProtocol::PACKET_SIZE_V1) {
        const uint8_t* data = (const uint8_t*)value.data();
        if (BcbpProtocol::validatePacket(data, value.length())) {
            BcbpPacketV1* packet = (BcbpPacketV1*)data;
            if (BleManager::getInstance()._packetCallback) {
                BleManager::getInstance()._packetCallback(packet);
            }
        } else {
            Serial.println("[BLE] Received invalid BCBP packet");
        }
    } else {
        Serial.printf("[BLE] Received data length: %d\n", value.length());
    }
}
