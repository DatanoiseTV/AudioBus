/*
 * AudioBus — Tunnel subsystem (SPI, I2C, GPIO, MIDI over the bus)
 *
 * Tunnels multiplex control/data traffic into the aux region of each frame.
 * Each tunnel has a fixed bandwidth allocation determined at configuration time.
 *
 * Tunnel packet format within the aux region:
 *   [TYPE:4][NODE:4][LEN:8][PAYLOAD:LEN bytes]
 *
 * GPIO tunnel is special: 2 bytes carry 16 GPIO states (no header needed,
 * fixed position in aux region per node).
 *
 * MIDI tunnel carries raw MIDI bytes, 1-3 bytes per frame (enough for
 * real-time MIDI at any sample rate — 48000 frames/sec >> 31250 baud MIDI).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "audiobus_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tunnel packet header (2 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t type_node;      /* [7:4] = tunnel_type, [3:0] = node_id */
    uint8_t length;         /* Payload length in bytes (0..255) */
} abus_tunnel_header_t;

#define ABUS_TUNNEL_HDR_LEN     2
#define ABUS_TUNNEL_MAX_PAYLOAD 253     /* 255 - 2 header bytes */

/* GPIO tunnel state (2 bytes per node) */
typedef struct __attribute__((packed)) {
    uint16_t pin_states;    /* Bit n = GPIO n state (up to 16 GPIOs per node) */
} abus_gpio_state_t;

/* SPI tunnel transaction */
typedef struct {
    uint8_t  cs_pin;        /* Which CS to assert on the remote node */
    uint8_t  mode;          /* SPI mode 0-3 */
    uint32_t clock_hz;      /* Desired SPI clock (best-effort) */
    const uint8_t *tx_data;
    uint8_t *rx_data;
    uint16_t len;
} abus_spi_tunnel_xfer_t;

/* I2C tunnel transaction */
typedef struct {
    uint8_t  addr;          /* 7-bit I2C address */
    bool     read;          /* true = read, false = write */
    const uint8_t *tx_data; /* Write data (or register address for read) */
    uint16_t tx_len;
    uint8_t *rx_data;       /* Read buffer */
    uint16_t rx_len;
} abus_i2c_tunnel_xfer_t;

/* Tunnel context (internal) */
typedef struct abus_tunnel_ctx abus_tunnel_ctx_t;

/* Create/destroy tunnel context */
esp_err_t abus_tunnel_init(abus_tunnel_ctx_t **out_ctx);
void      abus_tunnel_deinit(abus_tunnel_ctx_t *ctx);

/* Pack tunnel data into frame aux region */
int abus_tunnel_pack(abus_tunnel_ctx_t *ctx,
                     uint8_t *aux_buf, uint16_t aux_len,
                     abus_direction_t dir);

/* Unpack tunnel data from received frame aux region */
int abus_tunnel_unpack(abus_tunnel_ctx_t *ctx,
                       const uint8_t *aux_buf, uint16_t aux_len,
                       abus_direction_t dir);

/* Queue a SPI transaction for tunneling */
esp_err_t abus_tunnel_spi_xfer(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                               const abus_spi_tunnel_xfer_t *xfer);

/* Queue an I2C transaction for tunneling */
esp_err_t abus_tunnel_i2c_xfer(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                               const abus_i2c_tunnel_xfer_t *xfer);

/* Set GPIO pin state for tunneling */
esp_err_t abus_tunnel_gpio_set(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                               uint8_t pin, bool level);

/* Get remote GPIO pin state */
bool abus_tunnel_gpio_get(abus_tunnel_ctx_t *ctx, uint8_t node_id, uint8_t pin);

/* Queue MIDI bytes for tunneling */
esp_err_t abus_tunnel_midi_send(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                                const uint8_t *midi_data, uint8_t len);

#ifdef __cplusplus
}
#endif
