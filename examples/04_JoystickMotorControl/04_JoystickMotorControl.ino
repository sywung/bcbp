/*
  04_JoystickMotorControl.ino
  
  示範如何接收搖桿指令 (CMD_JOYSTICK) 並控制兩個直流馬達。
  搖桿數據範圍為 -100 到 +100。
  程式將根據 Y 軸計算前進/後退，根據 X 軸計算左右轉向。
*/

#include <BleManager.h>

// 定義馬達引腳 (以一般 ESP32 接 L298N 或 TB6612 為例)
struct MotorPins {
    int pwm;
    int in1;
    int in2;
};

MotorPins leftMotor = {25, 26, 27};
MotorPins rightMotor = {32, 33, 22};

// 設定 PWM 參數
const int freq = 5000;
const int resolution = 8; // 0-255

void setMotor(MotorPins& m, int speed) {
    if (speed > 0) {
        digitalWrite(m.in1, HIGH);
        digitalWrite(m.in2, LOW);
    } else if (speed < 0) {
        digitalWrite(m.in1, LOW);
        digitalWrite(m.in2, HIGH);
        speed = -speed;
    } else {
        digitalWrite(m.in1, LOW);
        digitalWrite(m.in2, LOW);
    }
    analogWrite(m.pwm, constrain(speed, 0, 255));
}

// 處理接收到的 BLE 封包
void onBlePacket(const BcbpPacketV1* packet) {
    if (packet->command == CMD_JOYSTICK) {
        // BCBP 搖桿協議中：
        // targetId 存放 X 軸數值 (int8_t, -100 ~ 100)
        // action   存放 Y 軸數值 (int8_t, -100 ~ 100)
        int8_t x = (int8_t)packet->targetId;
        int8_t y = (int8_t)packet->action;

        Serial.printf("搖桿數據 -> X: %d, Y: %d", x, y);

        // 簡易的差速驅動演算法
        // 將 -100~100 對應到 PWM 的 -255~255
        int throttle = map(y, -100, 100, -255, 255);
        int steering = map(x, -100, 100, -255, 255);

        int leftSpeed = throttle + steering;
        int rightSpeed = throttle - steering;

        setMotor(leftMotor, leftSpeed);
        setMotor(rightMotor, rightSpeed);
        
        Serial.printf("馬達出力 -> 左: %d, 右: %d", leftSpeed, rightSpeed);
    }
}

void setup() {
    Serial.begin(115200);

    // 初始化馬達引腳
    pinMode(leftMotor.pwm, OUTPUT);
    pinMode(leftMotor.in1, OUTPUT);
    pinMode(leftMotor.in2, OUTPUT);
    pinMode(rightMotor.pwm, OUTPUT);
    pinMode(rightMotor.in1, OUTPUT);
    pinMode(rightMotor.in2, OUTPUT);

    // 啟動 BLE
    BleManager::getInstance().setPacketCallback(onBlePacket);
    BleManager::getInstance().begin("ESP32-Tank-RC");

    Serial.println("搖桿馬達控制範例已啟動...");
}

void loop() {
    BleManager::getInstance().update();
    
    // 如果斷線，安全起見停止馬達
    if (!BleManager::getInstance().isConnected()) {
        setMotor(leftMotor, 0);
        setMotor(rightMotor, 0);
    }
}
