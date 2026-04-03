/*
 * AudioBus Master Node Example — ESP32-P4
 *
 * Demonstrates a master node using the SN65LVDT41 single-chip LVDS transceiver.
 * Only 5 GPIO pins for the audio bus — EMAC stays free for Ethernet.
 *
 * Hardware required:
 *   - ESP32-P4 DevKit
 *   - SN65LVDT41 LVDS transceiver (~$2)
 *   - 98.304 MHz crystal oscillator (2× audio clock, or Si5351A)
 *   - 100Ω twisted pair cable to first slave node
 *   - 100Ω termination resistor across the differential pair
 *   - See docs/HARDWARE.md for full schematic
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audiobus.h"

static const char *TAG = "master_example";

/*
 * GPIO pin assignments — only 5 pins for the audio bus!
 *
 *   ESP32-P4 GPIO 6  ←── 98.304 MHz oscillator (external clock input)
 *   ESP32-P4 GPIO 7  ──→ SN65LVDT41 D input  (TX data, PARLIO 1-bit out)
 *   ESP32-P4 GPIO 8  ←── SN65LVDT41 R output  (RX data, PARLIO 1-bit in)
 *   ESP32-P4 GPIO 9  ──→ SN65LVDT41 DE        (direction: HIGH=TX, LOW=RX)
 *   ESP32-P4 GPIO 10     (PARLIO clock output, optional routing)
 *
 *   SN65LVDT41 Y/Z ──── twisted pair ──── remote SN65LVDT41 A/B
 *   100Ω termination across Y/Z and across A/B
 */
#define PIN_EXT_CLK         6       /* 98.304 MHz oscillator */
#define PIN_TX_DATA         7       /* → SN65LVDT41 D (driver input) */
#define PIN_RX_DATA         8       /* ← SN65LVDT41 R (receiver output) */
#define PIN_DE              9       /* → SN65LVDT41 DE (driver enable) */
#define PIN_PARLIO_CLK      10      /* PARLIO clock (optional) */

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
            ESP_LOGI(TAG, "Link up");
            break;
        case ABUS_EVT_LINK_DOWN:
            ESP_LOGW(TAG, "Link down");
            break;
        default:
            break;
    }
}

/* Generate stereo sine wave test tone */
static void audio_generator_task(void *arg) {
    abus_handle_t bus = (abus_handle_t)arg;
    float phase = 0.0f;
    const float phase_inc = 2.0f * M_PI * 440.0f / 48000.0f;
    int32_t samples[2];

    while (1) {
        if (abus_get_state(bus) != ABUS_STATE_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        float val = 0.5f * sinf(phase);
        samples[0] = (int32_t)(val * 2147483647.0f);
        samples[1] = samples[0];
        phase += phase_inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        while (abus_audio_write(bus, samples, 1) == 0) {
            vTaskDelay(1);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "AudioBus Master Node — SN65LVDT41 single-chip PHY");

    abus_config_t config = {
        .role = ABUS_ROLE_MASTER,
        .phy_type = ABUS_PHY_LVDS_SINGLE,  /* Single-chip transceiver */
        .sample_rate = ABUS_SR_48000,
        .bit_depth = ABUS_DEPTH_32,
        .expected_nodes = 0,
        .audio_buffer_frames = 8,
        .event_cb = bus_event_handler,

        /* Only 5 pins! Compare to 24 for the 2-chip SerDes approach. */
        .pins.oneic_pins = {
            .upstream_data   = PIN_TX_DATA,
            .upstream_clk    = PIN_PARLIO_CLK,
            .upstream_de     = PIN_DE,
            .downstream_data = -1,      /* Master = single port */
            .downstream_clk  = -1,
            .downstream_de   = -1,
            .clk_in          = PIN_EXT_CLK,
            .i2c_sda         = -1,      /* Master doesn't need Si5351A */
            .i2c_scl         = -1,
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

    xTaskCreate(audio_generator_task, "audio_gen", 4096, bus, 10, NULL);

    /* Status reporting */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        abus_stats_t stats;
        abus_get_stats(bus, &stats);
        ESP_LOGI(TAG, "tx=%lu rx=%lu crc_err=%lu nodes=%d",
                 stats.frames_tx, stats.frames_rx,
                 stats.crc_errors, abus_get_node_count(bus));
    }
}
