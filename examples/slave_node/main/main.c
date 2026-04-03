/*
 * AudioBus Slave Node Example — ESP32-P4 (or ESP32-S3 with reduced features)
 *
 * Demonstrates an end-of-chain slave node that:
 *   - Receives downstream audio from the bus
 *   - Outputs it via local I2S DAC
 *   - Captures upstream audio from local I2S ADC
 *   - Sends captured audio back upstream
 *   - Handles GPIO tunnel commands from master
 *
 * This example implements a "speaker + microphone" node.
 * Adapt for your use case (speaker only, analog bridge, etc.)
 *
 * Hardware required:
 *   - ESP32-P4 DevKit (or ESP32-S3 for simpler end-nodes)
 *   - DS92LV1021A + DS92LV1212A (upstream port only for end-node)
 *   - I2S DAC (e.g., PCM5102A) for audio output
 *   - I2S ADC (e.g., PCM1808) for audio input (optional)
 *   - No external clock oscillator needed — slave recovers clock from bus!
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audiobus.h"

static const char *TAG = "slave_example";

/*
 * GPIO pin assignments — adjust for your PCB.
 * Slave needs only the upstream port (toward master).
 * The recovered clock from DS92LV1212A RCLK is used for both
 * PARLIO TX and the local I2S DAC/ADC MCLK.
 */
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
#define PIN_TX_CLK          17
#define PIN_TX_OE           18

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
#define PIN_RX_CLK          29      /* DS92LV1212A RCLK (self-clocking!) */
#define PIN_RX_LOCK         30      /* DS92LV1212A LOCK */

/* I2S pins for local audio codec */
#define PIN_I2S_BCK         31
#define PIN_I2S_WS          32
#define PIN_I2S_DOUT        33      /* To DAC */
#define PIN_I2S_DIN         34      /* From ADC */

/* Tunnel callback — handle GPIO/MIDI commands from master */
static void tunnel_handler(uint8_t node_id, abus_tunnel_type_t type,
                           const uint8_t *data, uint16_t len, void *ctx) {
    switch (type) {
        case ABUS_TUNNEL_GPIO:
            if (len >= 2) {
                uint16_t pin_states = (data[0] << 8) | data[1];
                ESP_LOGD(TAG, "GPIO command from master: 0x%04X", pin_states);
                /* Apply pin states to local GPIOs */
                /* e.g., control amplifier enable, LED, mute relay, etc. */
            }
            break;

        case ABUS_TUNNEL_MIDI:
            ESP_LOGD(TAG, "MIDI from master: %d bytes", len);
            /* Forward to local MIDI UART or handle internally */
            break;

        default:
            break;
    }
}

/* Bus event handler */
static void bus_event_handler(const abus_event_t *event, void *ctx) {
    switch (event->type) {
        case ABUS_EVT_CONFIG_COMPLETE:
            ESP_LOGI(TAG, "Configured by master, entering run mode");
            break;
        case ABUS_EVT_LINK_DOWN:
            ESP_LOGW(TAG, "Lost connection to master!");
            break;
        case ABUS_EVT_LINK_UP:
            ESP_LOGI(TAG, "Connected to master");
            break;
        default:
            break;
    }
}

/*
 * Audio bridge task: moves audio between the bus and local I2S.
 *
 * Downstream audio (from master) → I2S TX (DAC) → speaker
 * I2S RX (ADC) → microphone audio → upstream (to master)
 */
static void audio_bridge_task(void *arg) {
    abus_handle_t bus = (abus_handle_t)arg;

    /* Local audio buffers (stereo for this example) */
    int32_t rx_samples[2];      /* From bus (downstream) */
    int32_t tx_samples[2];      /* To bus (upstream) */

    while (1) {
        if (abus_get_state(bus) != ABUS_STATE_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Read downstream audio from bus */
        if (abus_audio_read(bus, rx_samples, 1) > 0) {
            /* In a real implementation, write rx_samples to I2S TX DMA buffer.
             * The I2S driver would be configured to use the recovered clock
             * from the AudioBus PHY (DS92LV1212A RCLK) as MCLK, ensuring
             * perfect sample-rate synchronization with the master. */
        }

        /* Read from I2S RX (microphone) and send upstream.
         * In a real implementation, read from I2S RX DMA buffer. */
        tx_samples[0] = 0;     /* Placeholder — real data from ADC */
        tx_samples[1] = 0;
        abus_audio_write(bus, tx_samples, 1);

        /* Yield — in production, this would be driven by I2S DMA callbacks
         * rather than polling, for sample-accurate timing. */
        vTaskDelay(1);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "AudioBus Slave Node (speaker+mic) — starting");

    abus_config_t config = {
        .role = ABUS_ROLE_SLAVE,
        .phy_type = ABUS_PHY_LVDS_SERDES,
        .sample_rate = ABUS_SR_48000,
        .bit_depth = ABUS_DEPTH_32,
        .audio_buffer_frames = 8,
        .event_cb = bus_event_handler,
        .event_cb_ctx = NULL,

        /* Node descriptor — advertises our capabilities during discovery */
        .node_desc = {
            .hw_type = 3,               /* speaker + mic */
            .max_dn_channels = 2,       /* Want 2 channels of downstream audio */
            .max_up_channels = 2,       /* Can provide 2 upstream channels */
            .preferred_depth = 32,
            .tunnel_request = (1 << ABUS_TUNNEL_GPIO) | (1 << ABUS_TUNNEL_MIDI),
            .tunnel_bw_request = 8,     /* 2 (GPIO) + 3 (MIDI) + 3 spare */
            .uid = 0xDEAD0001,          /* Unique hardware ID */
        },

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
            /* End-node: no downstream port */
            .downstream_tx_clk = -1,
        },
    };

    abus_handle_t bus;
    esp_err_t ret = abus_init(&config, &bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "abus_init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Register tunnel callback before starting */
    abus_tunnel_register(bus, tunnel_handler, NULL);

    ret = abus_start(bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "abus_start failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Start audio bridge task */
    xTaskCreate(audio_bridge_task, "audio_bridge", 4096, bus, 10, NULL);

    /* Main loop: status reporting */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        abus_stats_t stats;
        abus_get_stats(bus, &stats);
        ESP_LOGI(TAG, "State=%d tx=%lu rx=%lu crc_err=%lu",
                 abus_get_state(bus), stats.frames_tx, stats.frames_rx,
                 stats.crc_errors);
    }
}
