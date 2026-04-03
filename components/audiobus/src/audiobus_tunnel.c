/*
 * AudioBus — Tunnel subsystem (SPI, I2C, GPIO, MIDI multiplexing)
 *
 * Tunnels pack control/data traffic into the aux region of each frame.
 * Each tunnel has a fixed bandwidth allocation from the slot map.
 *
 * Packet format: [TYPE:4|NODE:4][LEN:8][PAYLOAD...]
 * GPIO: fixed 2 bytes per node (no header, direct slot access)
 * MIDI: up to 3 bytes per frame per node (status + data1 + data2)
 *
 * SPDX-License-Identifier: MIT
 */

#include "audiobus_tunnel.h"
#include "audiobus_types.h"
#include "esp_log.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "abus_tunnel";

/* ---------------------------------------------------------------------------
 * Tunnel context
 * --------------------------------------------------------------------------- */

#define TUNNEL_QUEUE_DEPTH  16

typedef struct {
    uint8_t  type_node;     /* [7:4]=type, [3:0]=node_id */
    uint8_t  len;
    uint8_t  data[ABUS_TUNNEL_MAX_PAYLOAD];
} tunnel_queued_pkt_t;

struct abus_tunnel_ctx {
    /* TX queue: packets waiting to be packed into frames */
    tunnel_queued_pkt_t tx_queue[TUNNEL_QUEUE_DEPTH];
    int tx_queue_head;
    int tx_queue_tail;
    int tx_queue_count;

    /* GPIO state cache per node (both directions) */
    uint16_t gpio_state_dn[ABUS_MAX_NODES];  /* Master → node */
    uint16_t gpio_state_up[ABUS_MAX_NODES];  /* Node → master */

    /* MIDI TX queue per node */
    uint8_t  midi_tx[ABUS_MAX_NODES][3];
    uint8_t  midi_tx_len[ABUS_MAX_NODES];

    /* RX callback */
    abus_tunnel_cb_t rx_cb;
    void *rx_cb_ctx;
};

/* ---------------------------------------------------------------------------
 * Init / deinit
 * --------------------------------------------------------------------------- */

esp_err_t abus_tunnel_init(abus_tunnel_ctx_t **out_ctx) {
    abus_tunnel_ctx_t *ctx = calloc(1, sizeof(abus_tunnel_ctx_t));
    if (!ctx) return ESP_ERR_NO_MEM;
    *out_ctx = ctx;
    return ESP_OK;
}

void abus_tunnel_deinit(abus_tunnel_ctx_t *ctx) {
    free(ctx);
}

/* ---------------------------------------------------------------------------
 * Pack: serialize queued tunnel data into frame aux region
 * --------------------------------------------------------------------------- */

int abus_tunnel_pack(abus_tunnel_ctx_t *ctx, uint8_t *aux_buf, uint16_t aux_len,
                     abus_direction_t dir) {
    if (!ctx || !aux_buf || aux_len == 0) return 0;

    int offset = 0;

    /* Pack queued packets that fit */
    while (ctx->tx_queue_count > 0 && offset + ABUS_TUNNEL_HDR_LEN < aux_len) {
        tunnel_queued_pkt_t *pkt = &ctx->tx_queue[ctx->tx_queue_tail];
        int pkt_total = ABUS_TUNNEL_HDR_LEN + pkt->len;
        if (offset + pkt_total > aux_len) break;

        /* Write tunnel header */
        aux_buf[offset + 0] = pkt->type_node;
        aux_buf[offset + 1] = pkt->len;
        if (pkt->len > 0) {
            memcpy(&aux_buf[offset + ABUS_TUNNEL_HDR_LEN], pkt->data, pkt->len);
        }
        offset += pkt_total;

        ctx->tx_queue_tail = (ctx->tx_queue_tail + 1) % TUNNEL_QUEUE_DEPTH;
        ctx->tx_queue_count--;
    }

    /* Zero remaining aux space */
    if (offset < aux_len) {
        memset(&aux_buf[offset], 0, aux_len - offset);
    }

    return offset;
}

/* ---------------------------------------------------------------------------
 * Unpack: deserialize tunnel data from received frame aux region
 * --------------------------------------------------------------------------- */

