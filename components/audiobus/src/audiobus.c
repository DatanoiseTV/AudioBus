/*
 * AudioBus — Core protocol engine
 *
 * State machine:
 *   RESET → INIT → DISCOVERY → CONFIG → RUNNING
 *                                     ↘ ERROR
 *
 * Master operation:
 *   1. Send discovery beacons, collect node descriptors
 *   2. Compute slot map (deterministic packing)
 *   3. Distribute config to all nodes
 *   4. Enter RUNNING: every sample period, pack downstream frame → TX,
 *      receive upstream frame → unpack → audio ring buffer
 *
 * Slave operation:
 *   1. Wait for discovery beacon on upstream port
 *   2. Respond with node descriptor
 *   3. Receive slot map configuration
 *   4. Enter RUNNING: receive downstream frame → extract local audio,
 *      insert upstream audio → forward (or respond if end-node)
 *
 * The frame processing runs in a high-priority FreeRTOS task driven by
 * PHY RX callbacks (ISR → task notification → process → TX next frame).
 *
 * SPDX-License-Identifier: MIT
 */

#include "audiobus.h"
#include "audiobus_phy.h"
#include "audiobus_tunnel.h"
#include "audiobus_types.h"

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"

#include <string.h>

static const char *TAG = "audiobus";

/* Frame packing functions (audiobus_frame.c) */
extern uint32_t abus_crc32(const uint8_t *data, int len);
extern uint8_t  abus_crc8(const uint8_t *data, int len);
extern int  abus_slotmap_compute(const abus_node_descriptor_t *nodes, uint8_t node_count,
                                 abus_sample_rate_t sr, abus_bit_depth_t bd,
                                 abus_slotmap_t *out);
extern void abus_frame_pack(const abus_slotmap_t *slotmap, uint16_t frame_counter,
                            const int32_t *dn_audio, const int32_t *up_audio,
                            const uint8_t *dn_aux, uint16_t dn_aux_len,
                            const uint8_t *up_aux, uint16_t up_aux_len,
                            uint8_t dn_sideband, uint8_t up_sideband,
                            uint8_t sr_flag, uint8_t bd_flag, uint8_t node_count,
                            uint8_t *out_frame);
extern int  abus_frame_unpack(const abus_slotmap_t *slotmap,
                              const uint8_t *frame, uint16_t frame_len,
                              int32_t *out_dn_audio, int32_t *out_up_audio,
                              uint8_t *out_dn_aux, uint16_t *out_dn_aux_len,
                              uint8_t *out_up_aux, uint16_t *out_up_aux_len,
                              uint8_t *out_dn_sideband, uint8_t *out_up_sideband,
                              abus_frame_header_t *out_hdr);

/* Discovery functions (audiobus_discovery.c) */
extern int abus_discovery_pack_beacon(uint8_t *buf, int buf_len, uint8_t target_node_id);
extern int abus_discovery_pack_response(uint8_t *buf, int buf_len,
                                        const abus_node_descriptor_t *desc);
extern int abus_discovery_unpack(const uint8_t *buf, int len,
                                 abus_node_descriptor_t *out_desc);
extern int abus_discovery_pack_config(uint8_t *buf, int buf_len,
                                      const abus_slotmap_t *slotmap);
extern int abus_discovery_unpack_config(const uint8_t *buf, int len,
                                        abus_slotmap_t *out_slotmap);

/* ---------------------------------------------------------------------------
 * Audio ring buffer
 * --------------------------------------------------------------------------- */

static void ringbuf_init(abus_audio_buffer_t *rb, uint16_t num_channels,
                         uint16_t num_frames) {
    rb->num_channels = num_channels;
    rb->num_frames = num_frames;
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->samples = heap_caps_calloc(num_channels * num_frames, sizeof(int32_t),
                                   MALLOC_CAP_INTERNAL);
}

static void ringbuf_free(abus_audio_buffer_t *rb) {
    heap_caps_free(rb->samples);
    rb->samples = NULL;
}

/* FIX #5: Use atomic load/store with acquire/release for multi-core safety.
 * volatile alone does NOT guarantee memory ordering on ESP32-P4 (RISC-V). */

