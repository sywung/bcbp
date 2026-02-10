/*
  02_CustomUUIDs.ino
  
  示範如何自定義 BLE Service 與 Characteristic 的 UUID。
  如果你有特定的 App 需求，或是想區隔不同的產品線，可以使用此功能。
*/

#include <BleManager.h>

void setup() {
    Serial.begin(115200);

    // 取得實例
    BleManager& ble = BleManager::getInstance();

    // 在呼叫 begin 之前，設定自定義 UUID
    // 參數順序：Service UUID, RX UUID, TX UUID
    // 如果只想改其中一個，可以傳入 nullptr 或使用獨立的 Setter
    
    // 方式 A：一次設定全部
    ble.setCustomUUIDs(
        "12345678-1234-1234-1234-1234567890ab", // Service
        "12345678-1234-1234-1234-1234567890ac", // RX (Write)
        "12345678-1234-1234-1234-1234567890ad"  // TX (Notify)
    );

    /* 
    方式 B：單獨設定 (範例)
    ble.setServiceUUID("73616968-7334-2722-6616-737977756e67");
    */

    // 啟動 BLE
    ble.begin("Custom-UUID-Device");

    Serial.println("使用自定義 UUID 啟動完成");
}

void loop() {
    BleManager::getInstance().update();
}
