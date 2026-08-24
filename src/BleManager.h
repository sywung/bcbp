#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include <functional>
#include "BcbpProtocol.h"

// Debug Logging Macros
#ifndef BCBP_ENABLE_DEBUG
#define BCBP_ENABLE_DEBUG 1  // Set to 0 to completely remove debug code
#endif

#if BCBP_ENABLE_DEBUG
  #define BCBP_LOGF(...) Serial.printf(__VA_ARGS__)
  #define BCBP_LOG(...)  Serial.println(__VA_ARGS__)
#else
  #define BCBP_LOGF(...)
  #define BCBP_LOG(...)
#endif

// Callback types
typedef std::function<void(bool connected)> ConnectionCallback;
typedef std::function<void(const BcbpPacketV1* packet)> PacketCallback;
typedef std::function<void(uint8_t command, uint8_t sequence,
                           const uint8_t* payload, uint8_t length)>
    PacketV2Callback;

class BleManager {
public:
    static BleManager& getInstance();
    void begin(const char* deviceName);
    void setDeviceName(const char* deviceName);
    void update();
    
    // Configuration
    void setCustomUUIDs(const char* serviceUUID, const char* rxUUID, const char* txUUID);
    void setServiceUUID(const char* uuid);
    void setRxUUID(const char* uuid);
    void setTxUUID(const char* uuid);
    
    // Callback setters
    void setConnectionCallback(ConnectionCallback cb);
    void setPacketCallback(PacketCallback cb);
    void setPacketV2Callback(PacketV2Callback cb);

    // Send a caller-built BCBP v1 packet through the TX notification channel.
    // The packet must already contain its CRC8.
    void sendPacket(BcbpPacketV1& packet);
    void sendPacketV2(uint8_t command, uint8_t sequence,
                      const uint8_t* payload, uint8_t length);
    bool sendBlob(uint8_t streamId, uint8_t sequence, uint8_t type,
                  const uint8_t* data, size_t length);

    void sendButtonEvent(uint8_t targetId, ButtonAction action);
    void sendDigitalReport(uint8_t channel, bool state);
    void sendDigitalReport(uint8_t channel, uint8_t state);
    void sendAnalogReport(uint8_t channel, uint16_t value);
    
    // Device -> App Feedback
    void sendHapticFeedback(HapticPattern pattern, uint8_t intensity = 0xFF);
    void sendSoundFeedback(SoundID soundId, uint8_t volume = 0xFF);
    void sendCombinedFeedback(HapticPattern pattern, SoundID soundId);

    void setBatteryLevel(uint8_t level);
    bool isConnected();
    uint16_t getNegotiatedMtu() const;

    // Permanent notify health counters; retain for DEC-022 P2 troubleshooting.
    uint32_t notifyCalls() const;
    uint32_t notifyRetries() const;
    uint32_t notifyFailures() const;
    void resetNotifyStats();

private:
    BleManager();
    
    NimBLEServer* _pServer;
    NimBLECharacteristic* _pTxCharacteristic;
    NimBLECharacteristic* _pRxCharacteristic;
    NimBLECharacteristic* _pBatteryCharacteristic;
    bool _deviceConnected;
    // Written by NimBLE callbacks and read from other tasks; keep the value observable.
    volatile uint16_t _negotiatedMtu;
    // Written by the BLE task and read from other tasks; integer-only notify
    // health counters retained for DEC-022 P2 troubleshooting.
    volatile uint32_t _notifyCalls;
    volatile uint32_t _notifyRetries;
    volatile uint32_t _notifyFailures;
    uint8_t _sequence;
    uint8_t _batteryLevel;

    std::string _serviceUUID;
    std::string _rxUUID;
    std::string _txUUID;

    ConnectionCallback _connectionCallback;
    PacketCallback _packetCallback;
    PacketV2Callback _packetV2Callback;

    class ServerCallbacks : public NimBLEServerCallbacks {
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
        void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override;
    };

    class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    };
    
    void _sendPacket(BcbpPacketV1& packet);

    // Friend classes to allow access to private members/callbacks
    friend class ServerCallbacks;
    friend class CharacteristicCallbacks;
};

#endif