static inline uint16_t ringbuf_available(const abus_audio_buffer_t *rb) {
    uint16_t wp = __atomic_load_n(&rb->write_pos, __ATOMIC_ACQUIRE);
    uint16_t rp = __atomic_load_n(&rb->read_pos, __ATOMIC_ACQUIRE);
    int diff = (int)wp - (int)rp;
    if (diff < 0) diff += rb->num_frames;
    return (uint16_t)diff;
}

static inline uint16_t ringbuf_space(const abus_audio_buffer_t *rb) {
    return rb->num_frames - 1 - ringbuf_available(rb);
}

static int ringbuf_write(abus_audio_buffer_t *rb, const int32_t *samples, int frames) {
    int written = 0;
    uint16_t nc = rb->num_channels;
    while (written < frames && ringbuf_space(rb) > 0) {
        uint16_t wp = rb->write_pos;
        memcpy(&rb->samples[wp * nc], &samples[written * nc], nc * sizeof(int32_t));
        /* Release: ensure memcpy is visible before index update */
        __atomic_store_n(&rb->write_pos, (wp + 1) % rb->num_frames, __ATOMIC_RELEASE);
        written++;
    }
    return written;
}

static int ringbuf_read(abus_audio_buffer_t *rb, int32_t *samples, int frames) {
    int read_count = 0;
    uint16_t nc = rb->num_channels;
    while (read_count < frames && ringbuf_available(rb) > 0) {
        uint16_t rp = rb->read_pos;
        memcpy(&samples[read_count * nc], &rb->samples[rp * nc], nc * sizeof(int32_t));
        __atomic_store_n(&rb->read_pos, (rp + 1) % rb->num_frames, __ATOMIC_RELEASE);
        read_count++;
    }
    return read_count;
}

/* ---------------------------------------------------------------------------
 * Internal handle structure
 * --------------------------------------------------------------------------- */

struct abus_handle {
    abus_config_t       config;
    abus_phy_t          phy;
    abus_state_t        state;

    /* Slot map (computed during CONFIG phase) */
    abus_slotmap_t      slotmap;

    /* Discovered nodes */
    abus_node_descriptor_t nodes[ABUS_MAX_NODES];
    uint8_t             node_count;
    uint8_t             my_node_id;         /* 0 for master, assigned for slaves */

    /* Audio ring buffers */
    abus_audio_buffer_t tx_audio;           /* App writes, frame packer reads */
    abus_audio_buffer_t rx_audio;           /* Frame unpacker writes, app reads */

    /* Tunnel context */
    abus_tunnel_ctx_t  *tunnel_ctx;
    abus_tunnel_cb_t    tunnel_cb;
    void               *tunnel_cb_ctx;

    /* Frame counter */
    uint16_t            frame_counter;

    /* Sideband */
    volatile uint8_t    tx_sideband;
    volatile uint8_t    rx_sideband;

    /* Scratch buffers for frame packing/unpacking (not DMA, just working memory) */
    uint8_t            *frame_buf;
    int32_t            *dn_audio_scratch;
    int32_t            *up_audio_scratch;
    uint8_t            *dn_aux_scratch;
    uint8_t            *up_aux_scratch;

    /* Frame processing task */
    TaskHandle_t        task_handle;
    QueueHandle_t       rx_queue;           /* ISR pushes received frames here */

    /* Statistics */
    abus_stats_t        stats;
};

/* Flags encoding helpers */
static uint8_t sr_to_flag(abus_sample_rate_t sr) {
    switch (sr) {
        case ABUS_SR_48000: return ABUS_FLAG_SR_48K;
        case ABUS_SR_96000: return ABUS_FLAG_SR_96K;
        case ABUS_SR_44100: return ABUS_FLAG_SR_44K;
        case ABUS_SR_88200: return ABUS_FLAG_SR_88K;
        default: return 0;
    }
}

static uint8_t bd_to_flag(abus_bit_depth_t bd) {
    switch (bd) {
        case ABUS_DEPTH_16: return ABUS_FLAG_BD_16;
        case ABUS_DEPTH_24: return ABUS_FLAG_BD_24;
        case ABUS_DEPTH_32: return ABUS_FLAG_BD_32;
        default: return 0;
    }
}

/* ---------------------------------------------------------------------------
 * PHY RX callback — called from ISR context.
 *
 * FIX #1: Do NOT call heap_caps_malloc from ISR. Use a pre-allocated
 * pool of frame buffers. The ISR copies data into the next free slot
 * and enqueues a lightweight index, not a heap pointer.
 * --------------------------------------------------------------------------- */

