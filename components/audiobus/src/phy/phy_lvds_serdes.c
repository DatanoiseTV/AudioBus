/*
 * AudioBus — LVDS SerDes PHY driver
 *
 * Physical layer using DS92LV1021A (10:1 serializer) + DS92LV1212A (1:10 deserializer)
 * driven by ESP32-P4 PARLIO peripheral.
 *
 * Wiring per port:
 *   PARLIO TX data[0..9]  → DS92LV1021A DIN[0..9]   (10 GPIO pins)
 *   PARLIO TX clk_out     → DS92LV1021A TCLK         (1 GPIO pin)
 *   GPIO                  → DS92LV1021A PDB           (1 GPIO pin, output enable)
 *   DS92LV1212A DOUT[0..9] → PARLIO RX data[0..9]    (10 GPIO pins)
 *   DS92LV1212A RCLK      → PARLIO RX clk_in          (1 GPIO pin, recovered clock)
 *   DS92LV1212A LOCK      → GPIO input                 (1 GPIO pin, CDR lock status)
 *
 * Clocking:
 *   Master: external 49.152 MHz oscillator → PARLIO TX clk_in_gpio_num
 *   Slave:  DS92LV1212A RCLK (recovered from upstream) → PARLIO TX external clock
 *           This propagates the master's exact clock through the entire chain.
 *
 * Half-duplex on single twisted pair:
 *   DS92LV1021A OE (PDB) controls whether the serializer drives the LVDS pair.
 *   DS92LV1212A is always listening (high-impedance input).
 *   Direction switching: disable local TX OE → remote TX drives → local RX captures.
 *   Guard time: 32 idle symbols (~650ns) for CDR phase reacquisition.
 *
 * Dual-port (intermediate nodes):
 *   Single PARLIO TX/RX shared between upstream and downstream ports via
 *   ESP32-P4 GPIO matrix re-routing. External bus MUX (SN74CB3Q3257) or
 *   GPIO matrix register writes switch the PARLIO signals between port pairs.
 *
 * SPDX-License-Identifier: MIT
 */

#include "audiobus_phy.h"
#include "audiobus_types.h"

#include "driver/parlio_tx.h"
#include "driver/parlio_rx.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

#include <string.h>

static const char *TAG = "abus_phy_lvds";

/* ---------------------------------------------------------------------------
 * 8b10b codec (defined in codec_8b10b.c)
 * --------------------------------------------------------------------------- */

typedef struct { int rd; } abus_8b10b_encoder_t;
typedef struct { int rd; bool comma_detected; int bit_alignment; } abus_8b10b_decoder_t;

extern void     abus_8b10b_encoder_init(abus_8b10b_encoder_t *enc);
extern uint16_t abus_8b10b_encode_data(abus_8b10b_encoder_t *enc, uint8_t byte);
extern uint16_t abus_8b10b_encode_k(abus_8b10b_encoder_t *enc, uint8_t k_byte);
extern void     abus_8b10b_decoder_init(abus_8b10b_decoder_t *dec);
extern int      abus_8b10b_decode_frame(abus_8b10b_decoder_t *dec,
                                         const uint16_t *in_syms, int in_len,
                                         uint8_t *out_data, uint8_t *out_k, int out_cap);

/* ---------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------- */

#define PARLIO_DATA_WIDTH       16      /* Must be power of 2; only [0..9] connected */
#define SERDES_DATA_BITS        10      /* DS92LV1021A/1212A parallel width */
#define SYMBOL_MASK             0x03FF  /* Lower 10 bits of 16-bit PARLIO word */

/* Guard time: idle symbols inserted between TX and RX phases for CDR relock */
#define GUARD_SYMBOLS           32

/* DMA buffer alignment required by ESP32-P4 GDMA */
#define DMA_ALIGN               4
#define DMA_BUF_ATTR            MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL

