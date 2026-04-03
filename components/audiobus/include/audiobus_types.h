/*
 * AudioBus - Multiplexed Audio/IO Protocol over Single Twisted Pair
 * Type definitions and protocol constants
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Physical layer constants
 *
 * LVDS SerDes PHY (DS92LV1021A / DS92LV1212A):
 *   Parallel clock:  49.152 MHz (48kHz family) or 45.1584 MHz (44.1kHz family)
 *   Symbol width:    10 bits (8b10b encoded)
 *   Line rate:       491.52 Mbps / 451.584 Mbps
 *   Effective data:  393.216 Mbps / 361.267 Mbps
 *   Single twisted pair, half-duplex with direction switching
 *   Self-clocking via DS92LV1212A CDR PLL (no separate clock wire needed)
 *   ESP32-P4 EMAC remains free for normal Ethernet/WiFi networking
 * --------------------------------------------------------------------------- */

/* Clock frequencies for LVDS SerDes PHY (parallel interface to serializer) */
#define ABUS_MCLK_48K       49152000    /* 48kHz family master clock */
#define ABUS_MCLK_44K       45158400    /* 44.1kHz family master clock */

/* Symbols per frame = MCLK / Fs */
#define ABUS_SYMBOLS_48K     1024       /* 49152000 / 48000 */
#define ABUS_SYMBOLS_96K      512       /* 49152000 / 96000 */
#define ABUS_SYMBOLS_44K     1024       /* 45158400 / 44100 */
#define ABUS_SYMBOLS_88K      512       /* 45158400 / 88200 */

/* Each 10-bit symbol carries 8 data bits (8b10b), so bytes == symbols */
#define ABUS_FRAME_BYTES_48K  1024
#define ABUS_FRAME_BYTES_96K   512
#define ABUS_FRAME_BYTES_MAX  1024

/* Frame structure offsets (byte positions) */
#define ABUS_SYNC_OFFSET        0
#define ABUS_SYNC_LEN           2       /* K28.5 + K28.1 (SOF comma pair) */
#define ABUS_HEADER_OFFSET      2
#define ABUS_HEADER_LEN         8
#define ABUS_PAYLOAD_OFFSET    10       /* Audio + aux data starts here */
#define ABUS_CRC_LEN            4       /* CRC-32 at frame end */
#define ABUS_GUARD_LEN          2       /* K28.5 + K27.7 direction turnaround */
#define ABUS_OVERHEAD          (ABUS_SYNC_LEN + ABUS_HEADER_LEN + \
                                ABUS_GUARD_LEN + ABUS_CRC_LEN)  /* = 16 bytes */

/* Maximum audio channels per direction */
#define ABUS_MAX_CHANNELS       64
#define ABUS_MAX_CHANNELS_TOTAL 128     /* 64 downstream + 64 upstream */
#define ABUS_MAX_NODES          16
#define ABUS_MAX_TUNNEL_SLOTS   32

/* Aux data byte for MIDI / sideband (1 byte per frame per direction) */
#define ABUS_AUX_SIDEBAND_BYTES  1

/* ---------------------------------------------------------------------------
 * Enumerations
 * --------------------------------------------------------------------------- */

typedef enum {
    ABUS_ROLE_MASTER = 0,
    ABUS_ROLE_SLAVE  = 1,
} abus_role_t;

typedef enum {
    ABUS_PHY_LVDS_SERDES    = 0,    /* DS92LV1021A/1212A via PARLIO (recommended) */
} abus_phy_type_t;

typedef enum {
    ABUS_SR_44100  = 44100,
    ABUS_SR_48000  = 48000,
    ABUS_SR_88200  = 88200,
    ABUS_SR_96000  = 96000,
} abus_sample_rate_t;

typedef enum {
    ABUS_DEPTH_16 = 16,
    ABUS_DEPTH_24 = 24,
    ABUS_DEPTH_32 = 32,
} abus_bit_depth_t;

typedef enum {
    ABUS_DIR_DOWNSTREAM = 0,    /* Master → Slaves */
    ABUS_DIR_UPSTREAM   = 1,    /* Slaves → Master */
} abus_direction_t;

