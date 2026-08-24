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
    // --- App <-> Device (RX) ---
    CMD_BUTTON   = 0x01,
    CMD_JOYSTICK = 0x02,

    // --- Device -> App (TX) ---
    CMD_DIGITAL  = 0x11,   // 數位狀態報告
    CMD_ANALOG   = 0x12,   // 類比數值報告
    CMD_HAPTIC   = 0x21,   // Vibration feedback
    CMD_SOUND    = 0x22,   // Sound feedback
    CMD_FEEDBACK = 0x23,   // Both haptic and sound

    // Matrix Clock time synchronization transaction.
    CMD_TIME_SYNC_LO = 0x30,
    CMD_TIME_SYNC_HI = 0x31,
    CMD_TIME_SYNC_COMMIT = 0x32,
    CMD_TIME_SYNC_ACK = 0x33,

    // Matrix Clock settings.
    CMD_BLE_NAME_BEGIN = 0x48,
    CMD_BLE_NAME_DATA = 0x49,
    CMD_BLE_NAME_COMMIT = 0x4A,
    CMD_SETTINGS_ACK = 0x4D,

    // DEC-022 blob transport and read-only status query.
    CMD_BLOB_BEGIN = 0x60,
    CMD_BLOB_DATA = 0x61,
    CMD_BLOB_END = 0x62,
    CMD_BLOB_ACK = 0x63,
    CMD_STATUS_QUERY = 0x64
};

// The receiver allocates this buffer once. Do not raise this without checking
// the target's free heap and the size of every receiver-side buffer.
static constexpr size_t BCBP_BLOB_MAX_LENGTH = 512;

enum BcbpBlobResult : uint8_t {
    BCBP_BLOB_OK = 0,
    BCBP_BLOB_TOO_LARGE = 1,
    BCBP_BLOB_CRC_OR_INCOMPLETE = 2,
    BCBP_BLOB_TRANSACTION_ERROR = 3,
    BCBP_BLOB_COMMAND_ERROR = 4
};

enum ButtonAction : uint8_t {
    ACT_RELEASE = 0x00,
    ACT_SHORT   = 0x01,
    ACT_LONG    = 0x02,
    ACT_DOUBLE  = 0x03
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
    // v2 carries its payload length in the header, so callers must validate
    // both the declared and actual packet sizes before accessing the payload.
    static const size_t HEADER_SIZE_V2 = 4;
    static const size_t OVERHEAD_V2 = 5;

    /**
     * Extract 16-bit analog value from a CMD_ANALOG packet.
     * For ANALOG packets, `action` holds the high byte and `sequence` holds the low byte.
     */
    static uint16_t getAnalogValue(const BcbpPacketV1* packet) {
        return ((uint16_t)packet->action << 8) | packet->sequence;
    }

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

    // CRC-32/IEEE, reflected polynomial, without a lookup table to save flash.
    static uint32_t calculateCRC32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; bit++) {
                crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320) : (crc >> 1);
            }
        }
        return ~crc;
    }

    static bool validatePacket(const uint8_t* data, size_t len) {
        if (len != PACKET_SIZE_V1) return false;
        if (data[0] != BCBP_V1) return false;
        
        // If CRC is 0, it might be in debug mode (as per protocol spec note)
        // but for strictness we should probably always check if it's not explicitly disabled.
        uint8_t calculatedCrc = calculateCRC8(data, PACKET_SIZE_V1 - 1);
        return (data[PACKET_SIZE_V1 - 1] == calculatedCrc);
    }

    static bool validatePacketV2(const uint8_t* data, size_t len) {
        if (len < OVERHEAD_V2) return false;
        if (data == nullptr) return false;
        if (data[0] != BCBP_V2) return false;
        if (len != OVERHEAD_V2 + data[3]) return false;

        uint8_t calculatedCrc = calculateCRC8(data, len - 1);
        return (data[len - 1] == calculatedCrc);
    }
};

#endif