/* Maximum symbols per frame (48kHz = 1024, 96kHz = 512) + guard */
#define MAX_TX_SYMBOLS          (ABUS_SYMBOLS_48K + 2 * GUARD_SYMBOLS)
#define MAX_TX_BUF_BYTES        (MAX_TX_SYMBOLS * sizeof(uint16_t))
#define MAX_RX_BUF_BYTES        (ABUS_SYMBOLS_48K * sizeof(uint16_t))

/* ---------------------------------------------------------------------------
 * Per-port state
 * --------------------------------------------------------------------------- */

typedef struct {
    /* GPIO assignments */
    int8_t tx_data_pins[SERDES_DATA_BITS];
    int8_t tx_clk_pin;
    int8_t tx_oe_pin;          /* DS92LV1021A PDB (active high = output enabled) */
    int8_t rx_data_pins[SERDES_DATA_BITS];
    int8_t rx_clk_pin;        /* DS92LV1212A RCLK → PARLIO external clock input */
    int8_t rx_lock_pin;       /* DS92LV1212A LOCK output (high = CDR locked) */
} port_pins_t;

/* ---------------------------------------------------------------------------
 * PHY driver context
 * --------------------------------------------------------------------------- */

typedef struct {
    abus_role_t         role;
    abus_sample_rate_t  sample_rate;
    uint16_t            symbols_per_frame;  /* 1024 or 512 */
    uint32_t            mclk_hz;            /* 49.152 or 45.158 MHz */

    /* Pin configurations for up to 2 ports */
    port_pins_t         port[2];            /* [0]=upstream, [1]=downstream */
    uint8_t             num_ports;          /* 1 for master/end-node, 2 for intermediate */

    /* Active port (for GPIO matrix switching in dual-port mode) */
    uint8_t             active_port;

    /* External clock GPIO (master: oscillator pin; slave: upstream RCLK) */
    int8_t              ext_clk_gpio;

    /* PARLIO handles */
    parlio_tx_unit_handle_t     tx_unit;
    parlio_rx_unit_handle_t     rx_unit;
    parlio_rx_delimiter_handle_t rx_delimiter;

    /* DMA frame buffers — 16-bit aligned, double-buffered */
    uint16_t           *tx_buf[2];          /* Two TX buffers for ping-pong */
    uint8_t             tx_buf_idx;         /* Currently transmitting buffer */
    uint16_t           *rx_buf[2];          /* Two RX buffers */
    uint8_t             rx_buf_idx;

    /* 8b10b codec state */
    abus_8b10b_encoder_t encoder;
    abus_8b10b_decoder_t decoder;

    /* Frame RX callback (set by upper layer) */
    void (*rx_frame_cb)(uint8_t port, const uint8_t *data, uint16_t len, void *arg);
    void *rx_frame_cb_arg;

    /* Decoded frame buffer (data bytes after 8b10b decode) */
    uint8_t            *decoded_buf;
    uint8_t            *decoded_k_buf;      /* K-character flags */

    /* Synchronization */
    SemaphoreHandle_t   tx_done_sem;
    volatile bool       running;

    /* Statistics */
    uint32_t            frames_tx;
    uint32_t            frames_rx;
    uint32_t            crc_errors;
    uint32_t            sync_losses;

} lvds_phy_ctx_t;

/* ---------------------------------------------------------------------------
 * GPIO helpers
 * --------------------------------------------------------------------------- */

static void configure_oe_gpio(int8_t pin, bool initial_state) {
    if (pin < 0) return;
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(pin, initial_state ? 1 : 0);
}

