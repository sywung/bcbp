/*
  05_VibrationFeedback.ino
  
  示範如何發送震動回饋 (Haptic Feedback) 給 App，以及接收來自 App 的震動指令。
  1. 當按鍵按下時，發送震動回饋給 App。
  2. 當 App 發送震動指令時，控制 ESP32 上的震動馬達 (以 LED 或 GPIO 模擬)。
*/

#include <BleManager.h>

const int BUTTON_PIN = 0;    // BOOT 鍵
const int VIB_MOTOR_PIN = 2; // 假設震動馬達接在 GPIO 2 (內建 LED)

bool lastBtnState = HIGH;

// 處理來自 App 的 BLE 指令
void onPacketReceived(const BcbpPacketV1* packet) {
    // 檢查是否為震動指令 (CMD_HAPTIC)
    if (packet->command == CMD_HAPTIC) {
        uint8_t pattern = packet->targetId; // 震動模式
        uint8_t intensity = packet->action; // 震動強度 (0-255)

        Serial.printf("收到震動指令: 模式=%d, 強度=%d\n", pattern, intensity);

        // 簡單的物理震動模擬
        digitalWrite(VIB_MOTOR_PIN, HIGH);
        delay(100); 
        digitalWrite(VIB_MOTOR_PIN, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(VIB_MOTOR_PIN, OUTPUT);

    // 設定回呼
    BleManager::getInstance().setPacketCallback(onPacketReceived);

    // 啟動 BLE
    BleManager::getInstance().begin("ESP32-Vibration");

    Serial.println("震動回饋範例已啟動...");
}

void loop() {
    BleManager::getInstance().update();

    // 偵測按鈕
    bool currentBtnState = digitalRead(BUTTON_PIN);
    if (currentBtnState == LOW && lastBtnState == HIGH) {
        if (BleManager::getInstance().isConnected()) {
            Serial.println("發送震動回饋 (HAPTIC_SHORT) 給 App...");
            
            // 使用內建函式發送震動回饋
            // 模式有: HAPTIC_SHORT, HAPTIC_LONG, HAPTIC_DOUBLE, HAPTIC_SUCCESS, HAPTIC_ERROR 等
            BleManager::getInstance().sendHapticFeedback(HAPTIC_SHORT);
        }
        delay(50);
    }
    lastBtnState = currentBtnState;
}
