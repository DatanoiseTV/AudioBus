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
 *   - SN65LVDT41 LVDS transceiver (~$2)
 *   - Si5351A clock generator (~$1.50, generates 98.304 MHz local clock)
 *   - I2S DAC (e.g., PCM5102A) for audio output
 *   - I2S ADC (e.g., PCM1808) for audio input (optional)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audiobus.h"

static const char *TAG = "slave_example";

/*
 * GPIO pin assignments — only 7 pins for bus + Si5351A!
 *
 *   GPIO 6  ←── Si5351A CLK0 output (98.304 MHz, generated from 25 MHz xtal)
 *   GPIO 7  ──→ SN65LVDT41 D  (TX data)
 *   GPIO 8  ←── SN65LVDT41 R  (RX data)
 *   GPIO 9  ──→ SN65LVDT41 DE (direction control)
 *   GPIO 10     PARLIO clock
 *   GPIO 3  ──→ Si5351A SDA (I2C for clock adjustment — software PLL)
 *   GPIO 4  ──→ Si5351A SCL
 */
#define PIN_EXT_CLK         6       /* Si5351A CLK0 → PARLIO ext clock */
#define PIN_TX_DATA         7       /* → SN65LVDT41 D */
#define PIN_RX_DATA         8       /* ← SN65LVDT41 R */
#define PIN_DE              9       /* → SN65LVDT41 DE */
#define PIN_PARLIO_CLK      10
#define PIN_I2C_SDA         3       /* Si5351A I2C */
#define PIN_I2C_SCL         4

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
        .phy_type = ABUS_PHY_LVDS_SINGLE,  /* Single-chip transceiver */
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

        /* Only 7 pins total (5 bus + 2 I2C for Si5351A) */
        .pins.oneic_pins = {
            .upstream_data   = PIN_TX_DATA,
            .upstream_clk    = PIN_PARLIO_CLK,
            .upstream_de     = PIN_DE,
            .downstream_data = -1,      /* End-node: no downstream */
            .downstream_clk  = -1,
            .downstream_de   = -1,
            .clk_in          = PIN_EXT_CLK,
            .i2c_sda         = PIN_I2C_SDA,   /* Si5351A for clock generation */
            .i2c_scl         = PIN_I2C_SCL,
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