static void configure_lock_gpio(int8_t pin) {
    if (pin < 0) return;
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

static inline void set_tx_oe(lvds_phy_ctx_t *ctx, uint8_t port, bool enable) {
    int8_t pin = ctx->port[port].tx_oe_pin;
    if (pin >= 0) {
        gpio_set_level(pin, enable ? 1 : 0);
    }
}

static inline bool get_lock_status(lvds_phy_ctx_t *ctx, uint8_t port) {
    int8_t pin = ctx->port[port].rx_lock_pin;
    if (pin < 0) return false;
    return gpio_get_level(pin) != 0;
}

/* ---------------------------------------------------------------------------
 * PARLIO TX/RX setup
 * --------------------------------------------------------------------------- */

static esp_err_t create_tx_unit(lvds_phy_ctx_t *ctx, uint8_t port_idx) {
    port_pins_t *pp = &ctx->port[port_idx];

    gpio_num_t data_pins[PARLIO_DATA_WIDTH];
    for (int i = 0; i < PARLIO_DATA_WIDTH; i++) {
        data_pins[i] = (i < SERDES_DATA_BITS) ? pp->tx_data_pins[i] : -1;
    }

    parlio_tx_unit_config_t tx_cfg = {
        .clk_in_gpio_num = ctx->ext_clk_gpio,
        .input_clk_src_freq_hz = ctx->mclk_hz,
        .output_clk_freq_hz = ctx->mclk_hz,     /* Divider = 1 */
        .data_width = PARLIO_DATA_WIDTH,
        .clk_out_gpio_num = pp->tx_clk_pin,
        .valid_gpio_num = -1,                    /* Not using valid signal */
        .trans_queue_depth = 4,
        .max_transfer_size = MAX_TX_BUF_BYTES,
        .dma_burst_size = 64,
        .sample_edge = PARLIO_SAMPLE_EDGE_POS,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB,
        .flags = {
            .clk_gate_en = 0,
            .allow_pd = 0,
        },
    };
    memcpy(tx_cfg.data_gpio_nums, data_pins, sizeof(data_pins));

    return parlio_new_tx_unit(&tx_cfg, &ctx->tx_unit);
}

static esp_err_t create_rx_unit(lvds_phy_ctx_t *ctx, uint8_t port_idx) {
    port_pins_t *pp = &ctx->port[port_idx];

    gpio_num_t data_pins[PARLIO_DATA_WIDTH];
    for (int i = 0; i < PARLIO_DATA_WIDTH; i++) {
        data_pins[i] = (i < SERDES_DATA_BITS) ? pp->rx_data_pins[i] : -1;
    }

    parlio_rx_unit_config_t rx_cfg = {
        /* Use external clock from DS92LV1212A RCLK (recovered clock) */
        .clk_in_gpio_num = pp->rx_clk_pin,
        .ext_clk_freq_hz = ctx->mclk_hz,
        .data_width = PARLIO_DATA_WIDTH,
        .clk_out_gpio_num = -1,
        .valid_gpio_num = -1,               /* We use soft delimiter */
        .trans_queue_depth = 4,
        .max_recv_size = MAX_RX_BUF_BYTES,
        .dma_burst_size = 64,
        .flags = {
            .free_clk = 1,                  /* RCLK is free-running */
            .clk_gate_en = 0,
            .allow_pd = 0,
        },
    };
    memcpy(rx_cfg.data_gpio_nums, data_pins, sizeof(data_pins));

    return parlio_new_rx_unit(&rx_cfg, &ctx->rx_unit);
}

/* ---------------------------------------------------------------------------
 * ISR callbacks
 * --------------------------------------------------------------------------- */

static bool IRAM_ATTR tx_done_isr(parlio_tx_unit_handle_t unit,
                                   const parlio_tx_done_event_data_t *edata,
                                   void *user_data) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)user_data;
    BaseType_t hp_woken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->tx_done_sem, &hp_woken);
    ctx->frames_tx++;
    return hp_woken == pdTRUE;
}