typedef enum {
    ABUS_TUNNEL_SPI   = 0,
    ABUS_TUNNEL_I2C   = 1,
    ABUS_TUNNEL_GPIO  = 2,
    ABUS_TUNNEL_MIDI  = 3,
    ABUS_TUNNEL_SIDEBAND = 4,   /* Generic 1-bit-per-frame sideband */
} abus_tunnel_type_t;

typedef enum {
    ABUS_STATE_RESET       = 0,
    ABUS_STATE_INIT        = 1,
    ABUS_STATE_DISCOVERY   = 2,
    ABUS_STATE_CONFIG      = 3,
    ABUS_STATE_RUNNING     = 4,
    ABUS_STATE_ERROR       = 5,
} abus_state_t;

typedef enum {
    ABUS_FRAME_NORMAL    = 0x00,
    ABUS_FRAME_DISCOVERY = 0x01,
    ABUS_FRAME_CONFIG    = 0x02,
    ABUS_FRAME_STATUS    = 0x03,
} abus_frame_type_t;

typedef enum {
    ABUS_EVT_NODE_DISCOVERED    = 0,
    ABUS_EVT_NODE_LOST          = 1,
    ABUS_EVT_LINK_UP            = 2,
    ABUS_EVT_LINK_DOWN          = 3,
    ABUS_EVT_CONFIG_COMPLETE    = 4,
    ABUS_EVT_FRAME_ERROR        = 5,
    ABUS_EVT_TUNNEL_DATA        = 6,
} abus_event_type_t;

/* ---------------------------------------------------------------------------
 * Frame header (8 bytes, packed)
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint16_t frame_counter;         /* Monotonic, wraps at 65536 */
    uint8_t  frame_type;            /* abus_frame_type_t */
    uint8_t  flags;                 /* Bit 0-1: sample_rate_idx, 2-3: bit_depth_idx,
                                       4: aux_sideband_valid, 5-7: reserved */
    uint8_t  node_count;            /* Active nodes in chain */
    uint8_t  dn_audio_slots;        /* Number of downstream audio slots */
    uint8_t  up_audio_slots;        /* Number of upstream audio slots */
    uint8_t  slotmap_crc;           /* CRC-8 of current slot map for sync check */
} abus_frame_header_t;

_Static_assert(sizeof(abus_frame_header_t) == ABUS_HEADER_LEN,
               "Frame header must be exactly 8 bytes");

/* Flags field encoding */
#define ABUS_FLAG_SR_48K     (0 << 0)
#define ABUS_FLAG_SR_96K     (1 << 0)
#define ABUS_FLAG_SR_44K     (2 << 0)
#define ABUS_FLAG_SR_88K     (3 << 0)
#define ABUS_FLAG_SR_MASK    (3 << 0)

#define ABUS_FLAG_BD_16      (0 << 2)
#define ABUS_FLAG_BD_24      (1 << 2)
#define ABUS_FLAG_BD_32      (2 << 2)
#define ABUS_FLAG_BD_MASK    (3 << 2)

#define ABUS_FLAG_SIDEBAND   (1 << 4)

/* ---------------------------------------------------------------------------
 * Slot map — computed at configuration time, fully deterministic
 *
 * The slot map assigns a fixed byte offset within the frame to each audio
 * channel and tunnel stream. Once computed, it never changes during operation.
 * Every node receives the same slot map, so packing/unpacking is lockstep.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint16_t byte_offset;           /* Offset within frame payload */
    uint8_t  byte_width;            /* 2 (16-bit), 3 (24-bit), or 4 (32-bit) */
    uint8_t  channel_id;            /* Logical channel index 0..127 */
    uint8_t  node_id;               /* Owning node (0 = master) */
    uint8_t  direction;             /* abus_direction_t */
} abus_audio_slot_t;

typedef struct {
    uint16_t byte_offset;           /* Offset within frame payload */
    uint16_t byte_width;            /* Allocated bandwidth in bytes/frame */
    uint8_t  tunnel_type;           /* abus_tunnel_type_t */
    uint8_t  node_id;
    uint8_t  direction;
} abus_tunnel_slot_t;

