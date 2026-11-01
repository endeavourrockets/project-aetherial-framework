/*
 * can_ids.h --> DRAFT CAN message ids and what goes in each message
 * DRAFT: nothing here is agreed with the team yet --> just the two OBC
 * messages the PoC programs use, the full id list comes later
 *
 * rules i used for the draft:
 * lower id --> higher priority (it wins arbitration when two boards talk at once)
 * numbers bigger than a byte are little endian (lowest byte first), like the MCU
 * every message has something that changes each time (a counter or a time
 *   stamp) --> a board on two redundant buses uses it to spot the second copy
 *   of the same message and ignore it (see experiment/main.c)
 */
#ifndef CAN_IDS_H
#define CAN_IDS_H

#include <stdint.h>

#define CAN_BITRATE_DRAFT 500000u /* same as CAN_BITRATE in payload-config/config.h */

/* 0x080 TIME_SYNC: OBC --> everyone, 1 Hz, 8 bytes
 * bytes 0-3 --> OBC mission time in ms (u32, time since the OBC reset)
 * bytes 4-7 --> UTC seconds since 1970 (u32), 0 = not known (D5, RTC/UTC TBD)
 * higher priority than FLIGHT_PHASE so it waits less on a busy bus --> the
 * delay on the time stamp stays small and steady */
#define CAN_ID_TIME_SYNC 0x080u
#define CAN_DLC_TIME_SYNC 8u

/* 0x100 FLIGHT_PHASE: OBC --> everyone, 10 Hz, 6 bytes
 * byte 0  --> flight phase (enum flight_phase below)
 * byte 1  --> sequence counter (u8, +1 every message, 255 wraps to 0)
 * bytes 2-5 --> OBC mission time in ms when the message was made (u32) */
#define CAN_ID_FLIGHT_PHASE   0x100u
#define CAN_DLC_FLIGHT_PHASE  6u

enum flight_phase { /* DRAFT, follows the payload state machine */
    PHASE_PAD = 0, /*on the pad, safe ⸜( ˶>ᴗ<˶)⸝♡ */
    PHASE_ARMED = 1,
    PHASE_BOOST = 2,
    PHASE_COAST = 3,
    PHASE_APOGEE = 4,
    PHASE_DESCENT = 5,
    PHASE_LANDED = 6,
};

static inline const char *phase_name(uint8_t p)
{
    static const char *const names[] = {
        "PAD", "ARMED", "BOOST", "COAST", "APOGEE", "DESCENT", "LANDED"
    };
    return p < 7u ? names[p] : "UNKNOWN";
}

/* little endian, byte 0 is the lowest */
static inline void put_u32(uint8_t *d, uint32_t v)
{
    d[0] = (uint8_t)v;
    d[1] = (uint8_t)(v >> 8);
    d[2] = (uint8_t)(v >> 16);
    d[3] = (uint8_t)(v >> 24);
}

static inline uint32_t get_u32(const uint8_t *d)
{
    return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

#endif