static bool IRAM_ATTR rx_done_isr(parlio_rx_unit_handle_t unit,
                                   const parlio_rx_event_data_t *edata,
                                   void *user_data) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)user_data;
    BaseType_t hp_woken = pdFALSE;

    if (!ctx->rx_frame_cb || !edata->data) return false;

    /* Decode 8b10b symbols to data bytes */
    const uint16_t *sym_buf = (const uint16_t *)edata->data;
    int num_symbols = edata->recv_bytes / sizeof(uint16_t);

    int decoded_len = abus_8b10b_decode_frame(
        &ctx->decoder, sym_buf, num_symbols,
        ctx->decoded_buf, ctx->decoded_k_buf, num_symbols);

    if (decoded_len > 0) {
        /* Find SOF (K28.5 + K28.1) in decoded stream */
        for (int i = 0; i < decoded_len - 1; i++) {
            if (ctx->decoded_k_buf[i] && ctx->decoded_buf[i] == K28_5 &&
                ctx->decoded_k_buf[i + 1] && ctx->decoded_buf[i + 1] == K28_1) {
                /* Found SOF — extract frame payload */
                int frame_start = i;
                int frame_len = decoded_len - frame_start;
                ctx->rx_frame_cb(ctx->active_port,
                                 &ctx->decoded_buf[frame_start],
                                 frame_len, ctx->rx_frame_cb_arg);
                ctx->frames_rx++;
                break;
            }
        }
    }

    /* Re-queue RX buffer for next frame */
    uint8_t next_idx = ctx->rx_buf_idx ^ 1;
    parlio_receive_config_t recv_cfg = {
        .delimiter = ctx->rx_delimiter,
        .flags.partial_rx_en = false,
    };
    parlio_rx_unit_receive_from_isr(ctx->rx_unit, ctx->rx_buf[next_idx],
                                    MAX_RX_BUF_BYTES, &recv_cfg, &hp_woken);
    ctx->rx_buf_idx = next_idx;

    return hp_woken == pdTRUE;
}

/* ---------------------------------------------------------------------------
 * Frame encoding — build 8b10b symbol buffer for PARLIO TX DMA
 * --------------------------------------------------------------------------- */

/**
 * Encode a frame's data bytes into the TX DMA buffer (16-bit 8b10b symbols).
 *
 * Layout on wire:
 *   [GUARD: K28.3 idle × N] [K28.5 comma] [K28.1 SOF] [frame data...] [K29.7 EOF]
 *   [GUARD: K28.3 idle × N]
 *
 * The guard symbols are PHY-level padding for CDR relock at direction boundaries.
 */
static int encode_tx_frame(lvds_phy_ctx_t *ctx, const uint8_t *frame_data,
                           uint16_t frame_len, uint16_t *out_symbols) {
    abus_8b10b_encoder_t *enc = &ctx->encoder;
    int idx = 0;

    /* Leading guard: idle K-characters for CDR warm-up */
    for (int i = 0; i < GUARD_SYMBOLS; i++) {
        out_symbols[idx++] = abus_8b10b_encode_k(enc, K28_3);
    }

    /* SOF: comma + start-of-frame */
    out_symbols[idx++] = abus_8b10b_encode_k(enc, K28_5);
    out_symbols[idx++] = abus_8b10b_encode_k(enc, K28_1);

    /* Frame payload (data bytes) */
    for (int i = 0; i < frame_len; i++) {
        out_symbols[idx++] = abus_8b10b_encode_data(enc, frame_data[i]);
    }

    /* EOF marker */
    out_symbols[idx++] = abus_8b10b_encode_k(enc, K29_7);

    /* Trailing guard: idle for direction turnaround */
    for (int i = 0; i < GUARD_SYMBOLS; i++) {
        out_symbols[idx++] = abus_8b10b_encode_k(enc, K28_3);
    }

    return idx;
}

/* ---------------------------------------------------------------------------
 * PHY ops implementation
 * --------------------------------------------------------------------------- */