#define RX_POOL_COUNT   8
#define RX_POOL_BUFSZ   ABUS_FRAME_BYTES_MAX

typedef struct {
    uint8_t  port;
    uint16_t len;
    uint8_t  data[RX_POOL_BUFSZ];
} rx_frame_slot_t;

/* Pre-allocated pool (initialized in abus_init) */
static rx_frame_slot_t *rx_pool;
static volatile uint8_t rx_pool_write;  /* Next slot to write (ISR side) */
static volatile uint8_t rx_pool_read;   /* Next slot to read (task side) */

static void phy_rx_callback(uint8_t port, const uint8_t *data,
                             uint16_t len, void *arg) {
    struct abus_handle *h = (struct abus_handle *)arg;
    if (!h || !data || len == 0 || len > RX_POOL_BUFSZ) return;

    /* Check if pool has a free slot (lock-free single-producer check) */
    uint8_t wp = rx_pool_write;
    uint8_t next = (wp + 1) % RX_POOL_COUNT;
    if (next == rx_pool_read) {
        h->stats.buffer_overruns++;
        return;  /* Pool full — drop frame */
    }

    /* Copy into pre-allocated slot (no malloc!) */
    rx_frame_slot_t *slot = &rx_pool[wp];
    slot->port = port;
    slot->len = len;
    memcpy(slot->data, data, len);
    __atomic_store_n(&rx_pool_write, next, __ATOMIC_RELEASE);

    /* Notify the processing task */
    BaseType_t hp = pdFALSE;
    uint8_t idx = wp;
    xQueueSendFromISR(h->rx_queue, &idx, &hp);
    portYIELD_FROM_ISR(hp);
}

/* ---------------------------------------------------------------------------
 * Master: process one frame cycle
 * --------------------------------------------------------------------------- */

static void master_frame_cycle(struct abus_handle *h) {
    /* Read audio from application TX ring buffer */
    uint16_t dn_channels = 0;
    for (int i = 0; i < h->slotmap.num_audio_slots; i++) {
        if (h->slotmap.audio_slots[i].direction == ABUS_DIR_DOWNSTREAM)
            dn_channels++;
    }

    if (dn_channels > 0) {
        ringbuf_read(&h->tx_audio, h->dn_audio_scratch, 1);
    }

    /* Pack tunnel data */
    uint16_t dn_aux_len = 0;
    if (h->tunnel_ctx) {
        dn_aux_len = abus_tunnel_pack(h->tunnel_ctx, h->dn_aux_scratch,
                                       h->slotmap.dn_aux_bytes, ABUS_DIR_DOWNSTREAM);
    }

    /* Pack frame */
    abus_frame_pack(&h->slotmap, h->frame_counter++,
                    h->dn_audio_scratch, NULL,
                    h->dn_aux_scratch, dn_aux_len,
                    NULL, 0,
                    h->tx_sideband, 0,
                    sr_to_flag(h->config.sample_rate),
                    bd_to_flag(h->config.bit_depth),
                    h->node_count,
                    h->frame_buf);

    /* Transmit downstream frame */
    uint8_t tx_port = (h->config.role == ABUS_ROLE_MASTER) ? 0 : 1;
    h->phy.ops->tx_frame(h->phy.ctx, tx_port,
                         h->frame_buf, h->slotmap.frame_bytes);
    h->stats.frames_tx++;
}

/* ---------------------------------------------------------------------------
 * Process a received frame (master or slave)
 * --------------------------------------------------------------------------- */

