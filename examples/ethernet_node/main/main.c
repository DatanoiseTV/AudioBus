/*
 * AudioBus Ethernet Node Example — ESP32-P4
 *
 * Plug-and-play networked audio: just plug into any Ethernet switch.
 * No master/slave configuration. Nodes discover each other automatically.
 *
 * This example creates a node that:
 *   - Publishes a 2-channel audio stream (stereo sine wave)
 *   - Auto-discovers other nodes on the network
 *   - Subscribes to the first stream it finds from another node
 *   - Runs PTP clock sync for sample-accurate playout
 *   - Coexists with normal IP traffic (Ethernet + WiFi both work)
 *
 * Hardware:
 *   - ESP32-P4 with Ethernet PHY (e.g., IP101/RTL8201 on eval board)
 *   - Standard Ethernet cable to any switch
 *   - That's it. No LVDS transceiver, no special cable.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "audiobus_net.h"

static const char *TAG = "eth_node";

static abus_net_handle_t net = NULL;
static uint16_t my_stream_id = 0;
static uint16_t subscribed_stream = 0;
static bool subscribed = false;

/* ---------------------------------------------------------------------------
 * Callbacks
 * --------------------------------------------------------------------------- */

static void on_node_discovered(const abus_net_node_t *node, void *ctx) {
    ESP_LOGI(TAG, "Discovered node: \"%s\" (uid=0x%08lx, type=%d, streams=%d)",
             node->name, (unsigned long)node->uid, node->hw_type, node->num_streams);
}

static void on_node_lost(uint32_t uid, void *ctx) {
    ESP_LOGW(TAG, "Node lost: 0x%08lx", (unsigned long)uid);
}

static void on_stream_announced(const abus_net_stream_t *stream, void *ctx) {
    ESP_LOGI(TAG, "Stream available: \"%s\" (id=0x%04X, %dch/%dbit/%luHz) from 0x%08lx",
             stream->name, stream->stream_id, stream->channels, stream->bit_depth,
             (unsigned long)stream->sample_rate, (unsigned long)stream->talker_uid);

    /* Auto-subscribe to the first remote stream we discover */
    if (!subscribed && net) {
        abus_net_subscribe(net, stream->talker_uid, stream->stream_id, 0);
        subscribed_stream = stream->stream_id;
        subscribed = true;
        ESP_LOGI(TAG, "Auto-subscribed to stream \"%s\"", stream->name);
    }
}

/* ---------------------------------------------------------------------------
 * Audio tasks
 * --------------------------------------------------------------------------- */

