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
    _negotiatedMtu(0),
    _notifyCalls(0),
    _notifyRetries(0),
    _notifyFailures(0),
    _sequence(0), 
    _batteryLevel(100), 
    _serviceUUID(SERVICE_UUID),
    _rxUUID(RX_CHARACTERISTIC_UUID),
    _txUUID(TX_CHARACTERISTIC_UUID),
    _connectionCallback(nullptr), 
    _packetCallback(nullptr),
    _packetV2Callback(nullptr) {}

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
    NimBLEDevice::setMTU(517);
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
    
    // The name is in scan response data so the 128-bit service UUID remains
    // in the primary advertisement. Apps must read advName from scan results.
    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName(deviceName);

    // Add Current Battery Level to Scan Response
    std::string batteryData((char*)&_batteryLevel, 1);
    scanResponseData.setServiceData(NimBLEUUID((uint16_t)0x180F), batteryData);
    
    pAdvertising->setScanResponseData(scanResponseData);
    pAdvertising->start();
    
    BCBP_LOGF("[BLE] Advertising started as %s (Initial Battery: %d%%)\n", deviceName, _batteryLevel);
}

void BleManager::setDeviceName(const char* deviceName) {
    if (deviceName == nullptr || deviceName[0] == '\0') return;
    NimBLEDevice::setDeviceName(deviceName);
    NimBLEDevice::getAdvertising()->stop();
    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName(deviceName);
    std::string batteryData((char*)&_batteryLevel, 1);
    scanResponseData.setServiceData(NimBLEUUID((uint16_t)0x180F), batteryData);
    NimBLEDevice::getAdvertising()->setScanResponseData(scanResponseData);
    NimBLEDevice::startAdvertising();
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

void BleManager::setPacketV2Callback(PacketV2Callback cb) {
    _packetV2Callback = cb;
}

void BleManager::sendPacket(BcbpPacketV1& packet) {
    if (!_deviceConnected) return;
    _sendPacket(packet);
}

void BleManager::sendPacketV2(uint8_t command, uint8_t sequence,
                              const uint8_t* payload, uint8_t length) {
    if (!_deviceConnected) return;
    if (payload == nullptr) length = 0;

    uint8_t packet[BcbpProtocol::OVERHEAD_V2 + 255];
    packet[0] = BCBP_V2;
    packet[1] = command;
    packet[2] = sequence;
    packet[3] = length;
    if (length > 0) {
        memcpy(packet + BcbpProtocol::HEADER_SIZE_V2, payload, length);
    }
    const size_t packetLength = BcbpProtocol::OVERHEAD_V2 + length;
    packet[packetLength - 1] = BcbpProtocol::calculateCRC8(packet, packetLength - 1);

    // Same overwrite race as _sendPacket() — see the comment there (BUG-008).
    // This path matters most for DEC-022 P2: a BLOB stream is a burst by
    // definition, so the no-argument notify() would lose all but the last
    // chunks. setValue() stays for GATT reads.
    _pTxCharacteristic->setValue(packet, packetLength);
    uint8_t retries = 5;
    while (!_pTxCharacteristic->notify(packet, packetLength) && retries--) {
        delay(1);
    }
}

bool BleManager::sendBlob(uint8_t streamId, uint8_t sequence, uint8_t type,
                          const uint8_t* data, size_t length) {
    if (!_deviceConnected || (data == nullptr && length != 0)) return false;

    const uint16_t mtu = getNegotiatedMtu();
    // A v2 packet has a one-byte length field. The 250-byte data cap comes
    // from that field, not from the negotiated MTU. Calculate the other cap
    // dynamically because ATT payload capacity changes with the MTU.
    // The UINT32_MAX check is for portability; size_t is 32-bit on ESP32,
    // so this condition cannot occur on that target.
    if (mtu <= 13 || length > UINT32_MAX) return false;
    const size_t chunkSize = min(static_cast<size_t>(mtu - 13), size_t(250));

    // The caller supplies the sequence so the receiver can associate every
    // response with its query and discard stale packets. For a blob, a stale
    // packet can be worse than for a single packet: its wrong offset can
    // silently corrupt the reassembled result.
    uint8_t payload[6];
    payload[0] = streamId;
    const uint32_t totalLength = static_cast<uint32_t>(length);
    payload[1] = static_cast<uint8_t>(totalLength);
    payload[2] = static_cast<uint8_t>(totalLength >> 8);
    payload[3] = static_cast<uint8_t>(totalLength >> 16);
    payload[4] = static_cast<uint8_t>(totalLength >> 24);
    payload[5] = type;
    sendPacketV2(CMD_BLOB_BEGIN, sequence, payload, sizeof(payload));

    uint8_t dataPayload[255];
    for (size_t offset = 0; offset < length; offset += chunkSize) {
        const size_t dataLength = min(chunkSize, length - offset);
        dataPayload[0] = streamId;
        dataPayload[1] = static_cast<uint8_t>(offset);
        dataPayload[2] = static_cast<uint8_t>(offset >> 8);
        dataPayload[3] = static_cast<uint8_t>(offset >> 16);
        dataPayload[4] = static_cast<uint8_t>(offset >> 24);
        memcpy(dataPayload + 5, data + offset, dataLength);
        // The offset lets the receiver identify a missing BLE packet and
        // locate the corresponding bytes (BUG-008, protocol section 9).
        sendPacketV2(CMD_BLOB_DATA, sequence, dataPayload,
                     static_cast<uint8_t>(dataLength + 5));
    }

    const uint32_t crc = BcbpProtocol::calculateCRC32(data, length);
    payload[0] = streamId;
    payload[1] = static_cast<uint8_t>(crc);
    payload[2] = static_cast<uint8_t>(crc >> 8);
    payload[3] = static_cast<uint8_t>(crc >> 16);
    payload[4] = static_cast<uint8_t>(crc >> 24);
    sendPacketV2(CMD_BLOB_END, sequence, payload, 5);
    return true;
}

bool BleManager::isConnected() {
    return _deviceConnected;
}

uint16_t BleManager::getNegotiatedMtu() const {
    return _negotiatedMtu;
}

uint32_t BleManager::notifyCalls() const {
    return _notifyCalls;
}

uint32_t BleManager::notifyRetries() const {
    return _notifyRetries;
}

uint32_t BleManager::notifyFailures() const {
    return _notifyFailures;
}

void BleManager::resetNotifyStats() {
    _notifyCalls = 0;
    _notifyRetries = 0;
    _notifyFailures = 0;
}

void BleManager::_sendPacket(BcbpPacketV1& packet) {
    // Notify the explicit payload, rather than the characteristic's stored value.
    // The no-argument notify() sends whatever value the characteristic holds at
    // the time the BLE stack processes the request. During a burst, the next
    // setValue() can overwrite that shared value before the previous request is
    // actually transmitted, causing the receiver to see only the last packets,
    // often repeatedly. The payload overload copies this packet when notify() is
    // called, so each request retains the bytes intended for that packet.
    //
    // Keep setValue() because the current value must remain available to GATT
    // reads; it is not a substitute for the payload passed to notify(). Also,
    // notify() returning true only means that the request was accepted locally,
    // not that the client received it. Consequently, increasing the retry count
    // cannot fix this overwrite race: BUG-008 showed zero retries and failures
    // even while packets were being lost. The longer burst time after this fix
    // is the cost of copying each payload into its own mbuf, replacing the old
    // no-copy path; it is not caused by an added delay.
    //
    // These counters are intentionally permanent notify health metrics. They
    // perform integer increments only in this hot path and remain useful for
    // DEC-022 P2 troubleshooting.
    _notifyCalls++;
    _pTxCharacteristic->setValue((uint8_t*)&packet, sizeof(packet));
    uint8_t retries = 5;
    bool notified = _pTxCharacteristic->notify((const uint8_t*)&packet, sizeof(packet));
    while (!notified && retries > 0) {
        --retries;
        ++_notifyRetries;
        delay(1);
        notified = _pTxCharacteristic->notify((const uint8_t*)&packet, sizeof(packet));
    }
    if (!notified) {
        ++_notifyFailures;
    }
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

    _sendPacket(packet);
    BCBP_LOGF("[BLE] Sent Button Packet: ID=%d, ACT=%d, SEQ=%d\n", targetId, (uint8_t)action, packet.sequence);
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

    _sendPacket(packet);
    BCBP_LOGF("[BLE] Sent DI Packet: CH=%d, STATE=%d, SEQ=%d\n", channel, state, packet.sequence);
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

    _sendPacket(packet);
    BCBP_LOGF("[BLE] Sent AI Packet: CH=%d, VALUE=%d\n", channel, value);
}

void BleManager::sendHapticFeedback(HapticPattern pattern, uint8_t intensity) {
    if (!_deviceConnected) return;

    BcbpPacketV1 packet;
    packet.version = BCBP_V1;
    packet.command = CMD_HAPTIC;
    packet.targetId = (uint8_t)pattern;
    packet.action = intensity;
    packet.sequence = _sequence++;
    packet.crc8 = BcbpProtocol::calculateCRC8((uint8_t*)&packet, 5);

    _sendPacket(packet);
    BCBP_LOGF("[BLE] Sent Haptic: PAT=%d, INT=%d\n", (uint8_t)pattern, intensity);
}

void BleManager::sendSoundFeedback(SoundID soundId, uint8_t volume) {
    if (!_deviceConnected) return;

    BcbpPacketV1 packet;
    packet.version = BCBP_V1;
    packet.command = CMD_SOUND;
    packet.targetId = (uint8_t)soundId;
    packet.action = volume;
    packet.sequence = _sequence++;
    packet.crc8 = BcbpProtocol::calculateCRC8((uint8_t*)&packet, 5);

    _sendPacket(packet);
    BCBP_LOGF("[BLE] Sent Sound: ID=%d, VOL=%d\n", (uint8_t)soundId, volume);
}

void BleManager::sendCombinedFeedback(HapticPattern pattern, SoundID soundId) {
    if (!_deviceConnected) return;

    BcbpPacketV1 packet;
    packet.version = BCBP_V1;
    packet.command = CMD_FEEDBACK;
    packet.targetId = (uint8_t)pattern;
    packet.action = (uint8_t)soundId;
    packet.sequence = _sequence++;
    packet.crc8 = BcbpProtocol::calculateCRC8((uint8_t*)&packet, 5);

    _sendPacket(packet);
    BCBP_LOGF("[BLE] Sent Feedback: HAP=%d, SND=%d\n", (uint8_t)pattern, (uint8_t)soundId);
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

void BleManager::ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    BleManager::getInstance()._deviceConnected = true;
    BleManager::getInstance()._negotiatedMtu = connInfo.getMTU();
    BCBP_LOG("[BLE] Client connected");
    if (BleManager::getInstance()._connectionCallback) {
        BleManager::getInstance()._connectionCallback(true);
    }
}

void BleManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    BleManager::getInstance()._deviceConnected = false;
    BleManager::getInstance()._negotiatedMtu = 0;
    BCBP_LOG("[BLE] Client disconnected");
    if (BleManager::getInstance()._connectionCallback) {
        BleManager::getInstance()._connectionCallback(false);
    }
    NimBLEDevice::startAdvertising();
    BCBP_LOG("[BLE] Advertising restarted");
}

