/*
 * AudioBus — Ethernet Network Transport
 *
 * Plug-and-play networked audio over standard Ethernet. Plug any node into
 * any switch port — no master/slave, no configuration, just audio.
 *
 * Architecture:
 *   - Layer 2 multicast (EtherType 0x88B6) — coexists with IP traffic
 *   - IEEE 1588v2 PTP with ESP32-P4 hardware timestamping (<1µs sync)
 *   - Automatic grandmaster election (best clock wins, transparent failover)
 *   - Talker/listener model: any node can publish or subscribe to streams
 *   - Dynamic channel counts: each stream declares its own format
 *   - Configurable packet interval: 125µs (6 samples) to 4ms (192 samples)
 *   - Presentation timestamps: receivers buffer to a common playout point
 *     → deterministic latency, zero jitter regardless of switch hops
 *
 * Network friendliness:
 *   - Uses dedicated multicast group (no broadcast storms)
 *   - DSCP/PCP priority marking for QoS-aware switches
 *   - VLAN tagging support (802.1Q) for traffic isolation
 *   - Bandwidth self-policing: each stream declares its rate
 *   - Works alongside normal IP traffic on the same switch
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "audiobus_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------- */

/* Layer 2 EtherType — IEEE 802 local experimental (won't collide with IP/ARP) */
#define ABUS_ETH_ETHERTYPE      0x88B6

/* Multicast MAC prefix: 01:60:AB:xx:xx:xx (locally administered) */
#define ABUS_ETH_MCAST_PREFIX   { 0x01, 0x60, 0xAB }

/* Multicast groups */
#define ABUS_ETH_MCAST_DISCOVERY { 0x01, 0x60, 0xAB, 0xFF, 0xFF, 0x00 }
#define ABUS_ETH_MCAST_PTP       { 0x01, 0x60, 0xAB, 0xFF, 0xFF, 0x01 }
/* Audio streams use 01:60:AB:SS:SS:xx where SS:SS = stream_id */

/* Maximum streams a single node can publish or subscribe to */
#define ABUS_NET_MAX_STREAMS     16

/* Maximum channels per stream */
#define ABUS_NET_MAX_STREAM_CH   64

/* Maximum nodes on the network */
#define ABUS_NET_MAX_NODES       64

/* Discovery beacon interval (ms) */
#define ABUS_NET_BEACON_INTERVAL_MS  1000

/* Default presentation latency (µs) — how far ahead of "now" to schedule playout */
#define ABUS_NET_DEFAULT_LATENCY_US  2000

/* PTP sync interval: 8 per second (125ms) */
#define ABUS_NET_PTP_SYNC_INTERVAL_MS  125

/* ---------------------------------------------------------------------------
 * Packet types
 * --------------------------------------------------------------------------- */

typedef enum {
    ABUS_NET_PKT_AUDIO      = 0x01,     /* Audio sample data */
    ABUS_NET_PKT_BEACON     = 0x02,     /* Node discovery beacon */
    ABUS_NET_PKT_SUBSCRIBE  = 0x03,     /* Listener subscribes to a stream */
    ABUS_NET_PKT_UNSUBSCRIBE = 0x04,    /* Listener unsubscribes */
    ABUS_NET_PKT_STREAM_ANN = 0x05,     /* Talker announces a stream */
    ABUS_NET_PKT_STREAM_DEL = 0x06,     /* Talker removes a stream */
    ABUS_NET_PKT_PTP_SYNC   = 0x10,     /* PTP Sync */
    ABUS_NET_PKT_PTP_FOLLOWUP = 0x11,   /* PTP Follow_Up (carries HW timestamp) */
    ABUS_NET_PKT_PTP_DELAY_REQ = 0x12,  /* PTP Delay_Req */
    ABUS_NET_PKT_PTP_DELAY_RESP = 0x13, /* PTP Delay_Resp */
    ABUS_NET_PKT_TUNNEL     = 0x20,     /* SPI/I2C/GPIO/MIDI tunnel */
    ABUS_NET_PKT_METADATA   = 0x30,     /* Stream metadata (names, labels, etc.) */
} abus_net_pkt_type_t;

