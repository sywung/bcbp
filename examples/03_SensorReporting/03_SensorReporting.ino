/*
  03_SensorReporting.ino
  
  示範如何定期發送感測器數據 (數位、類比) 與電池電量。
*/

#include <BleManager.h>

const int POT_PIN = 34;    // 類比感測器 (如電位計) 引腳
const int SENSOR_PIN = 13; // 數位感測器引腳

unsigned long lastReportTime = 0;
const unsigned long reportInterval = 1000; // 每秒發送一次

void setup() {
    Serial.begin(115200);
    pinMode(SENSOR_PIN, INPUT_PULLUP);

    BleManager::getInstance().begin("Sensor-Node");
    
    // 設定初始電池電量
    BleManager::getInstance().setBatteryLevel(100);
}

void loop() {
    BleManager::getInstance().update();

    // 定期回報感測器數值
    if (millis() - lastReportTime > reportInterval) {
        lastReportTime = millis();

        if (BleManager::getInstance().isConnected()) {
            // 1. 讀取並發送類比數值 (0-4095)
            uint16_t analogVal = analogRead(POT_PIN);
            BleManager::getInstance().sendAnalogReport(0, analogVal);
            
            // 2. 讀取並發送數位狀態 (0 或 1)
            bool digitalVal = digitalRead(SENSOR_PIN);
            BleManager::getInstance().sendDigitalReport(0, !digitalVal); // 反相因為 PULLUP

            // 3. 模擬電池下降
            static uint8_t battery = 100;
            if (battery > 10) battery--;
            BleManager::getInstance().setBatteryLevel(battery);

            Serial.printf("已回報: Analog=%d, Digital=%d, Battery=%d%%", 
                          analogVal, !digitalVal, battery);
        }
    }
}
