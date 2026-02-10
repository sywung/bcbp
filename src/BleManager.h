#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include <functional>
#include "BcbpProtocol.h"

// Callback types
typedef std::function<void(bool connected)> ConnectionCallback;
typedef std::function<void(const BcbpPacketV1* packet)> PacketCallback;

class BleManager {
public:
    static BleManager& getInstance();
    void begin(const char* deviceName);
    void update();
    
    // Configuration
    void setCustomUUIDs(const char* serviceUUID, const char* rxUUID, const char* txUUID);
    void setServiceUUID(const char* uuid);
    void setRxUUID(const char* uuid);
    void setTxUUID(const char* uuid);
    
    // Callback setters
    void setConnectionCallback(ConnectionCallback cb);
    void setPacketCallback(PacketCallback cb);

    void sendButtonEvent(uint8_t targetId, ButtonAction action);
    void sendDigitalReport(uint8_t channel, bool state);
    void sendDigitalReport(uint8_t channel, uint8_t state);
    void sendAnalogReport(uint8_t channel, uint16_t value);
    void setBatteryLevel(uint8_t level);
    bool isConnected();

private:
    BleManager();
    
    NimBLEServer* _pServer;
    NimBLECharacteristic* _pTxCharacteristic;
    NimBLECharacteristic* _pRxCharacteristic;
    NimBLECharacteristic* _pBatteryCharacteristic;
    bool _deviceConnected;
    uint8_t _sequence;
    uint8_t _batteryLevel;

    std::string _serviceUUID;
    std::string _rxUUID;
    std::string _txUUID;

    ConnectionCallback _connectionCallback;
    PacketCallback _packetCallback;

    class ServerCallbacks : public NimBLEServerCallbacks {
        void onConnect(NimBLEServer* pServer) override;
        void onDisconnect(NimBLEServer* pServer) override;
    };

    class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic* pCharacteristic) override;
    };
    
    // Friend classes to allow access to private members/callbacks
    friend class ServerCallbacks;
    friend class CharacteristicCallbacks;
};

#endif
