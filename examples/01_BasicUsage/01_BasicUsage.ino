/*
  01_BasicUsage.ino
  
  示範如何最基礎地使用 Esp32BleControl 函式庫。
  包含初始化、連線回呼、以及發送按鈕事件。
*/

#include <BleManager.h>

// 假設開發板上的按鈕接在 GPIO 0 (大多數 ESP32 的 BOOT 鍵)
const int BUTTON_PIN = 0;
bool lastButtonState = HIGH;

// 當連線狀態改變時會執行此函式
void onBleConnection(bool connected) {
    if (connected) {
        Serial.println(">>> BLE 已連線！");
    } else {
        Serial.println(">>> BLE 已斷線，正在重新廣播...");
    }
}

// 當接收到來自 App 的封包時會執行此函式
void onBlePacketReceived(const BcbpPacketV1* packet) {
    Serial.printf("收到封包 - 指令: 0x%02X, 目標: %d, 動作: %d", 
                  packet->command, packet->targetId, packet->action);
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // 1. 設定回呼函式 (非必選，但建議設定)
    BleManager::getInstance().setConnectionCallback(onBleConnection);
    BleManager::getInstance().setPacketCallback(onBlePacketReceived);

    // 2. 啟動 BLE
    // 這會初始化 NimBLE 並開始廣播名稱為 "ESP32-Basic" 的裝置
    BleManager::getInstance().begin("ESP32-Basic");

    Serial.println("BLE 初始化完成，等待連線...");
}

void loop() {
    // 必須在 loop 中呼叫 update 以維持運作
    BleManager::getInstance().update();

    // 簡易的按鈕偵測範例
    bool currentState = digitalRead(BUTTON_PIN);
    if (currentState == LOW && lastButtonState == HIGH) {
        // 當按鈕按下時，發送一個「按鈕 1 短按」的事件給 App
        if (BleManager::getInstance().isConnected()) {
            Serial.println("發送按鈕事件...");
            BleManager::getInstance().sendButtonEvent(1, ACT_SHORT);
        } else {
            Serial.println("尚未連線，無法發送");
        }
        delay(50); // 簡易去彈跳
    }
    lastButtonState = currentState;
}