/* ---------------------------------------------------------------------------
 * Wire format — common header for all AudioBus Ethernet packets
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint8_t  version;           /* Protocol version (1) */
    uint8_t  pkt_type;          /* abus_net_pkt_type_t */
    uint16_t flags;             /* Bit 0: VLAN present, 1-3: reserved, 4-7: DSCP */
    uint32_t source_uid;        /* Sender's unique node ID */
    uint16_t seq;               /* Per-source sequence number */
    uint16_t length;            /* Payload length after this header */
} abus_net_header_t;

#define ABUS_NET_HEADER_LEN     12

/* ---------------------------------------------------------------------------
 * Audio packet payload
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint16_t stream_id;         /* Unique stream identifier */
    uint8_t  channels;          /* Number of channels in this packet */
    uint8_t  bit_depth;         /* 16, 24, or 32 */
    uint16_t sample_rate_hz_hi; /* Sample rate, big-endian (high 16 bits) */
    uint16_t sample_rate_hz_lo; /* Sample rate, big-endian (low 16 bits) */
    uint16_t samples_per_ch;    /* Number of sample frames in this packet */
    uint16_t reserved;
    int64_t  presentation_ts;   /* PTP timestamp (ns): when to start playout */
    /* Followed by: channels × samples_per_ch × (bit_depth/8) bytes of audio */
} abus_net_audio_payload_t;

#define ABUS_NET_AUDIO_HDR_LEN  20

/* ---------------------------------------------------------------------------
 * Discovery beacon payload
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint32_t uid;               /* Node unique ID */
    uint8_t  name_len;          /* Length of node name string */
    uint8_t  hw_type;           /* 0=generic, 1=speaker, 2=mic, etc. */
    uint8_t  num_talker_streams;  /* How many streams this node publishes */
    uint8_t  num_listener_slots;  /* How many streams this node can subscribe to */
    uint8_t  ptp_priority;      /* PTP clock priority (lower = better) */
    uint8_t  ptp_clock_class;   /* PTP clock class (6=PTP, 248=default, etc.) */
    uint16_t flags;             /* Bit 0: is current grandmaster */
    /* Followed by: name string (name_len bytes, UTF-8, no null terminator) */
} abus_net_beacon_payload_t;

/* ---------------------------------------------------------------------------
 * Stream announcement — talker describes a stream it publishes
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint16_t stream_id;
    uint8_t  channels;          /* Channel count (dynamic — can change between announcements) */
    uint8_t  bit_depth;
    uint32_t sample_rate;
    uint16_t packet_interval_us; /* Packet interval in microseconds */
    uint8_t  encoding;          /* 0=PCM, 1=DSD (future) */
    uint8_t  name_len;          /* Stream name length */
    /* Followed by:
     *   - name string (name_len bytes, UTF-8)
     *   - channel labels: channels × label_entry_t */
} abus_net_stream_ann_t;

/* Per-channel label (for stream announcements) */
typedef struct __attribute__((packed)) {
    uint8_t  label_len;         /* 0 = unnamed */
    /* Followed by label string (label_len bytes, UTF-8) */
} abus_net_channel_label_t;

/* ---------------------------------------------------------------------------
 * Subscription request — listener subscribes to a talker's stream
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint16_t stream_id;         /* Stream to subscribe to */
    uint32_t talker_uid;        /* UID of the talker node */
    uint8_t  channel_mask_len;  /* Length of channel bitmask in bytes (0 = all channels) */
    /* Followed by: channel bitmask (bit N = subscribe to channel N) */
} abus_net_subscribe_t;

/* ---------------------------------------------------------------------------
 * Tunnel packet (SPI/I2C/GPIO/MIDI over Ethernet)
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint32_t target_uid;        /* Destination node UID (0 = broadcast) */
    uint8_t  tunnel_type;       /* abus_tunnel_type_t */
    uint8_t  tunnel_len;        /* Payload length */
    /* Followed by tunnel payload */
} abus_net_tunnel_payload_t;

/* ---------------------------------------------------------------------------
 * Metadata packet — key/value pairs for stream or node metadata
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint16_t stream_id;         /* 0 = node-level metadata */
    uint8_t  num_entries;
    /* Followed by: num_entries × { key_len(1), val_len(2), key(key_len), value(val_len) } */
} abus_net_metadata_t;