typedef struct {
    /* Frame geometry */
    uint16_t frame_bytes;           /* Total frame size (1024 or 512) */
    uint16_t payload_bytes;         /* frame_bytes - ABUS_OVERHEAD */

    /* Downstream audio region */
    uint16_t dn_audio_offset;       /* First byte of DN audio in frame */
    uint16_t dn_audio_bytes;        /* Total DN audio bytes */

    /* Downstream tunnel/aux region */
    uint16_t dn_aux_offset;
    uint16_t dn_aux_bytes;

    /* Upstream audio region (after guard) */
    uint16_t up_audio_offset;
    uint16_t up_audio_bytes;

    /* Upstream tunnel/aux region */
    uint16_t up_aux_offset;
    uint16_t up_aux_bytes;

    /* Sideband (1 bit per direction per frame, packed into the sideband byte) */
    uint16_t dn_sideband_offset;
    uint16_t up_sideband_offset;

    /* Slot arrays */
    abus_audio_slot_t  audio_slots[ABUS_MAX_CHANNELS_TOTAL];
    uint8_t            num_audio_slots;

    abus_tunnel_slot_t tunnel_slots[ABUS_MAX_TUNNEL_SLOTS];
    uint8_t            num_tunnel_slots;

    /* CRC-8 of this slot map (for config verification across nodes) */
    uint8_t            slotmap_crc;
} abus_slotmap_t;

/* ---------------------------------------------------------------------------
 * Node descriptor — what each node advertises during discovery
 * --------------------------------------------------------------------------- */

typedef struct {
    uint8_t  node_id;               /* Assigned during discovery (0 = master) */
    uint8_t  hw_type;               /* 0=generic, 1=speaker, 2=mic, 3=speaker+mic,
                                       4=analog_bridge, 5=digital_bridge */
    uint8_t  max_dn_channels;       /* How many downstream channels this node wants */
    uint8_t  max_up_channels;       /* How many upstream channels this node can source */
    uint8_t  preferred_depth;       /* Preferred bit depth */
    uint8_t  tunnel_request;        /* Bitmask: bit0=SPI, bit1=I2C, bit2=GPIO, bit3=MIDI */
    uint16_t tunnel_bw_request;     /* Total tunnel bytes/frame requested */
    uint32_t uid;                   /* Unique 32-bit hardware ID */
} abus_node_descriptor_t;

/* ---------------------------------------------------------------------------
 * Audio buffer — zero-copy ring buffer for audio exchange with application
 * --------------------------------------------------------------------------- */

typedef struct {
    int32_t *samples;               /* Interleaved sample buffer (always 32-bit internal) */
    uint16_t num_channels;
    uint16_t num_frames;            /* Buffer depth in sample frames */
    volatile uint16_t write_pos;
    volatile uint16_t read_pos;
} abus_audio_buffer_t;

/* ---------------------------------------------------------------------------
 * Event callback
 * --------------------------------------------------------------------------- */

typedef struct {
    abus_event_type_t type;
    uint8_t  node_id;
    union {
        abus_node_descriptor_t discovered_node;
        struct { uint8_t *data; uint16_t len; } tunnel_data;
        uint32_t error_code;
    };
} abus_event_t;

typedef void (*abus_event_cb_t)(const abus_event_t *event, void *user_ctx);

/* ---------------------------------------------------------------------------
 * 8b10b special characters (K-codes) used in framing
 * --------------------------------------------------------------------------- */

#define K28_5   0xBC    /* Comma — used for byte/word alignment */
#define K28_1   0x3C    /* SOF — start of frame marker */
#define K27_7   0xFB    /* Turnaround — direction switch marker */
#define K28_3   0x7C    /* Idle — sent when no data */
#define K29_7   0xFD    /* EOF — end of frame (before CRC) */
#define K23_7   0xF7    /* Discovery beacon */

/* 8b10b encoded symbol — 16-bit to hold 10-bit value + control flag */
typedef struct {
    uint16_t symbol : 10;           /* 10-bit encoded value */
    uint16_t is_k   : 1;           /* 1 = K-character (control), 0 = D-character (data) */
    uint16_t error  : 1;           /* Disparity/decode error */
    uint16_t _pad   : 4;
} abus_symbol_t;

#ifdef __cplusplus
}
#endif