/* Generate and publish a stereo sine wave */
static void talker_task(void *arg) {
    float phase = 0.0f;
    const float freq = 440.0f;
    const float sr = 48000.0f;
    const float inc = 2.0f * M_PI * freq / sr;
    int32_t samples[2];

    while (1) {
        if (my_stream_id == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        float val = 0.3f * sinf(phase);
        int32_t s = (int32_t)(val * 2147483647.0f);
        samples[0] = s;
        samples[1] = s;
        phase += inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        while (abus_net_stream_write(net, my_stream_id, samples, 1) == 0) {
            vTaskDelay(1);
        }
    }
}

/* Read from a subscribed stream (would feed to I2S DAC in production) */
static void listener_task(void *arg) {
    int32_t samples[64];    /* Up to 64 channels */

    while (1) {
        if (!subscribed || subscribed_stream == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int n = abus_net_stream_read(net, subscribed_stream, samples, 1);
        if (n > 0) {
            /* In production: write samples to I2S DMA buffer for DAC output.
             * The PTP presentation timestamp ensures all listeners on the
             * network start playing this exact sample at the same moment. */
            (void)samples;
        } else {
            vTaskDelay(1);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Ethernet initialization (standard ESP-IDF)
 * --------------------------------------------------------------------------- */

static esp_eth_handle_t init_ethernet(void) {
    /* This is standard ESP-IDF Ethernet setup.
     * Adjust PHY model and pins for your specific board. */

    esp_netif_init();
    esp_event_loop_create_default();

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    /* Enable IEEE 1588 hardware timestamping (ESP32-P4 EMAC feature) */
    /* emac_config.flags |= ETH_ESP32_EMAC_FLAG_PTP_ENABLE; */ /* TODO: check actual API */

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 0;
    phy_config.reset_gpio_num = -1;

    /* Adjust PHY type for your board (IP101, RTL8201, LAN87xx, etc.) */
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_driver_install(&eth_config, &eth_handle);

    /* Attach to TCP/IP stack (so normal IP networking works too) */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle));

    esp_eth_start(eth_handle);

    ESP_LOGI(TAG, "Ethernet initialized — IP stack and AudioBus share the same port");
    return eth_handle;
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

void app_main(void) {
    ESP_LOGI(TAG, "AudioBus Ethernet Node — plug and play");

    /* 1. Initialize standard Ethernet (IP stack works normally) */
    esp_eth_handle_t eth = init_ethernet();

    /* 2. Initialize AudioBus Ethernet transport */
    abus_net_config_t config = {
        .uid = 0,                       /* Auto-generate from MAC */
        .name = "ESP32-P4 AudioNode",
        .hw_type = 3,                   /* speaker + mic */
        .ptp_priority = 128,            /* Default — any node can be grandmaster */
        .ptp_clock_class = 248,         /* Freerun (no external PTP reference) */
        .default_sample_rate = 48000,
        .default_bit_depth = 32,
        .default_packet_interval_us = 1000,  /* 1ms packets (48 samples) */
        .presentation_latency_us = 2000,     /* 2ms playout buffer */
        .dscp = 46,                          /* EF priority marking */
        .vlan_id = 0,                        /* No VLAN tagging */
        .on_node_discovered = on_node_discovered,
        .on_node_lost = on_node_lost,
        .on_stream_announced = on_stream_announced,
        .cb_ctx = NULL,
    };

    esp_err_t ret = abus_net_init(&config, &net);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "abus_net_init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Attach to the same Ethernet handle — AudioBus coexists with IP */
    abus_net_attach_eth(net, eth);

    abus_net_start(net);

    /* 3. Create a published audio stream */
    const char *labels[] = {"Left", "Right"};
    ret = abus_net_stream_create(net, "Main Stereo", 2, 48000, 32, 1000, &my_stream_id);
    if (ret == ESP_OK) {
        abus_net_stream_set_labels(net, my_stream_id, labels, 2);
        ESP_LOGI(TAG, "Publishing stream 0x%04X: \"Main Stereo\" (2ch/32bit/48kHz)",
                 my_stream_id);
    }

    /* 4. Start audio tasks */
    xTaskCreate(talker_task, "talker", 4096, NULL, 10, NULL);
    xTaskCreate(listener_task, "listener", 4096, NULL, 10, NULL);

    /* 5. Status monitoring */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        abus_net_stats_t stats;
        abus_net_get_stats(net, &stats);

        abus_net_ptp_state_t ptp;
        abus_net_get_ptp_state(net, &ptp);

        ESP_LOGI(TAG, "NET: tx=%lu rx=%lu audio_tx=%lu audio_rx=%lu seq_err=%lu late=%lu",
                 stats.pkts_tx, stats.pkts_rx, stats.audio_pkts_tx,
                 stats.audio_pkts_rx, stats.seq_errors, stats.late_packets);
        ESP_LOGI(TAG, "PTP: %s offset=%lldns delay=%lldns syncs=%lu",
                 ptp.is_grandmaster ? "GRANDMASTER" : "slave",
                 ptp.offset_ns, ptp.path_delay_ns, (unsigned long)ptp.sync_count);

        /* List discovered nodes */
        abus_net_node_t nodes[8];
        int n = abus_net_get_nodes(net, nodes, 8);
        for (int i = 0; i < n; i++) {
            ESP_LOGI(TAG, "  Node: \"%s\" (0x%08lx) %s",
                     nodes[i].name, (unsigned long)nodes[i].uid,
                     nodes[i].is_grandmaster ? "[GM]" : "");
        }
    }
}