/* ---------------------------------------------------------------------------
 * PTP packets
 * --------------------------------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint16_t ptp_seq;
    int64_t  origin_timestamp;  /* Nanoseconds since epoch (from HW timestamp) */
} abus_net_ptp_sync_t;

typedef struct __attribute__((packed)) {
    uint16_t ptp_seq;
    int64_t  precise_origin_ts; /* Exact HW timestamp of the Sync packet departure */
} abus_net_ptp_followup_t;

typedef struct __attribute__((packed)) {
    uint16_t ptp_seq;
    int64_t  receive_timestamp; /* When Delay_Req was received (HW timestamp) */
    uint32_t requester_uid;
} abus_net_ptp_delay_resp_t;

/* ---------------------------------------------------------------------------
 * Runtime types
 * --------------------------------------------------------------------------- */

/* Stream descriptor (runtime, not wire format) */
typedef struct {
    uint16_t    stream_id;
    uint32_t    talker_uid;         /* Who publishes this stream */
    uint8_t     channels;
    uint8_t     bit_depth;
    uint32_t    sample_rate;
    uint16_t    packet_interval_us;
    char        name[32];           /* Stream name */
    char        channel_labels[ABUS_NET_MAX_STREAM_CH][16];
    bool        active;
} abus_net_stream_t;

/* Node descriptor (runtime) */
typedef struct {
    uint32_t    uid;
    char        name[32];
    uint8_t     hw_type;
    uint8_t     ptp_priority;
    uint8_t     ptp_clock_class;
    bool        is_grandmaster;
    int64_t     last_beacon_time;   /* Local time of last beacon received */
    uint8_t     num_streams;
    uint16_t    stream_ids[ABUS_NET_MAX_STREAMS];
} abus_net_node_t;

/* PTP clock state */
typedef struct {
    bool        is_grandmaster;
    uint32_t    grandmaster_uid;
    int64_t     offset_ns;          /* Our clock offset from grandmaster */
    int64_t     path_delay_ns;      /* One-way network delay to grandmaster */
    int64_t     last_sync_time;
    double      freq_ratio;         /* Clock frequency ratio (for drift compensation) */
    uint32_t    sync_count;
} abus_net_ptp_state_t;

/* ---------------------------------------------------------------------------
 * Ethernet transport configuration
 * --------------------------------------------------------------------------- */

typedef struct {
    /* Node identity */
    uint32_t    uid;                /* Unique node ID (0 = auto from MAC) */
    char        name[32];           /* Human-readable node name */
    uint8_t     hw_type;            /* Hardware type (speaker, mic, bridge, etc.) */

    /* PTP clock configuration */
    uint8_t     ptp_priority;       /* Lower = more likely to become grandmaster (1-255) */
    uint8_t     ptp_clock_class;    /* 6=PTP-locked, 13=app-specific, 248=default */

    /* Audio defaults */
    uint32_t    default_sample_rate;
    uint8_t     default_bit_depth;
    uint16_t    default_packet_interval_us;  /* 125, 250, 500, 1000, 2000, 4000 */

    /* Playout buffer */
    uint32_t    presentation_latency_us;     /* Target playout latency (default: 2000) */

    /* Network */
    uint16_t    vlan_id;            /* 0 = no VLAN tagging */
    uint8_t     dscp;               /* DiffServ for QoS (default: 46 = EF) */

    /* Ethernet (ESP32-P4 EMAC pins are mostly fixed, so no pin config needed) */
    /* Optional: external PHY address for MDIO */
    uint8_t     phy_addr;           /* Default: 0 */

    /* Callbacks */
    void (*on_node_discovered)(const abus_net_node_t *node, void *ctx);
    void (*on_node_lost)(uint32_t uid, void *ctx);
    void (*on_stream_announced)(const abus_net_stream_t *stream, void *ctx);
    void (*on_stream_removed)(uint16_t stream_id, uint32_t talker_uid, void *ctx);
    void *cb_ctx;
} abus_net_config_t;

/* Opaque handle */
typedef struct abus_net_handle *abus_net_handle_t;

/* ---------------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------------- */

/** Initialize Ethernet transport. Configures EMAC, starts PTP, begins discovery. */
esp_err_t abus_net_init(const abus_net_config_t *config, abus_net_handle_t *out);

