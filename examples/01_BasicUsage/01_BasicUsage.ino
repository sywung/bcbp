/*
  01_BasicUsage.ino
  
  示範如何最基礎地使用 BCBP 函式庫。
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
    switch (packet->command) {
        case CMD_BUTTON:
            Serial.printf("Button %d %s\n", packet->targetId,
                          packet->action == ACT_SHORT ? "DOWN" : "UP");
            break;
        case CMD_DIGITAL:
            Serial.printf("Digital CH%d = %d\n", packet->targetId, packet->action);
            break;
        case CMD_ANALOG: {
            // 16-bit value: action = high byte, sequence = low byte
            uint16_t value = BcbpProtocol::getAnalogValue(packet);
            Serial.printf("Analog CH%d = %d\n", packet->targetId, value);
            break;
        }
        default:
            Serial.printf("收到封包 - 指令: 0x%02X, 目標: %d, 動作: %d\n",
                          packet->command, packet->targetId, packet->action);
            break;
    }
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
    if (currentState != lastButtonState) {
        if (BleManager::getInstance().isConnected()) {
            if (currentState == LOW) {
                // 當按鈕按下時
                Serial.println("發送按鈕按下事件...");
                BleManager::getInstance().sendButtonEvent(1, ACT_SHORT);
            } else {
                // 當按鈕放開時
                Serial.println("發送按鈕放開事件...");
                BleManager::getInstance().sendButtonEvent(1, ACT_RELEASE);
            }
        }
        delay(50); // 簡易去彈跳
    }
    lastButtonState = currentState;
}