static esp_err_t lvds_init(void *phy_ctx) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;

    /* Allocate DMA buffers */
    for (int i = 0; i < 2; i++) {
        ctx->tx_buf[i] = heap_caps_aligned_calloc(DMA_ALIGN, MAX_TX_SYMBOLS,
                                                   sizeof(uint16_t), DMA_BUF_ATTR);
        ctx->rx_buf[i] = heap_caps_aligned_calloc(DMA_ALIGN, ABUS_SYMBOLS_48K,
                                                   sizeof(uint16_t), DMA_BUF_ATTR);
        if (!ctx->tx_buf[i] || !ctx->rx_buf[i]) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffers");
            return ESP_ERR_NO_MEM;
        }
    }

    ctx->decoded_buf = heap_caps_malloc(ABUS_FRAME_BYTES_MAX, MALLOC_CAP_INTERNAL);
    ctx->decoded_k_buf = heap_caps_malloc(ABUS_FRAME_BYTES_MAX, MALLOC_CAP_INTERNAL);
    if (!ctx->decoded_buf || !ctx->decoded_k_buf) return ESP_ERR_NO_MEM;

    ctx->tx_done_sem = xSemaphoreCreateBinary();

    /* Configure OE and LOCK GPIOs */
    for (int p = 0; p < ctx->num_ports; p++) {
        configure_oe_gpio(ctx->port[p].tx_oe_pin, false);   /* Start with TX disabled */
        configure_lock_gpio(ctx->port[p].rx_lock_pin);
    }

    /* Initialize 8b10b codec */
    abus_8b10b_encoder_init(&ctx->encoder);
    abus_8b10b_decoder_init(&ctx->decoder);

    /* Create PARLIO TX unit (initially for port 0) */
    esp_err_t ret = create_tx_unit(ctx, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PARLIO TX: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create PARLIO RX unit */
    ret = create_rx_unit(ctx, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PARLIO RX: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create soft delimiter for RX (fixed-size frames) */
    parlio_rx_soft_delimiter_config_t delim_cfg = {
        .sample_edge = PARLIO_SAMPLE_EDGE_POS,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB,
        .eof_data_len = ctx->symbols_per_frame * sizeof(uint16_t),
        .timeout_ticks = ctx->mclk_hz / 1000,  /* 1ms timeout */
    };
    ret = parlio_new_rx_soft_delimiter(&delim_cfg, &ctx->rx_delimiter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RX delimiter: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register ISR callbacks */
    parlio_tx_event_callbacks_t tx_cbs = {
        .on_trans_done = tx_done_isr,
    };
    parlio_tx_unit_register_event_callbacks(ctx->tx_unit, &tx_cbs, ctx);

    parlio_rx_event_callbacks_t rx_cbs = {
        .on_receive_done = rx_done_isr,
    };
    parlio_rx_unit_register_event_callbacks(ctx->rx_unit, &rx_cbs, ctx);

    ctx->active_port = 0;
    ESP_LOGI(TAG, "LVDS PHY initialized: %lu Hz, %u symbols/frame",
             ctx->mclk_hz, ctx->symbols_per_frame);

    return ESP_OK;
}

static esp_err_t lvds_start(void *phy_ctx) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;

    ctx->running = true;

    /* Enable PARLIO units */
    ESP_RETURN_ON_ERROR(parlio_tx_unit_enable(ctx->tx_unit), TAG, "TX enable failed");
    ESP_RETURN_ON_ERROR(parlio_rx_unit_enable(ctx->rx_unit, true), TAG, "RX enable failed");

    /* Queue initial RX buffer */
    parlio_receive_config_t recv_cfg = {
        .delimiter = ctx->rx_delimiter,
    };
    ESP_RETURN_ON_ERROR(
        parlio_rx_unit_receive(ctx->rx_unit, ctx->rx_buf[0],
                               MAX_RX_BUF_BYTES, &recv_cfg),
        TAG, "Initial RX failed");

    /* Start soft delimiter */
    parlio_rx_soft_delimiter_start_stop(ctx->rx_unit, ctx->rx_delimiter, true);

    /* If master, enable TX on port 0 (downstream) */
    if (ctx->role == ABUS_ROLE_MASTER) {
        set_tx_oe(ctx, 0, true);
    }

    ESP_LOGI(TAG, "LVDS PHY started, role=%s",
             ctx->role == ABUS_ROLE_MASTER ? "master" : "slave");
    return ESP_OK;
}

static esp_err_t lvds_stop(void *phy_ctx) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;
    ctx->running = false;

    /* Disable all TX outputs */
    for (int p = 0; p < ctx->num_ports; p++) {
        set_tx_oe(ctx, p, false);
    }

    parlio_rx_soft_delimiter_start_stop(ctx->rx_unit, ctx->rx_delimiter, false);
    parlio_tx_unit_disable(ctx->tx_unit);
    parlio_rx_unit_disable(ctx->rx_unit);

    ESP_LOGI(TAG, "LVDS PHY stopped");
    return ESP_OK;
}

