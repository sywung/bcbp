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
    CMD_BUTTON   = 0x01,
    CMD_JOYSTICK = 0x02,
    CMD_DIGITAL  = 0x11,
    CMD_ANALOG   = 0x12
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

#pragma pack(push, 1)
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