static void process_rx_frame(struct abus_handle *h, const rx_frame_slot_t *msg) {
    abus_frame_header_t hdr;
    uint16_t dn_aux_len = 0, up_aux_len = 0;
    uint8_t dn_sideband = 0, up_sideband = 0;

    int ret = abus_frame_unpack(&h->slotmap, msg->data, msg->len,
                                h->dn_audio_scratch, h->up_audio_scratch,
                                h->dn_aux_scratch, &dn_aux_len,
                                h->up_aux_scratch, &up_aux_len,
                                &dn_sideband, &up_sideband, &hdr);

    if (ret == -1) { h->stats.crc_errors++; return; }
    if (ret == -2) { h->stats.sync_losses++; return; }
    if (ret == -3) { /* Slot map mismatch — need reconfig */ return; }

    h->stats.frames_rx++;

    if (h->config.role == ABUS_ROLE_MASTER) {
        /* Master receives upstream audio */
        ringbuf_write(&h->rx_audio, h->up_audio_scratch, 1);
        h->rx_sideband = up_sideband;

        /* Process upstream tunnel data */
        if (h->tunnel_ctx && up_aux_len > 0) {
            abus_tunnel_unpack(h->tunnel_ctx, h->up_aux_scratch,
                              up_aux_len, ABUS_DIR_UPSTREAM);
        }

        /* Immediately send next downstream frame */
        master_frame_cycle(h);

    } else {
        /* Slave receives downstream audio */
        ringbuf_write(&h->rx_audio, h->dn_audio_scratch, 1);
        h->rx_sideband = dn_sideband;

        /* Process downstream tunnel data */
        if (h->tunnel_ctx && dn_aux_len > 0) {
            abus_tunnel_unpack(h->tunnel_ctx, h->dn_aux_scratch,
                              dn_aux_len, ABUS_DIR_DOWNSTREAM);
        }

        /* Prepare upstream response:
         * Read application's upstream audio from TX ring buffer,
         * insert into the frame's upstream slots, and transmit back. */
        uint16_t up_channels = 0;
        for (int i = 0; i < h->slotmap.num_audio_slots; i++) {
            if (h->slotmap.audio_slots[i].direction == ABUS_DIR_UPSTREAM &&
                h->slotmap.audio_slots[i].node_id == h->my_node_id) {
                up_channels++;
            }
        }

        if (up_channels > 0) {
            ringbuf_read(&h->tx_audio, h->up_audio_scratch, 1);
        }

        uint16_t up_tunnel_len = 0;
        if (h->tunnel_ctx) {
            up_tunnel_len = abus_tunnel_pack(h->tunnel_ctx, h->up_aux_scratch,
                                             h->slotmap.up_aux_bytes, ABUS_DIR_UPSTREAM);
        }

        /* Re-pack frame with upstream data filled in */
        abus_frame_pack(&h->slotmap, hdr.frame_counter,
                        h->dn_audio_scratch,    /* Keep downstream as-is for forwarding */
                        h->up_audio_scratch,
                        h->dn_aux_scratch, dn_aux_len,
                        h->up_aux_scratch, up_tunnel_len,
                        dn_sideband, h->tx_sideband,
                        hdr.flags & ABUS_FLAG_SR_MASK,
                        hdr.flags & ABUS_FLAG_BD_MASK,
                        hdr.node_count,
                        h->frame_buf);

        /* Switch direction and transmit upstream */
        h->phy.ops->set_direction(h->phy.ctx, 0, true);  /* Enable TX on upstream port */
        h->phy.ops->tx_frame(h->phy.ctx, 0, h->frame_buf, h->slotmap.frame_bytes);
        h->stats.frames_tx++;
    }
}

/* ---------------------------------------------------------------------------
 * Frame processing task (runs at highest audio priority)
 * --------------------------------------------------------------------------- */