/** Start the transport (enable EMAC, begin beaconing). */
esp_err_t abus_net_start(abus_net_handle_t h);

/** Stop the transport. */
esp_err_t abus_net_stop(abus_net_handle_t h);

/** Deinitialize and free all resources. */
esp_err_t abus_net_deinit(abus_net_handle_t h);

/* ---------------------------------------------------------------------------
 * Talker API — publish audio streams
 * --------------------------------------------------------------------------- */

/**
 * Create a new audio stream (this node becomes the talker).
 * The stream is announced to the network and listeners can subscribe.
 *
 * @param name          Human-readable stream name (e.g., "Main L/R")
 * @param channels      Number of audio channels
 * @param sample_rate   Sample rate in Hz
 * @param bit_depth     16, 24, or 32
 * @param interval_us   Packet interval in µs (0 = use config default)
 * @param out_stream_id Returns the assigned stream ID
 */
esp_err_t abus_net_stream_create(abus_net_handle_t h,
                                  const char *name,
                                  uint8_t channels,
                                  uint32_t sample_rate,
                                  uint8_t bit_depth,
                                  uint16_t interval_us,
                                  uint16_t *out_stream_id);

/**
 * Set channel labels for a stream (optional, for display purposes).
 * @param labels    Array of null-terminated strings, one per channel.
 */
esp_err_t abus_net_stream_set_labels(abus_net_handle_t h, uint16_t stream_id,
                                      const char **labels, uint8_t num_labels);

/**
 * Dynamically change the channel count of a published stream.
 * Listeners are notified via a new stream announcement.
 */
esp_err_t abus_net_stream_set_channels(abus_net_handle_t h, uint16_t stream_id,
                                        uint8_t new_channel_count);

/**
 * Write audio samples to a published stream.
 * Samples are buffered and sent at the configured packet interval.
 *
 * @param samples   Interleaved 32-bit samples (always 32-bit internally)
 * @param frames    Number of sample frames
 * @return Number of frames actually written (0 if buffer full)
 */
int abus_net_stream_write(abus_net_handle_t h, uint16_t stream_id,
                          const int32_t *samples, int frames);

/** Remove a published stream. Listeners are notified. */
esp_err_t abus_net_stream_destroy(abus_net_handle_t h, uint16_t stream_id);

/* ---------------------------------------------------------------------------
 * Listener API — subscribe to remote streams
 * --------------------------------------------------------------------------- */

/**
 * Subscribe to a remote stream. Audio becomes available via abus_net_stream_read().
 *
 * @param talker_uid   UID of the talker node
 * @param stream_id    Stream ID to subscribe to
 * @param channel_mask Bitmask of channels to receive (0 = all channels)
 */
esp_err_t abus_net_subscribe(abus_net_handle_t h, uint32_t talker_uid,
                              uint16_t stream_id, uint64_t channel_mask);

/** Unsubscribe from a remote stream. */
esp_err_t abus_net_unsubscribe(abus_net_handle_t h, uint32_t talker_uid,
                                uint16_t stream_id);

/**
 * Read audio samples from a subscribed stream.
 * Samples are presented at the correct PTP-synchronized playout time.
 *
 * @param stream_id  The stream to read from
 * @param samples    Output buffer (interleaved 32-bit)
 * @param frames     Maximum frames to read
 * @return Number of frames read (0 if no data available yet)
 */
int abus_net_stream_read(abus_net_handle_t h, uint16_t stream_id,
                         int32_t *samples, int frames);

/* ---------------------------------------------------------------------------
 * Tunnel API (over Ethernet)
 * --------------------------------------------------------------------------- */

/** Send tunnel data to a specific node (or broadcast with uid=0). */
esp_err_t abus_net_tunnel_send(abus_net_handle_t h, uint32_t target_uid,
                                abus_tunnel_type_t type,
                                const uint8_t *data, uint16_t len);

/** Register tunnel receive callback. */
esp_err_t abus_net_tunnel_register(abus_net_handle_t h,
                                    void (*cb)(uint32_t sender_uid,
                                              abus_tunnel_type_t type,
                                              const uint8_t *data, uint16_t len,
                                              void *ctx),
                                    void *ctx);