void BleManager::ServerCallbacks::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) {
    BleManager::getInstance()._negotiatedMtu = MTU;
}

void BleManager::CharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) {
        BCBP_LOGF("[BLE] Received data length: %d\n", value.length());
    } else if (((const uint8_t*)value.data())[0] == BCBP_V1 &&
               value.length() == BcbpProtocol::PACKET_SIZE_V1) {
        const uint8_t* data = (const uint8_t*)value.data();
        if (BcbpProtocol::validatePacket(data, value.length())) {
            BcbpPacketV1* packet = (BcbpPacketV1*)data;
            if (BleManager::getInstance()._packetCallback) {
                BleManager::getInstance()._packetCallback(packet);
            }
        } else {
            BCBP_LOG("[BLE] Received invalid BCBP packet");
        }
    } else if (((const uint8_t*)value.data())[0] == BCBP_V2) {
        const uint8_t* data = (const uint8_t*)value.data();
        if (BcbpProtocol::validatePacketV2(data, value.length())) {
            if (BleManager::getInstance()._packetV2Callback) {
                BleManager::getInstance()._packetV2Callback(
                    data[1], data[2], data + BcbpProtocol::HEADER_SIZE_V2,
                    data[3]);
            }
        } else {
            BCBP_LOG("[BLE] Received invalid BCBP v2 packet");
        }
    } else {
        BCBP_LOGF("[BLE] Received data length: %d\n", value.length());
    }
}