static void frame_task(void *arg) {
    struct abus_handle *h = (struct abus_handle *)arg;
    uint8_t slot_idx;

    /* Master: kick off the first frame */
    if (h->config.role == ABUS_ROLE_MASTER && h->state == ABUS_STATE_RUNNING) {
        master_frame_cycle(h);
    }

    while (h->state == ABUS_STATE_RUNNING || h->state == ABUS_STATE_DISCOVERY) {
        /* FIX #1: Receive pool index, not heap pointer */
        if (xQueueReceive(h->rx_queue, &slot_idx, pdMS_TO_TICKS(100)) == pdTRUE) {
            rx_frame_slot_t *slot = &rx_pool[slot_idx];
            if (h->state == ABUS_STATE_RUNNING) {
                /* Build a temporary msg-like struct on stack for process_rx_frame */
                rx_frame_slot_t local_copy = *slot;
                process_rx_frame(h, &local_copy);
            }
            /* Release slot back to pool */
            __atomic_store_n(&rx_pool_read,
                             (slot_idx + 1) % RX_POOL_COUNT, __ATOMIC_RELEASE);
        }
    }

    /* FIX #17: Signal completion before deleting */
    xTaskNotifyGive(h->task_handle);
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * Public API implementation
 * --------------------------------------------------------------------------- */

esp_err_t abus_init(const abus_config_t *config, abus_handle_t *out_handle) {
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "null arg");

    struct abus_handle *h = calloc(1, sizeof(struct abus_handle));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "alloc handle");

    h->config = *config;
    h->state = ABUS_STATE_INIT;
    h->my_node_id = (config->role == ABUS_ROLE_MASTER) ? 0 : 0xFF;

    /* Create PHY */
    esp_err_t ret;
    if (config->phy_type == ABUS_PHY_LVDS_SINGLE) {
        ret = abus_phy_oneic_create(&config->pins.oneic_pins, config->sample_rate,
                                    config->role, &h->phy);
    } else if (config->phy_type == ABUS_PHY_LVDS_SERDES) {
        ret = abus_phy_lvds_create(&config->pins.serdes_pins, config->sample_rate,
                                   config->role, &h->phy);
    } else {
        ESP_LOGE(TAG, "Unsupported PHY type");
        free(h);
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_GOTO_ON_ERROR(ret, fail, TAG, "PHY create failed");

    ret = h->phy.ops->init(h->phy.ctx);
    ESP_GOTO_ON_ERROR(ret, fail, TAG, "PHY init failed");

    /* Register PHY RX callback */
    h->phy.ops->set_rx_callback(h->phy.ctx, 0, phy_rx_callback, h);

    /* Allocate scratch buffers */
    uint16_t frame_bytes = (config->sample_rate <= ABUS_SR_48000) ?
                           ABUS_FRAME_BYTES_48K : ABUS_FRAME_BYTES_96K;

    h->frame_buf = heap_caps_calloc(1, frame_bytes, MALLOC_CAP_INTERNAL);
    h->dn_audio_scratch = heap_caps_calloc(ABUS_MAX_CHANNELS, sizeof(int32_t),
                                           MALLOC_CAP_INTERNAL);
    h->up_audio_scratch = heap_caps_calloc(ABUS_MAX_CHANNELS, sizeof(int32_t),
                                           MALLOC_CAP_INTERNAL);
    h->dn_aux_scratch = heap_caps_calloc(1, frame_bytes / 2, MALLOC_CAP_INTERNAL);
    h->up_aux_scratch = heap_caps_calloc(1, frame_bytes / 2, MALLOC_CAP_INTERNAL);

    /* FIX #7: Check ALL scratch buffer allocations */
    if (!h->frame_buf || !h->dn_audio_scratch || !h->up_audio_scratch ||
        !h->dn_aux_scratch || !h->up_aux_scratch) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    /* Initialize tunnel subsystem */
    abus_tunnel_init(&h->tunnel_ctx);

    /* FIX #1: Allocate pre-allocated RX frame pool (ISR-safe) */
    rx_pool = heap_caps_calloc(RX_POOL_COUNT, sizeof(rx_frame_slot_t),
                               MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!rx_pool) { ret = ESP_ERR_NO_MEM; goto fail; }
    rx_pool_write = 0;
    rx_pool_read = 0;

    /* Create RX processing queue (carries pool indices, not pointers) */
    h->rx_queue = xQueueCreate(RX_POOL_COUNT, sizeof(uint8_t));

    /* Initialize audio ring buffers (will be resized after discovery) */
    uint16_t buf_frames = config->audio_buffer_frames ? config->audio_buffer_frames : 8;
    ringbuf_init(&h->tx_audio, ABUS_MAX_CHANNELS, buf_frames);
    ringbuf_init(&h->rx_audio, ABUS_MAX_CHANNELS, buf_frames);

    *out_handle = h;
    ESP_LOGI(TAG, "AudioBus initialized: role=%s, sr=%u, depth=%u",
             config->role == ABUS_ROLE_MASTER ? "master" : "slave",
             (unsigned)config->sample_rate, (unsigned)config->bit_depth);
    return ESP_OK;

fail:
    /* FIX #8: Full cleanup on failure */
    if (h) {
        if (h->phy.ops && h->phy.ctx) h->phy.ops->deinit(h->phy.ctx);
        if (h->tunnel_ctx) abus_tunnel_deinit(h->tunnel_ctx);
        if (h->rx_queue) vQueueDelete(h->rx_queue);
        ringbuf_free(&h->tx_audio);
        ringbuf_free(&h->rx_audio);
        heap_caps_free(h->frame_buf);
        heap_caps_free(h->dn_audio_scratch);
        heap_caps_free(h->up_audio_scratch);
        heap_caps_free(h->dn_aux_scratch);
        heap_caps_free(h->up_aux_scratch);
        heap_caps_free(rx_pool);
        rx_pool = NULL;
        free(h);
    }
    return ret;
}