static esp_err_t lvds_deinit(void *phy_ctx) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;

    if (ctx->tx_unit) parlio_del_tx_unit(ctx->tx_unit);
    if (ctx->rx_unit) parlio_del_rx_unit(ctx->rx_unit);
    if (ctx->rx_delimiter) parlio_del_rx_delimiter(ctx->rx_delimiter);
    if (ctx->tx_done_sem) vSemaphoreDelete(ctx->tx_done_sem);

    for (int i = 0; i < 2; i++) {
        heap_caps_free(ctx->tx_buf[i]);
        heap_caps_free(ctx->rx_buf[i]);
    }
    heap_caps_free(ctx->decoded_buf);
    heap_caps_free(ctx->decoded_k_buf);
    free(ctx);

    return ESP_OK;
}

static esp_err_t lvds_tx_frame(void *phy_ctx, uint8_t port,
                               const uint8_t *data, uint16_t len) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;

    /* Encode frame into 8b10b symbol buffer */
    uint8_t buf_idx = ctx->tx_buf_idx ^ 1;  /* Write to inactive buffer */
    int num_symbols = encode_tx_frame(ctx, data, len, ctx->tx_buf[buf_idx]);

    /* Ensure correct port is active (switch GPIO matrix if needed) */
    if (port != ctx->active_port && ctx->num_ports > 1) {
        /* TODO: GPIO matrix switching for dual-port intermediate nodes */
        ctx->active_port = port;
    }

    /* Enable TX output on the target port */
    set_tx_oe(ctx, port, true);

    /* Submit to PARLIO TX DMA — size is in BITS */
    parlio_transmit_config_t tx_cfg = {
        .idle_value = 0,
        .flags.queue_nonblocking = 0,
    };

    esp_err_t ret = parlio_tx_unit_transmit(
        ctx->tx_unit, ctx->tx_buf[buf_idx],
        num_symbols * PARLIO_DATA_WIDTH,  /* Total bits to transmit */
        &tx_cfg);

    if (ret == ESP_OK) {
        ctx->tx_buf_idx = buf_idx;
    }

    return ret;
}

static esp_err_t lvds_set_rx_callback(void *phy_ctx, uint8_t port,
                                       void (*cb)(uint8_t port, const uint8_t *data,
                                                 uint16_t len, void *arg),
                                       void *arg) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;
    ctx->rx_frame_cb = cb;
    ctx->rx_frame_cb_arg = arg;
    return ESP_OK;
}

static bool lvds_link_status(void *phy_ctx, uint8_t port) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;
    return get_lock_status(ctx, port);
}

static esp_err_t lvds_set_direction(void *phy_ctx, uint8_t port, bool tx) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;
    set_tx_oe(ctx, port, tx);
    return ESP_OK;
}

