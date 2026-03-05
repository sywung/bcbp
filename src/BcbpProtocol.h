#ifndef BCBP_PROTOCOL_H
#define BCBP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/**
 * Bluetooh Control Binary Protocol (BCBP) v1
 */

enum BcbpVersion : uint8_t {
    BCBP_V1 = 0x01,
    BCBP_V2 = 0x02
};

enum BcbpCommand : uint8_t {
    // --- App -> Device (RX) ---
    CMD_BUTTON   = 0x01,
    CMD_JOYSTICK = 0x02,
    CMD_DIGITAL  = 0x11,
    CMD_ANALOG   = 0x12,

    // --- Device -> App (TX) ---
    CMD_HAPTIC   = 0x21,   // Vibration feedback
    CMD_SOUND    = 0x22,   // Sound feedback
    CMD_FEEDBACK = 0x23    // Both haptic and sound
};

enum ButtonAction : uint8_t {
    ACT_SHORT  = 0x01,
    ACT_LONG   = 0x02,
    ACT_DOUBLE = 0x03
};

enum JoystickFlags : uint8_t {
    JS_ABSOLUTE = 0x00,
    JS_RELATIVE = 0x01
};

enum HapticPattern : uint8_t {
    HAPTIC_SHORT   = 0x01,  // ~50ms
    HAPTIC_LONG    = 0x02,  // ~300ms
    HAPTIC_DOUBLE  = 0x03,  // Double tap
    HAPTIC_SUCCESS = 0x04,  // Short-Long
    HAPTIC_ERROR   = 0x05,  // Long-Long-Long
    HAPTIC_WARNING = 0x06   // Short-Short
};

enum SoundID : uint8_t {
    SOUND_BEEP    = 0x01,
    SOUND_SUCCESS = 0x02,
    SOUND_ERROR   = 0x03,
    SOUND_ALERT   = 0x04,
    SOUND_DOUBLE  = 0x05
};

#pragma pack(push, 1)
/**
 * BCBP v1 Packet Structure (6 bytes)
 * 
 * Field Mapping by Command:
 * | Cmd          | targetId | action      | sequence    |
 * |--------------|----------|-------------|-------------|
 * | BUTTON       | ID       | Action      | Seq         |
 * | JOYSTICK     | X (-127) | Y (-127)    | Seq         |
 * | DIGITAL      | Channel  | State (0/1) | Seq         |
 * | ANALOG       | Channel  | Value High  | Value Low   |
 * | HAPTIC       | Pattern  | Intensity   | Seq         |
 * | SOUND        | Sound ID | Volume      | Seq         |
 * | FEEDBACK     | Haptic   | Sound ID    | Seq         |
 */
struct BcbpPacketV1 {
    uint8_t version;
    uint8_t command;
    uint8_t targetId;
    uint8_t action;
    uint8_t sequence;
    uint8_t crc8;
};
#pragma pack(pop)

class BcbpProtocol {
public:
    static const size_t PACKET_SIZE_V1 = 6;

    static uint8_t calculateCRC8(const uint8_t* data, size_t len) {
        uint8_t crc = 0x00;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x07;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }

    static bool validatePacket(const uint8_t* data, size_t len) {
        if (len != PACKET_SIZE_V1) return false;
        if (data[0] != BCBP_V1) return false;
        
        // If CRC is 0, it might be in debug mode (as per protocol spec note)
        // but for strictness we should probably always check if it's not explicitly disabled.
        uint8_t calculatedCrc = calculateCRC8(data, PACKET_SIZE_V1 - 1);
        return (data[PACKET_SIZE_V1 - 1] == calculatedCrc);
    }
};

#endif