esp_err_t abus_start(abus_handle_t handle) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");

    esp_err_t ret = h->phy.ops->start(h->phy.ctx);
    ESP_RETURN_ON_ERROR(ret, TAG, "PHY start failed");

    if (h->config.role == ABUS_ROLE_MASTER) {
        /* Master node is always node 0 */
        h->nodes[0].node_id = 0;
        h->nodes[0].max_dn_channels = 2;
        h->nodes[0].max_up_channels = 2;
        h->node_count = 1;

        /* --- LVDS discovery: probe for downstream nodes --- */
        h->state = ABUS_STATE_DISCOVERY;
        ESP_LOGI(TAG, "Starting LVDS discovery...");

        uint8_t beacon_buf[64];
        uint8_t resp_slot_idx;

        for (uint8_t nid = 1; nid < ABUS_MAX_NODES; nid++) {
            /* Build and send a discovery beacon for this node_id */
            int blen = abus_discovery_pack_beacon(beacon_buf, sizeof(beacon_buf), nid);
            if (blen < 0) break;

            h->phy.ops->tx_frame(h->phy.ctx, 0, beacon_buf, blen);

            /* Wait for a discovery response with 50ms timeout */
            bool got_response = false;
            if (xQueueReceive(h->rx_queue, &resp_slot_idx, pdMS_TO_TICKS(50)) == pdTRUE) {
                rx_frame_slot_t *slot = &rx_pool[resp_slot_idx];
                abus_node_descriptor_t desc;
                if (abus_discovery_unpack(slot->data, slot->len, &desc) == 0 &&
                    slot->data[0] == 0x02 /* DISC_SUBTYPE_RESPONSE */) {
                    desc.node_id = nid;
                    h->nodes[nid] = desc;
                    h->node_count = nid + 1;
                    got_response = true;
                    ESP_LOGI(TAG, "Discovered node %u: hw_type=%u, dn_ch=%u, up_ch=%u, uid=0x%08lx",
                             nid, desc.hw_type, desc.max_dn_channels,
                             desc.max_up_channels, (unsigned long)desc.uid);
                }
                /* Release slot back to pool */
                __atomic_store_n(&rx_pool_read,
                                 (resp_slot_idx + 1) % RX_POOL_COUNT, __ATOMIC_RELEASE);
            }

            if (!got_response) {
                ESP_LOGI(TAG, "No response for node %u — end of chain", nid);
                break;  /* No more nodes in the chain */
            }
        }

        ESP_LOGI(TAG, "Discovery complete: %u node(s) found", h->node_count);

        /* Compute slot map with all discovered nodes */
        abus_slotmap_compute(h->nodes, h->node_count,
                             h->config.sample_rate, h->config.bit_depth,
                             &h->slotmap);

        /* Send config frame to all slave nodes */
        if (h->node_count > 1) {
            uint8_t config_buf[2 + sizeof(abus_slotmap_t)];
            int clen = abus_discovery_pack_config(config_buf, sizeof(config_buf),
                                                   &h->slotmap);
            if (clen > 0) {
                h->phy.ops->tx_frame(h->phy.ctx, 0, config_buf, clen);
                ESP_LOGI(TAG, "Sent slot map config to %u slave(s)", h->node_count - 1);
            }
        }

        h->state = ABUS_STATE_RUNNING;
    } else {
        h->state = ABUS_STATE_DISCOVERY;
    }

    /* Start frame processing task at high priority */
    xTaskCreatePinnedToCore(frame_task, "abus_frame", 4096, h,
                            configMAX_PRIORITIES - 2, &h->task_handle, 1);

    ESP_LOGI(TAG, "AudioBus started");
    return ESP_OK;
}

esp_err_t abus_stop(abus_handle_t handle) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");

    h->state = ABUS_STATE_RESET;

    /* FIX #17: Wait for task to signal completion, not blind delay */
    if (h->task_handle) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        h->task_handle = NULL;
    }

    h->phy.ops->stop(h->phy.ctx);
    ESP_LOGI(TAG, "AudioBus stopped");
    return ESP_OK;
}