static uint32_t lvds_get_recovered_clock(void *phy_ctx, uint8_t port) {
    lvds_phy_ctx_t *ctx = (lvds_phy_ctx_t *)phy_ctx;
    if (get_lock_status(ctx, port)) {
        return ctx->mclk_hz;  /* When locked, RCLK = MCLK */
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * PHY ops vtable
 * --------------------------------------------------------------------------- */

static const abus_phy_ops_t lvds_phy_ops = {
    .init               = lvds_init,
    .start              = lvds_start,
    .stop               = lvds_stop,
    .deinit             = lvds_deinit,
    .tx_frame           = lvds_tx_frame,
    .set_rx_callback    = lvds_set_rx_callback,
    .link_status        = lvds_link_status,
    .set_direction      = lvds_set_direction,
    .get_recovered_clock = lvds_get_recovered_clock,
};

/* ---------------------------------------------------------------------------
 * Factory function
 * --------------------------------------------------------------------------- */

esp_err_t abus_phy_lvds_create(const void *pin_config, abus_sample_rate_t sr,
                               abus_role_t role, abus_phy_t *out_phy) {
    /* pin_config points to the lvds_pins struct in abus_pin_config_t */
    typedef struct {
        int8_t upstream_tx_data[10];
        int8_t upstream_tx_clk;
        int8_t upstream_tx_oe;
        int8_t upstream_rx_data[10];
        int8_t upstream_rx_clk;
        int8_t upstream_rx_lock;
        int8_t downstream_tx_data[10];
        int8_t downstream_tx_clk;
        int8_t downstream_tx_oe;
        int8_t downstream_rx_data[10];
        int8_t downstream_rx_clk;
        int8_t downstream_rx_lock;
    } lvds_pins_t;

    const lvds_pins_t *pins = (const lvds_pins_t *)pin_config;

    lvds_phy_ctx_t *ctx = calloc(1, sizeof(lvds_phy_ctx_t));
    if (!ctx) return ESP_ERR_NO_MEM;

    ctx->role = role;
    ctx->sample_rate = sr;

    /* Determine clock and frame size from sample rate */
    if (sr == ABUS_SR_48000 || sr == ABUS_SR_96000) {
        ctx->mclk_hz = ABUS_MCLK_48K;
    } else {
        ctx->mclk_hz = ABUS_MCLK_44K;
    }

    if (sr == ABUS_SR_48000 || sr == ABUS_SR_44100) {
        ctx->symbols_per_frame = 1024;
    } else {
        ctx->symbols_per_frame = 512;
    }

    /* Copy upstream port pins */
    memcpy(ctx->port[0].tx_data_pins, pins->upstream_tx_data, 10);
    ctx->port[0].tx_clk_pin  = pins->upstream_tx_clk;
    ctx->port[0].tx_oe_pin   = pins->upstream_tx_oe;
    memcpy(ctx->port[0].rx_data_pins, pins->upstream_rx_data, 10);
    ctx->port[0].rx_clk_pin  = pins->upstream_rx_clk;
    ctx->port[0].rx_lock_pin = pins->upstream_rx_lock;

    /* Check if downstream port is configured (intermediate node) */
    bool has_downstream = (pins->downstream_tx_clk >= 0);
    if (has_downstream) {
        memcpy(ctx->port[1].tx_data_pins, pins->downstream_tx_data, 10);
        ctx->port[1].tx_clk_pin  = pins->downstream_tx_clk;
        ctx->port[1].tx_oe_pin   = pins->downstream_tx_oe;
        memcpy(ctx->port[1].rx_data_pins, pins->downstream_rx_data, 10);
        ctx->port[1].rx_clk_pin  = pins->downstream_rx_clk;
        ctx->port[1].rx_lock_pin = pins->downstream_rx_lock;
        ctx->num_ports = 2;
    } else {
        ctx->num_ports = 1;
    }

    /* External clock source:
     * Master uses the upstream RX clock pin as a pass-through (the 49.152MHz
     * oscillator is connected to this pin, and also feeds PARLIO TX)
     * Slave uses the upstream RCLK (recovered clock from deserializer)
     */
    ctx->ext_clk_gpio = pins->upstream_rx_clk;

    out_phy->ops = &lvds_phy_ops;
    out_phy->ctx = ctx;

    ESP_LOGI(TAG, "LVDS PHY created: role=%s, sr=%u, mclk=%lu Hz, ports=%u",
             role == ABUS_ROLE_MASTER ? "master" : "slave",
             (unsigned)sr, ctx->mclk_hz, ctx->num_ports);

    return ESP_OK;
}
