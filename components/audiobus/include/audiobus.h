/*
 * AudioBus - Multiplexed Audio/IO Protocol over Single Twisted Pair
 * Main public API
 *
 * Multi-channel audio + IO multiplexing over single twisted pair:
 *   - Up to 64 channels of 32-bit audio at 48kHz (62 at 96kHz)
 *   - SPI, I2C, GPIO, and MIDI tunneling
 *   - Daisy-chainable with hot-plug support
 *   - Deterministic, zero-jitter TDM framing
 *   - Self-clocking via 8b10b encoding + DS92LV SerDes CDR
 *   - ESP32 EMAC remains free for normal networking
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
 * Configuration
 * --------------------------------------------------------------------------- */

typedef struct {
    /* PHY pin assignments — LVDS SerDes (PARLIO interface to DS92LV1021A/1212A) */
    struct {
        int8_t upstream_tx_data[10];    /* PARLIO TX data pins [0..9] → DS92LV1021A DIN */
        int8_t upstream_tx_clk;         /* PARLIO TX clock → DS92LV1021A TCLK */
        int8_t upstream_tx_oe;          /* GPIO → DS92LV1021A PDB (output enable) */
        int8_t upstream_rx_data[10];    /* DS92LV1212A DOUT → PARLIO RX data [0..9] */
        int8_t upstream_rx_clk;         /* DS92LV1212A RCLK → PARLIO RX clock */
        int8_t upstream_rx_lock;        /* DS92LV1212A LOCK → GPIO (link detect) */

        int8_t downstream_tx_data[10];
        int8_t downstream_tx_clk;
        int8_t downstream_tx_oe;
        int8_t downstream_rx_data[10];
        int8_t downstream_rx_clk;
        int8_t downstream_rx_lock;
    } lvds_pins;

    /* Reserved for future PHY options */
    uint8_t _reserved[16];
} abus_pin_config_t;

typedef struct {
    abus_role_t         role;
    abus_phy_type_t     phy_type;
    abus_sample_rate_t  sample_rate;
    abus_bit_depth_t    bit_depth;

    abus_pin_config_t   pins;

    /* Master-only: how many nodes to expect (0 = auto-discover) */
    uint8_t             expected_nodes;

    /* Slave-only: node descriptor for discovery */
    abus_node_descriptor_t node_desc;

    /* Audio buffer depth in sample frames (default: 4) */
    uint16_t            audio_buffer_frames;

    /* Event callback */
    abus_event_cb_t     event_cb;
    void               *event_cb_ctx;
} abus_config_t;

/* Opaque handle */
typedef struct abus_handle *abus_handle_t;

/* ---------------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------------- */

/**
 * Initialize the AudioBus engine.
 * Allocates DMA buffers, configures PHY, but does not start the bus.
 */
esp_err_t abus_init(const abus_config_t *config, abus_handle_t *out_handle);

/**
 * Start the bus.
 * Master: begins sending sync frames and starts discovery.
 * Slave: enables receiver and waits for discovery.
 */
esp_err_t abus_start(abus_handle_t handle);

/**
 * Stop the bus gracefully. Audio output mutes, nodes notified.
 */
esp_err_t abus_stop(abus_handle_t handle);

/**
 * Release all resources.
 */
esp_err_t abus_deinit(abus_handle_t handle);

/* ---------------------------------------------------------------------------
 * Audio I/O — zero-copy, ISR-safe
 *
 * Audio is exchanged in 32-bit samples internally, regardless of wire bit depth.
 * The frame packer truncates/pads as needed for the wire format.
 * --------------------------------------------------------------------------- */

/**
 * Write downstream audio samples (master) or upstream samples (slave).
 * @param samples  Interleaved 32-bit samples, num_channels wide
 * @param frames   Number of sample frames to write
 * @return Number of frames actually written (may be less if buffer full)
 */
int abus_audio_write(abus_handle_t handle, const int32_t *samples, int frames);

/**
 * Read received audio samples.
 * Master reads upstream audio; slave reads downstream audio.
 * @param samples  Output buffer for interleaved 32-bit samples
 * @param frames   Maximum frames to read
 * @return Number of frames actually read
 */
int abus_audio_read(abus_handle_t handle, int32_t *samples, int frames);

/**
 * Get current audio buffer fill level (frames).
 */
int abus_audio_available(abus_handle_t handle);

/* ---------------------------------------------------------------------------
 * Tunnel I/O — SPI, I2C, GPIO, MIDI
 * --------------------------------------------------------------------------- */

/**
 * Send tunnel data to a specific node.
 * Data is queued and transmitted in the next available frame aux slots.
 * @param node_id   Target node
 * @param type      Tunnel type
 * @param data      Data bytes
 * @param len       Length (must fit in allocated tunnel bandwidth)
 */
esp_err_t abus_tunnel_send(abus_handle_t handle, uint8_t node_id,
                           abus_tunnel_type_t type,
                           const uint8_t *data, uint16_t len);

/**
 * Register callback for incoming tunnel data.
 */
typedef void (*abus_tunnel_cb_t)(uint8_t node_id, abus_tunnel_type_t type,
                                 const uint8_t *data, uint16_t len, void *ctx);
esp_err_t abus_tunnel_register(abus_handle_t handle, abus_tunnel_cb_t cb, void *ctx);

/* ---------------------------------------------------------------------------
 * Sideband channel — 1 bit per frame per direction (e.g., MIDI clock tick)
 * --------------------------------------------------------------------------- */

/**
 * Set the sideband bit for the next outgoing frame.
 */
esp_err_t abus_sideband_set(abus_handle_t handle, bool value);

/**
 * Get the sideband bit from the last received frame.
 */
bool abus_sideband_get(abus_handle_t handle);

/* ---------------------------------------------------------------------------
 * Configuration / status
 * --------------------------------------------------------------------------- */

/**
 * Get the current slot map (valid after ABUS_STATE_RUNNING).
 */
const abus_slotmap_t *abus_get_slotmap(abus_handle_t handle);

/**
 * Get current bus state.
 */
abus_state_t abus_get_state(abus_handle_t handle);

/**
 * Get number of discovered nodes.
 */
uint8_t abus_get_node_count(abus_handle_t handle);

/**
 * Get descriptor of a discovered node.
 */
esp_err_t abus_get_node_info(abus_handle_t handle, uint8_t node_id,
                             abus_node_descriptor_t *out_desc);

/**
 * Force re-discovery of the bus (master only).
 */
esp_err_t abus_rediscover(abus_handle_t handle);

/**
 * Get bus statistics.
 */
typedef struct {
    uint32_t frames_tx;
    uint32_t frames_rx;
    uint32_t crc_errors;
    uint32_t sync_losses;
    uint32_t buffer_overruns;
    uint32_t buffer_underruns;
    int32_t  clock_offset_ppb;      /* Slave clock offset from master in ppb */
} abus_stats_t;

esp_err_t abus_get_stats(abus_handle_t handle, abus_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
