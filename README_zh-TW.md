# Esp32BleControl 函式庫

這是一個輕量級的 ESP32 Arduino 函式庫，專門用於 BLE 通訊，並實作了 **按鈕控制二進位協定 (BCBP)**。此函式庫負責處理 BLE 堆疊管理（基於 NimBLE）、廣播以及各類數據傳輸（如按鈕事件、數位/類比感測器報告和電池電量）。

## 功能特色

*   **自動化 BLE 管理**：處理初始化、廣播與連線生命週期。
*   **內建 BCBP 協定**：內建 BCBP v1 封包的編碼與解碼功能。
*   **事件驅動架構**：使用 Callback 回呼函式來處理連線狀態變更與接收到的封包。
*   **電池服務支援**：支援標準 BLE Battery Service (0x180F)。
*   **低延遲效能**：基於高效能的 `NimBLE-Arduino` 底層。

## 相依套件

*   [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

## 安裝方式 (PlatformIO)

本函式庫設計為 PlatformIO 專案的本地 Library。請確認您的目錄結構如下：

```
project_root/
├── lib/
│   └── Esp32BleControl/
│       ├── README.md
│       ├── README_zh-TW.md
│       └── src/
│           ├── BleManager.h
│           ├── BleManager.cpp
│           └── BcbpProtocol.h
├── platformio.ini
└── src/
    └── main.cpp
```

在您的 `platformio.ini` 設定檔中加入依賴：

```ini
[env:esp32dev]
lib_deps =
    h2zero/NimBLE-Arduino
```

## 使用說明

### 1. 引入標頭檔

```cpp
#include <BleManager.h>
```

### 2. 定義 Callback 函式

在您的主程式中定義函式，用來處理 BLE 連線事件與接收到的資料封包。

```cpp
// 連線狀態回呼
void onBleConnect(bool connected) {
    if (connected) {
        Serial.println("已連線至 App!");
    } else {
        Serial.println("已斷線，重新廣播中...");
    }
}

// 收到封包回呼
void onBlePacket(const BcbpPacketV1* packet) {
    switch (packet->command) {
        case CMD_BUTTON:
            // 處理按鈕指令
            Serial.printf("按鈕 ID: %d, 動作: %d\n", packet->targetId, packet->action);
            break;
        case CMD_JOYSTICK:
            // 處理搖桿指令
            break;
        case CMD_DIGITAL:
            // 處理數位控制指令
            break;
        // ... 處理其他指令
    }
}
```

### 3. 初始化設定 (Setup)

在 `setup()` 函式中初始化 Library。

```cpp
void setup() {
    Serial.begin(115200);
    
    // 1. 取得實例
    BleManager& ble = BleManager::getInstance();

    // 2. 註冊 Callbacks
    ble.setConnectionCallback(onBleConnect);
    ble.setPacketCallback(onBlePacket);
    
    // 3. 設定初始電池電量 (0-100)
    ble.setBatteryLevel(100);
    
    // 4. 自定義 UUID (選用，必須在 begin 之前設定)
    // 可以一次設定全部：
    // ble.setCustomUUIDs("SERVICE-UUID", "RX-UUID", "TX-UUID");
    // 或者單獨設定：
    // ble.setServiceUUID("SERVICE-UUID");
    
    // 5. 啟動 BLE 並設定裝置名稱
    ble.begin("My-ESP32-Device");
}
```

### 4. 主迴圈 (Loop)

請務必在 `loop()` 中呼叫 `update()`。

```cpp
void loop() {
    BleManager::getInstance().update();
}
```

### 5. 發送資料

**發送按鈕事件：**
```cpp
// 目標 ID: 1, 動作: 短按 (ACT_SHORT)
BleManager::getInstance().sendButtonEvent(1, ACT_SHORT);
```

**發送數位感測器報告：**
```cpp
// 方式 A：傳送布林狀態 (通道: 0, 狀態: true/false)
BleManager::getInstance().sendDigitalReport(0, true);

// 方式 B：傳送位元組數值 (通道: 0, 數值: 0-255)
// 適合用於狀態遮罩 (Bitmask) 或自定義狀態碼
BleManager::getInstance().sendDigitalReport(0, (uint8_t)0xAA);
```

**發送類比感測器報告：**
```cpp
// 通道: 0, 數值: 1024
BleManager::getInstance().sendAnalogReport(0, 1024);
```

**更新電池電量：**
```cpp
BleManager::getInstance().setBatteryLevel(95);
```

## API 參考

### `BleManager` 類別

*   `static BleManager& getInstance()`: 取得 Singleton 實例。
*   `void begin(const char* deviceName)`: 初始化 BLE 並開始廣播。
*   `void update()`: 維護作業，需在 `loop()` 中呼叫（負責斷線後的重新廣播）。
*   `void setConnectionCallback(ConnectionCallback cb)`: 設定連線狀態改變時的回呼函式。
*   `void setPacketCallback(PacketCallback cb)`: 設定收到有效 BCBP 封包時的回呼函式。
*   `bool isConnected()`: 若有 Client 連線中則回傳 `true`。

### 設定方法 (Configuration)

*   `void setCustomUUIDs(const char* service, const char* rx, const char* tx)`: 一次設定所有自定義 UUID。
*   `void setServiceUUID(const char* uuid)`: 單獨設定 Service UUID。
*   `void setRxUUID(const char* uuid)`: 單獨設定 RX (Write) Characteristic UUID。
*   `void setTxUUID(const char* uuid)`: 單獨設定 TX (Notify) Characteristic UUID。

### 資料發送方法

*   `void sendButtonEvent(uint8_t targetId, ButtonAction action)`
*   `void sendDigitalReport(uint8_t channel, bool state)`: 發送布林狀態 (0 或 1)。
*   `void sendDigitalReport(uint8_t channel, uint8_t state)`: 發送 8-bit 位元組狀態。
*   `void sendAnalogReport(uint8_t channel, uint16_t value)`
*   `void setBatteryLevel(uint8_t level)`

## 協定常數 (`BcbpProtocol.h`)

**指令類型 (`packet->command`):**
*   `CMD_BUTTON` (0x01)
*   `CMD_JOYSTICK` (0x02)
*   `CMD_DIGITAL` (0x11)
*   `CMD_ANALOG` (0x12)

**按鈕動作 (`packet->action`):**
*   `ACT_SHORT` (0x01)
*   `ACT_LONG` (0x02)
*   `ACT_DOUBLE` (0x03)

## 範例程式碼

```cpp
#include <BleManager.h>

// 定義回呼函式
void onBleConnect(bool connected) {
  if (connected) {
    Serial.println("已連線");
  } else {
    Serial.println("已斷線");
  }
}

void onBlePacket(const BcbpPacketV1* packet) {
  // 處理收到的指令
  Serial.printf("Command: %02X\n", packet->command);
}

void setup() {
  Serial.begin(115200);

  // 初始化
  BleManager::getInstance().setConnectionCallback(onBleConnect);
  BleManager::getInstance().setPacketCallback(onBlePacket);
  BleManager::getInstance().begin("ESP32-Device");
}

void loop() {
  // 務必在 loop 呼叫 update
  BleManager::getInstance().update();

  // 範例：發送按鈕事件
  // BleManager::getInstance().sendButtonEvent(1, ACT_SHORT);
}
```