int abus_tunnel_unpack(abus_tunnel_ctx_t *ctx, const uint8_t *aux_buf,
                       uint16_t aux_len, abus_direction_t dir) {
    if (!ctx || !aux_buf) return 0;

    int offset = 0;
    int count = 0;

    while (offset + ABUS_TUNNEL_HDR_LEN <= aux_len) {
        uint8_t type_node = aux_buf[offset];
        uint8_t pkt_len = aux_buf[offset + 1];

        if (type_node == 0 && pkt_len == 0) break;  /* End of tunnel data */
        if (offset + ABUS_TUNNEL_HDR_LEN + pkt_len > aux_len) break;

        uint8_t tunnel_type = (type_node >> 4) & 0x0F;
        uint8_t node_id = type_node & 0x0F;

        /* Update GPIO state cache */
        if (tunnel_type == ABUS_TUNNEL_GPIO && pkt_len >= 2) {
            uint16_t state = (aux_buf[offset + 2] << 8) | aux_buf[offset + 3];
            if (dir == ABUS_DIR_UPSTREAM) {
                ctx->gpio_state_up[node_id] = state;
            } else {
                ctx->gpio_state_dn[node_id] = state;
            }
        }

        /* Invoke RX callback */
        if (ctx->rx_cb) {
            ctx->rx_cb(node_id, tunnel_type,
                      &aux_buf[offset + ABUS_TUNNEL_HDR_LEN], pkt_len,
                      ctx->rx_cb_ctx);
        }

        offset += ABUS_TUNNEL_HDR_LEN + pkt_len;
        count++;
    }

    return count;
}

/* ---------------------------------------------------------------------------
 * Tunnel send helpers
 * --------------------------------------------------------------------------- */

static esp_err_t queue_tunnel_pkt(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                                   abus_tunnel_type_t type,
                                   const uint8_t *data, uint8_t len) {
    if (ctx->tx_queue_count >= TUNNEL_QUEUE_DEPTH) {
        ESP_LOGW(TAG, "Tunnel TX queue full");
        return ESP_ERR_NO_MEM;
    }

    tunnel_queued_pkt_t *pkt = &ctx->tx_queue[ctx->tx_queue_head];
    pkt->type_node = ((uint8_t)type << 4) | (node_id & 0x0F);
    pkt->len = len;
    if (len > 0 && data) {
        memcpy(pkt->data, data, len);
    }

    ctx->tx_queue_head = (ctx->tx_queue_head + 1) % TUNNEL_QUEUE_DEPTH;
    ctx->tx_queue_count++;
    return ESP_OK;
}

esp_err_t abus_tunnel_spi_xfer(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                               const abus_spi_tunnel_xfer_t *xfer) {
    if (!ctx || !xfer) return ESP_ERR_INVALID_ARG;
    /* Pack SPI transaction: [CS:1][MODE:1][DATA...] */
    uint8_t hdr[2] = { xfer->cs_pin, xfer->mode };
    uint8_t buf[ABUS_TUNNEL_MAX_PAYLOAD];
    int off = 0;
    memcpy(buf, hdr, 2); off += 2;
    if (xfer->tx_data && xfer->len > 0 && off + xfer->len <= ABUS_TUNNEL_MAX_PAYLOAD) {
        memcpy(&buf[off], xfer->tx_data, xfer->len);
        off += xfer->len;
    }
    return queue_tunnel_pkt(ctx, node_id, ABUS_TUNNEL_SPI, buf, off);
}

esp_err_t abus_tunnel_i2c_xfer(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                               const abus_i2c_tunnel_xfer_t *xfer) {
    if (!ctx || !xfer) return ESP_ERR_INVALID_ARG;
    /* Pack I2C transaction: [ADDR:1][FLAGS:1][DATA...] */
    uint8_t buf[ABUS_TUNNEL_MAX_PAYLOAD];
    int off = 0;
    buf[off++] = xfer->addr;
    buf[off++] = xfer->read ? 0x01 : 0x00;
    if (xfer->tx_data && xfer->tx_len > 0 && off + xfer->tx_len <= ABUS_TUNNEL_MAX_PAYLOAD) {
        memcpy(&buf[off], xfer->tx_data, xfer->tx_len);
        off += xfer->tx_len;
    }
    return queue_tunnel_pkt(ctx, node_id, ABUS_TUNNEL_I2C, buf, off);
}

esp_err_t abus_tunnel_gpio_set(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                               uint8_t pin, bool level) {
    if (!ctx || pin >= 16) return ESP_ERR_INVALID_ARG;
    if (level) {
        ctx->gpio_state_dn[node_id] |= (1 << pin);
    } else {
        ctx->gpio_state_dn[node_id] &= ~(1 << pin);
    }
    uint8_t data[2] = {
        (ctx->gpio_state_dn[node_id] >> 8) & 0xFF,
        ctx->gpio_state_dn[node_id] & 0xFF,
    };
    return queue_tunnel_pkt(ctx, node_id, ABUS_TUNNEL_GPIO, data, 2);
}

bool abus_tunnel_gpio_get(abus_tunnel_ctx_t *ctx, uint8_t node_id, uint8_t pin) {
    if (!ctx || pin >= 16) return false;
    return (ctx->gpio_state_up[node_id] >> pin) & 1;
}

esp_err_t abus_tunnel_midi_send(abus_tunnel_ctx_t *ctx, uint8_t node_id,
                                const uint8_t *midi_data, uint8_t len) {
    if (!ctx || !midi_data || len == 0 || len > 3) return ESP_ERR_INVALID_ARG;
    return queue_tunnel_pkt(ctx, node_id, ABUS_TUNNEL_MIDI, midi_data, len);
}