esp_err_t abus_deinit(abus_handle_t handle) {
    struct abus_handle *h = handle;
    if (!h) return ESP_OK;

    abus_stop(h);

    h->phy.ops->deinit(h->phy.ctx);

    ringbuf_free(&h->tx_audio);
    ringbuf_free(&h->rx_audio);
    if (h->tunnel_ctx) abus_tunnel_deinit(h->tunnel_ctx);
    vQueueDelete(h->rx_queue);

    heap_caps_free(h->frame_buf);
    heap_caps_free(h->dn_audio_scratch);
    heap_caps_free(h->up_audio_scratch);
    heap_caps_free(h->dn_aux_scratch);
    heap_caps_free(h->up_aux_scratch);
    free(h);
    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * Audio I/O
 * --------------------------------------------------------------------------- */

int abus_audio_write(abus_handle_t handle, const int32_t *samples, int frames) {
    struct abus_handle *h = handle;
    if (!h || !samples) return 0;
    return ringbuf_write(&h->tx_audio, samples, frames);
}

int abus_audio_read(abus_handle_t handle, int32_t *samples, int frames) {
    struct abus_handle *h = handle;
    if (!h || !samples) return 0;
    return ringbuf_read(&h->rx_audio, samples, frames);
}

int abus_audio_available(abus_handle_t handle) {
    struct abus_handle *h = handle;
    if (!h) return 0;
    return ringbuf_available(&h->rx_audio);
}

/* ---------------------------------------------------------------------------
 * Tunnel I/O
 * --------------------------------------------------------------------------- */

esp_err_t abus_tunnel_send(abus_handle_t handle, uint8_t node_id,
                           abus_tunnel_type_t type,
                           const uint8_t *data, uint16_t len) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h && data, ESP_ERR_INVALID_ARG, TAG, "null arg");
    /* FIX #19: Return error until properly implemented */
    (void)node_id; (void)type; (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t abus_tunnel_register(abus_handle_t handle, abus_tunnel_cb_t cb, void *ctx) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
    h->tunnel_cb = cb;
    h->tunnel_cb_ctx = ctx;
    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * Sideband
 * --------------------------------------------------------------------------- */

esp_err_t abus_sideband_set(abus_handle_t handle, bool value) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
    h->tx_sideband = value ? 0xFF : 0x00;
    return ESP_OK;
}

bool abus_sideband_get(abus_handle_t handle) {
    struct abus_handle *h = handle;
    if (!h) return false;
    return h->rx_sideband != 0;
}

/* ---------------------------------------------------------------------------
 * Status
 * --------------------------------------------------------------------------- */

const abus_slotmap_t *abus_get_slotmap(abus_handle_t handle) {
    struct abus_handle *h = handle;
    return h ? &h->slotmap : NULL;
}

abus_state_t abus_get_state(abus_handle_t handle) {
    struct abus_handle *h = handle;
    return h ? h->state : ABUS_STATE_RESET;
}

uint8_t abus_get_node_count(abus_handle_t handle) {
    struct abus_handle *h = handle;
    return h ? h->node_count : 0;
}

esp_err_t abus_get_node_info(abus_handle_t handle, uint8_t node_id,
                             abus_node_descriptor_t *out_desc) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h && out_desc, ESP_ERR_INVALID_ARG, TAG, "null arg");
    ESP_RETURN_ON_FALSE(node_id < h->node_count, ESP_ERR_INVALID_ARG, TAG, "bad node_id");
    *out_desc = h->nodes[node_id];
    return ESP_OK;
}

esp_err_t abus_rediscover(abus_handle_t handle) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
    ESP_RETURN_ON_FALSE(h->config.role == ABUS_ROLE_MASTER, ESP_ERR_INVALID_STATE,
                        TAG, "only master can rediscover");
    h->state = ABUS_STATE_DISCOVERY;
    return ESP_OK;
}

esp_err_t abus_get_stats(abus_handle_t handle, abus_stats_t *out_stats) {
    struct abus_handle *h = handle;
    ESP_RETURN_ON_FALSE(h && out_stats, ESP_ERR_INVALID_ARG, TAG, "null arg");
    *out_stats = h->stats;
    return ESP_OK;
}
