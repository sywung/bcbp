/*
  06_SoundFeedback.ino
  
  示範如何發送聲音回饋 (Sound Feedback) 給 App，以及接收來自 App 的聲音指令。
  1. 當按鍵按下時，發送聲音回饋給 App，讓手機播放音效。
  2. 當 App 發送聲音指令時，控制 ESP32 上的蜂鳴器 (以 GPIO 模擬)。
*/

#include <BleManager.h>

const int BUTTON_PIN = 0;    // BOOT 鍵
const int BUZZER_PIN = 25;   // 假設蜂鳴器接在 GPIO 25

bool lastBtnState = HIGH;

// 處理來自 App 的 BLE 指令
void onPacketReceived(const BcbpPacketV1* packet) {
    // 檢查是否為聲音指令 (CMD_SOUND)
    if (packet->command == CMD_SOUND) {
        uint8_t soundId = packet->targetId; // 聲音 ID
        uint8_t volume = packet->action;   // 音量 (0-255)

        Serial.printf("收到聲音指令: ID=%d, 音量=%d\n", soundId, volume);

        // 簡單的物理蜂鳴器模擬 (鳴叫 100 毫秒)
        tone(BUZZER_PIN, 2000, 100); 
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);

    // 設定回呼
    BleManager::getInstance().setPacketCallback(onPacketReceived);

    // 啟動 BLE
    BleManager::getInstance().begin("ESP32-Sound");

    Serial.println("聲音回饋範例已啟動...");
}

void loop() {
    BleManager::getInstance().update();

    // 偵測按鈕
    bool currentBtnState = digitalRead(BUTTON_PIN);
    if (currentBtnState == LOW && lastBtnState == HIGH) {
        if (BleManager::getInstance().isConnected()) {
            Serial.println("發送成功音效 (SOUND_SUCCESS) 給 App...");
            
            // 使用內建函式發送音效回饋
            // ID 有: SOUND_BEEP, SOUND_SUCCESS, SOUND_ERROR, SOUND_ALERT, SOUND_DOUBLE 等
            BleManager::getInstance().sendSoundFeedback(SOUND_SUCCESS);
        }
        delay(50);
    }
    lastBtnState = currentBtnState;
}