/* ---------------------------------------------------------------------------
 * Tunnel convenience API — GPIO, MIDI, SPI, I2C over Ethernet
 *
 * These wrap abus_net_tunnel_send() with structured payloads matching the
 * bus-mode tunnel format, so both transports are interchangeable at the app level.
 * --------------------------------------------------------------------------- */

/**
 * Set a GPIO pin on a remote node.
 * State is cached locally and sent as a 16-bit bitmask per node.
 * @param target_uid   Remote node UID
 * @param pin          GPIO pin number (0-15)
 * @param level        true=HIGH, false=LOW
 */
esp_err_t abus_net_gpio_set(abus_net_handle_t h, uint32_t target_uid,
                             uint8_t pin, bool level);

/** Read last-known GPIO state of a remote node (from received tunnel data). */
bool abus_net_gpio_get(abus_net_handle_t h, uint32_t node_uid, uint8_t pin);

/**
 * Send MIDI message to a specific node (or broadcast with uid=0).
 * @param data   Raw MIDI bytes (1-3 bytes: status [+ data1 [+ data2]])
 * @param len    1, 2, or 3
 */
esp_err_t abus_net_midi_send(abus_net_handle_t h, uint32_t target_uid,
                              const uint8_t *data, uint8_t len);

/** Register MIDI receive callback (separate from raw tunnel callback). */
esp_err_t abus_net_midi_register(abus_net_handle_t h,
                                  void (*cb)(uint32_t sender_uid,
                                            const uint8_t *data, uint8_t len,
                                            void *ctx),
                                  void *ctx);

/**
 * Execute an SPI transaction on a remote node.
 * The transaction is serialized into a tunnel packet.
 * Response (if any) arrives via the tunnel callback.
 *
 * @param target_uid   Remote node
 * @param cs_pin       Which CS to assert on the remote node
 * @param mode         SPI mode 0-3
 * @param tx_data      Data to send
 * @param len          Transaction length
 */
esp_err_t abus_net_spi_xfer(abus_net_handle_t h, uint32_t target_uid,
                             uint8_t cs_pin, uint8_t mode,
                             const uint8_t *tx_data, uint16_t len);

/**
 * Execute an I2C transaction on a remote node.
 *
 * @param target_uid   Remote node
 * @param addr         7-bit I2C address
 * @param is_read      true=read, false=write
 * @param tx_data      Data to write (or register address for read)
 * @param tx_len       Write data length
 */
esp_err_t abus_net_i2c_xfer(abus_net_handle_t h, uint32_t target_uid,
                             uint8_t addr, bool is_read,
                             const uint8_t *tx_data, uint16_t tx_len);

/* ---------------------------------------------------------------------------
 * Metadata API
 * --------------------------------------------------------------------------- */

/** Set node-level metadata (key/value, both UTF-8). */
esp_err_t abus_net_set_metadata(abus_net_handle_t h,
                                 const char *key, const char *value);

/** Set stream-level metadata. */
esp_err_t abus_net_stream_set_metadata(abus_net_handle_t h, uint16_t stream_id,
                                        const char *key, const char *value);

/* ---------------------------------------------------------------------------
 * Status / diagnostics
 * --------------------------------------------------------------------------- */

/** Get PTP clock state. */
esp_err_t abus_net_get_ptp_state(abus_net_handle_t h, abus_net_ptp_state_t *out);

/** Get list of discovered nodes. */
int abus_net_get_nodes(abus_net_handle_t h, abus_net_node_t *out, int max_nodes);

/** Get list of all announced streams on the network. */
int abus_net_get_streams(abus_net_handle_t h, abus_net_stream_t *out, int max_streams);

typedef struct {
    uint32_t    pkts_tx;
    uint32_t    pkts_rx;
    uint32_t    audio_pkts_tx;
    uint32_t    audio_pkts_rx;
    uint32_t    ptp_syncs;
    uint32_t    seq_errors;         /* Out-of-order or missing packets */
    uint32_t    late_packets;       /* Arrived after presentation time */
    int64_t     ptp_offset_ns;      /* Current PTP offset from grandmaster */
    int64_t     ptp_path_delay_ns;
    uint32_t    jitter_ns;          /* Measured packet arrival jitter (RMS) */
} abus_net_stats_t;

esp_err_t abus_net_get_stats(abus_net_handle_t h, abus_net_stats_t *out);

#ifdef __cplusplus
}
#endif
