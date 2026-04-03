/*
 * AudioBus Master Node Example — ESP32-P4
 *
 * Demonstrates a master node that:
 *   - Initializes the LVDS SerDes PHY on PARLIO
 *   - Discovers slave nodes on the daisy chain
 *   - Streams 2 channels of downstream audio (sine wave test tone)
 *   - Receives upstream audio from slaves
 *   - Tunnels MIDI data to slave nodes
 *
 * Hardware required:
 *   - ESP32-P4 DevKit
 *   - 49.152 MHz crystal oscillator module (audio master clock)
 *   - DS92LV1021A serializer + DS92LV1212A deserializer
 *   - LVDS twisted pair to first slave node
 *   - See docs/HARDWARE.md for full schematic
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audiobus.h"

static const char *TAG = "master_example";

/* GPIO pin assignments — adjust for your PCB */
#define PIN_MCLK_IN         6       /* 49.152 MHz oscillator → PARLIO ext clock */

/* Upstream port (to first slave node) */
#define PIN_TX_D0           7
#define PIN_TX_D1           8
#define PIN_TX_D2           9
#define PIN_TX_D3           10
#define PIN_TX_D4           11
#define PIN_TX_D5           12
#define PIN_TX_D6           13
#define PIN_TX_D7           14
#define PIN_TX_D8           15
#define PIN_TX_D9           16
#define PIN_TX_CLK          17      /* PARLIO clk_out → DS92LV1021A TCLK */
#define PIN_TX_OE           18      /* → DS92LV1021A PDB (output enable) */

#define PIN_RX_D0           19
#define PIN_RX_D1           20
#define PIN_RX_D2           21
#define PIN_RX_D3           22
#define PIN_RX_D4           23
#define PIN_RX_D5           24
#define PIN_RX_D6           25
#define PIN_RX_D7           26
#define PIN_RX_D8           27
#define PIN_RX_D9           28
#define PIN_RX_CLK          29      /* DS92LV1212A RCLK → PARLIO ext clock */
#define PIN_RX_LOCK         30      /* DS92LV1212A LOCK (CDR lock status) */

/* Event callback */
static void bus_event_handler(const abus_event_t *event, void *ctx) {
    switch (event->type) {
        case ABUS_EVT_NODE_DISCOVERED:
            ESP_LOGI(TAG, "Node %d discovered: type=%d, dn_ch=%d, up_ch=%d",
                     event->node_id,
                     event->discovered_node.hw_type,
                     event->discovered_node.max_dn_channels,
                     event->discovered_node.max_up_channels);
            break;

        case ABUS_EVT_NODE_LOST:
            ESP_LOGW(TAG, "Node %d lost!", event->node_id);
            break;

        case ABUS_EVT_LINK_UP:
            ESP_LOGI(TAG, "Link up on port %d", event->node_id);
            break;

        case ABUS_EVT_LINK_DOWN:
            ESP_LOGW(TAG, "Link down on port %d", event->node_id);
            break;

        case ABUS_EVT_CONFIG_COMPLETE:
            ESP_LOGI(TAG, "Bus configured, entering run mode");
            break;

        case ABUS_EVT_FRAME_ERROR:
            ESP_LOGD(TAG, "Frame error: 0x%08lx", (unsigned long)event->error_code);
            break;

        default:
            break;
    }
}

/* Generate a stereo sine wave test tone */
static void audio_generator_task(void *arg) {
    abus_handle_t bus = (abus_handle_t)arg;
    const float freq = 440.0f;
    const float sr = 48000.0f;
    const float amplitude = 0.5f;
    float phase = 0.0f;
    const float phase_inc = 2.0f * M_PI * freq / sr;

    int32_t samples[2];  /* Stereo: L + R */

    while (1) {
        /* Wait until bus is running */
        if (abus_get_state(bus) != ABUS_STATE_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Generate one sample frame */
        float val = amplitude * sinf(phase);
        samples[0] = (int32_t)(val * 2147483647.0f);  /* Left: sine */
        samples[1] = samples[0];                        /* Right: same */

        phase += phase_inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        /* Write to bus (blocks briefly if buffer full) */
        while (abus_audio_write(bus, samples, 1) == 0) {
            vTaskDelay(1);
        }
    }
}

/* Read upstream audio from slaves */
static void audio_receiver_task(void *arg) {
    abus_handle_t bus = (abus_handle_t)arg;
    int32_t samples[64];  /* Up to 64 channels */

    while (1) {
        if (abus_get_state(bus) != ABUS_STATE_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int n = abus_audio_read(bus, samples, 1);
        if (n > 0) {
            /* Process upstream audio here — e.g., record, mix, forward to USB/I2S */
            (void)samples;
        } else {
            vTaskDelay(1);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "AudioBus Master Node — starting");

    abus_config_t config = {
        .role = ABUS_ROLE_MASTER,
        .phy_type = ABUS_PHY_LVDS_SERDES,
        .sample_rate = ABUS_SR_48000,
        .bit_depth = ABUS_DEPTH_32,
        .expected_nodes = 0,        /* Auto-discover */
        .audio_buffer_frames = 8,
        .event_cb = bus_event_handler,
        .event_cb_ctx = NULL,
        .pins.lvds_pins = {
            .upstream_tx_data = {
                PIN_TX_D0, PIN_TX_D1, PIN_TX_D2, PIN_TX_D3, PIN_TX_D4,
                PIN_TX_D5, PIN_TX_D6, PIN_TX_D7, PIN_TX_D8, PIN_TX_D9,
            },
            .upstream_tx_clk  = PIN_TX_CLK,
            .upstream_tx_oe   = PIN_TX_OE,
            .upstream_rx_data = {
                PIN_RX_D0, PIN_RX_D1, PIN_RX_D2, PIN_RX_D3, PIN_RX_D4,
                PIN_RX_D5, PIN_RX_D6, PIN_RX_D7, PIN_RX_D8, PIN_RX_D9,
            },
            .upstream_rx_clk  = PIN_RX_CLK,
            .upstream_rx_lock = PIN_RX_LOCK,
            /* Master has no downstream port — single-port operation */
            .downstream_tx_clk = -1,
        },
    };

    abus_handle_t bus;
    esp_err_t ret = abus_init(&config, &bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "abus_init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = abus_start(bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "abus_start failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Start audio generator and receiver tasks */
    xTaskCreate(audio_generator_task, "audio_gen", 4096, bus, 10, NULL);
    xTaskCreate(audio_receiver_task, "audio_rx", 4096, bus, 10, NULL);

    /* Main loop: print stats periodically */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        abus_stats_t stats;
        abus_get_stats(bus, &stats);

        ESP_LOGI(TAG, "Stats: tx=%lu rx=%lu crc_err=%lu sync_loss=%lu overrun=%lu underrun=%lu",
                 stats.frames_tx, stats.frames_rx, stats.crc_errors,
                 stats.sync_losses, stats.buffer_overruns, stats.buffer_underruns);
        ESP_LOGI(TAG, "Nodes: %d, State: %d", abus_get_node_count(bus), abus_get_state(bus));
    }
}
