/*
 * AudioBus Ethernet Test — Waveshare ESP32-P4-NANO
 *
 * Flash this onto two (or more) ESP32-P4-NANO boards, plug them into
 * the same Ethernet switch, and watch them discover each other and
 * stream audio with PTP-synchronized timing.
 *
 * What happens automatically:
 *   1. Board boots, initializes onboard Ethernet PHY
 *   2. PTP grandmaster election (board with lowest MAC wins)
 *   3. Each board publishes a named stereo test stream
 *   4. Each board subscribes to the first remote stream it finds
 *   5. Latency, jitter, and packet stats are printed every 3 seconds
 *   6. GPIO tunnel: each board toggles a remote GPIO to prove tunneling works
 *   7. MIDI tunnel: each board sends a MIDI note-on every 2 seconds
 *
 * Hardware:
 *   - 2+ Waveshare ESP32-P4-NANO boards
 *   - Ethernet cables
 *   - Any Ethernet switch (unmanaged is fine)
 *   - No other wiring needed — Ethernet PHY is onboard
 *
 * Build & flash:
 *   cd examples/ethernet_node
 *   idf.py set-target esp32p4
 *   idf.py build
 *   idf.py -p /dev/ttyUSBx flash monitor
 *
 * To distinguish boards in the log, each board derives a unique name
 * from its MAC address (e.g., "P4-Nano-A3F2").
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
#include "esp_mac.h"
#include "esp_timer.h"
#include "audiobus_net.h"
#include "esp32_p4_nano_pinmap.h"

static const char *TAG = "abus_test";

/* ---------------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------------- */

static abus_net_handle_t    net = NULL;
static uint16_t             my_stream_id = 0;
static uint16_t             remote_stream_id = 0;
static uint32_t             remote_talker_uid = 0;
static volatile bool        subscribed = false;
static char                 board_name[32] = {0};

/* ---------------------------------------------------------------------------
 * Callbacks
 * --------------------------------------------------------------------------- */

static void on_node_discovered(const abus_net_node_t *node, void *ctx) {
    ESP_LOGI(TAG, "[DISCOVER] Node \"%s\" (uid=0x%08lX, type=%d, streams=%d) %s",
             node->name, (unsigned long)node->uid, node->hw_type, node->num_streams,
             node->is_grandmaster ? " ** GRANDMASTER **" : "");
}

static void on_node_lost(uint32_t uid, void *ctx) {
    ESP_LOGW(TAG, "[LOST] Node 0x%08lX disappeared", (unsigned long)uid);
    if (uid == remote_talker_uid) {
        subscribed = false;
        remote_stream_id = 0;
        remote_talker_uid = 0;
    }
}

static void on_stream_announced(const abus_net_stream_t *stream, void *ctx) {
    ESP_LOGI(TAG, "[STREAM] \"%s\" (id=0x%04X, %dch/%dbit/%luHz) from 0x%08lX",
             stream->name, stream->stream_id, stream->channels, stream->bit_depth,
             (unsigned long)stream->sample_rate, (unsigned long)stream->talker_uid);

    /* Auto-subscribe to the first remote stream */
    if (!subscribed && net && stream->talker_uid != 0) {
        esp_err_t ret = abus_net_subscribe(net, stream->talker_uid,
                                            stream->stream_id, 0);
        if (ret == ESP_OK) {
            remote_stream_id = stream->stream_id;
            remote_talker_uid = stream->talker_uid;
            subscribed = true;
            ESP_LOGI(TAG, "[SUBSCRIBE] Listening to \"%s\" from 0x%08lX",
                     stream->name, (unsigned long)stream->talker_uid);
        }
    }
}

/* MIDI receive callback */
static void on_midi_rx(uint32_t sender_uid, const uint8_t *data, uint8_t len, void *ctx) {
    if (len >= 1) {
        ESP_LOGI(TAG, "[MIDI RX] from 0x%08lX: %02X %02X %02X (%d bytes)",
                 (unsigned long)sender_uid,
                 data[0], len > 1 ? data[1] : 0, len > 2 ? data[2] : 0, len);
    }
}

/* Raw tunnel callback (for GPIO state updates) */
static void on_tunnel_rx(uint32_t sender_uid, abus_tunnel_type_t type,
                          const uint8_t *data, uint16_t len, void *ctx) {
    if (type == ABUS_TUNNEL_GPIO && len >= 2) {
        uint16_t pins = (data[0] << 8) | data[1];
        ESP_LOGD(TAG, "[GPIO RX] from 0x%08lX: 0x%04X", (unsigned long)sender_uid, pins);
    }
}

/* ---------------------------------------------------------------------------
 * Audio talker task — generate stereo sine wave
 * --------------------------------------------------------------------------- */

static void talker_task(void *arg) {
    float phase = 0.0f;
    const float sr = 48000.0f;
    const float inc = 2.0f * M_PI * 440.0f / sr;
    int32_t samples[2];

    /* Wait for stream to be created */
    while (my_stream_id == 0) vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "[TALKER] Streaming 440Hz sine on stream 0x%04X", my_stream_id);

    while (1) {
        float val = 0.25f * sinf(phase);
        int32_t s = (int32_t)(val * 2147483647.0f);
        samples[0] = s;         /* Left */
        samples[1] = -s;        /* Right (inverted — easy to verify on scope) */
        phase += inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        if (abus_net_stream_write(net, my_stream_id, samples, 1) == 0) {
            vTaskDelay(1);  /* Buffer full — back off */
        }
    }
}

/* ---------------------------------------------------------------------------
 * Audio listener task — read from subscribed stream
 * --------------------------------------------------------------------------- */

static void listener_task(void *arg) {
    int32_t samples[2];
    uint32_t sample_count = 0;

    while (1) {
        if (!subscribed || remote_stream_id == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int n = abus_net_stream_read(net, remote_stream_id, samples, 1);
        if (n > 0) {
            sample_count += n;
            /* Every 48000 samples (~1 sec), log a quick peek at the data */
            if ((sample_count % 48000) == 0) {
                ESP_LOGI(TAG, "[LISTENER] Received %lu frames, last L=%ld R=%ld",
                         (unsigned long)sample_count,
                         (long)samples[0], (long)samples[1]);
            }
        } else {
            vTaskDelay(1);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Tunnel test task — GPIO toggle + MIDI note-on every 2 seconds
 * --------------------------------------------------------------------------- */

static void tunnel_test_task(void *arg) {
    bool gpio_state = false;
    uint8_t midi_note = 60;     /* Middle C */

    vTaskDelay(pdMS_TO_TICKS(5000));  /* Let discovery settle */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (!subscribed || remote_talker_uid == 0) continue;

        /* Toggle GPIO pin 0 on the remote node */
        gpio_state = !gpio_state;
        abus_net_gpio_set(net, remote_talker_uid, 0, gpio_state);
        ESP_LOGI(TAG, "[GPIO TX] Pin 0 → %s on node 0x%08lX",
                 gpio_state ? "HIGH" : "LOW", (unsigned long)remote_talker_uid);

        /* Send MIDI note-on (channel 1, velocity 100) */
        uint8_t midi_msg[3] = { 0x90, midi_note, 100 };
        abus_net_midi_send(net, remote_talker_uid, midi_msg, 3);
        ESP_LOGI(TAG, "[MIDI TX] Note On: note=%d vel=100 → 0x%08lX",
                 midi_note, (unsigned long)remote_talker_uid);

        midi_note++;
        if (midi_note > 72) midi_note = 60;
    }
}

/* ---------------------------------------------------------------------------
 * Stats task — print latency, jitter, PTP state every 3 seconds
 * --------------------------------------------------------------------------- */

static void stats_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));

        /* Global stats */
        abus_net_stats_t stats;
        abus_net_get_stats(net, &stats);

        ESP_LOGI(TAG, "--- %s stats ---", board_name);
        ESP_LOGI(TAG, "  Packets: TX=%lu  RX=%lu  Audio TX=%lu  Audio RX=%lu",
                 stats.pkts_tx, stats.pkts_rx,
                 stats.audio_pkts_tx, stats.audio_pkts_rx);
        ESP_LOGI(TAG, "  Errors:  seq_err=%lu  late=%lu",
                 stats.seq_errors, stats.late_packets);

        /* PTP state */
        abus_net_ptp_state_t ptp;
        abus_net_get_ptp_state(net, &ptp);
        ESP_LOGI(TAG, "  PTP: %s  offset=%lldns  delay=%lldns  syncs=%lu",
                 ptp.is_grandmaster ? "GRANDMASTER" : "slave",
                 ptp.offset_ns, ptp.path_delay_ns,
                 (unsigned long)ptp.sync_count);

        /* Per-stream latency (if subscribed) */
        if (subscribed && remote_stream_id != 0) {
            abus_net_stream_latency_t sl;
            if (abus_net_get_stream_latency(net, remote_stream_id, &sl) == ESP_OK) {
                ESP_LOGI(TAG, "  Stream 0x%04X latency:", sl.stream_id);
                ESP_LOGI(TAG, "    Total: %ldus  Network: %ldus  Buffer: %ldus",
                         (long)sl.total_latency_us, (long)sl.network_latency_us,
                         (long)sl.buffer_depth_us);
                ESP_LOGI(TAG, "    Jitter: RMS=%ldns  Peak=%ldns  [%ld..%ld]ns",
                         (long)sl.jitter_rms_ns, (long)sl.jitter_peak_ns,
                         (long)sl.jitter_min_ns, (long)sl.jitter_max_ns);
                ESP_LOGI(TAG, "    Packets: rx=%lu  lost=%lu  late=%lu  loss=%.4f%%",
                         sl.packets_received, sl.packets_lost, sl.packets_late,
                         sl.loss_ratio * 100.0f);
            }
        }

        /* Ping remote node */
        if (remote_talker_uid != 0) {
            abus_net_node_latency_t nl;
            if (abus_net_ping(net, remote_talker_uid, 50, &nl) == ESP_OK) {
                ESP_LOGI(TAG, "  Ping \"%s\" (0x%08lX):", nl.node_name,
                         (unsigned long)nl.node_uid);
                ESP_LOGI(TAG, "    RTT: %ldus  avg=%ldus  [%ld..%ld]us  jitter=%ldus",
                         (long)nl.roundtrip_us, (long)nl.roundtrip_avg_us,
                         (long)nl.roundtrip_min_us, (long)nl.roundtrip_max_us,
                         (long)nl.roundtrip_jitter_us);
                ESP_LOGI(TAG, "    PTP one-way: %ldns  pings=%lu timeouts=%lu",
                         (long)nl.ptp_oneway_ns, nl.ping_count, nl.ping_timeouts);
            }
        }

        /* List all discovered nodes */
        abus_net_node_t nodes[8];
        int n = abus_net_get_nodes(net, nodes, 8);
        if (n > 0) {
            ESP_LOGI(TAG, "  Network: %d node%s", n, n > 1 ? "s" : "");
            for (int i = 0; i < n; i++) {
                ESP_LOGI(TAG, "    \"%s\" (0x%08lX) type=%d %s",
                         nodes[i].name, (unsigned long)nodes[i].uid,
                         nodes[i].hw_type,
                         nodes[i].is_grandmaster ? "[GM]" : "");
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Ethernet initialization — Waveshare ESP32-P4-NANO onboard PHY
 * --------------------------------------------------------------------------- */

/*
 * Waveshare ESP32-P4-NANO RMII pin definitions (IP101GRI PHY):
 *
 *   GPIO34  → TXD0       GPIO35  → TXD1
 *   GPIO29  → RXD0       GPIO30  → RXD1
 *   GPIO49  → TX_EN
 *   GPIO28  → CRS_DV
 *   GPIO50  → REF_CLK    (50 MHz, doubled from 25 MHz external crystal)
 *   GPIO52  → MDIO       GPIO31  → MDC
 *   GPIO51  → PHY_RESET
 *
 *   PHY address: 1
 */
#define P4NANO_ETH_TXD0        34
#define P4NANO_ETH_TXD1        35
#define P4NANO_ETH_RXD0        29
#define P4NANO_ETH_RXD1        30
#define P4NANO_ETH_TX_EN       49
#define P4NANO_ETH_CRS_DV      28
#define P4NANO_ETH_REF_CLK     50
#define P4NANO_ETH_MDIO        52
#define P4NANO_ETH_MDC         31
#define P4NANO_ETH_PHY_RST     51
#define P4NANO_ETH_PHY_ADDR     1

static esp_eth_handle_t init_ethernet(void) {
    esp_netif_init();
    esp_event_loop_create_default();

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    /* P4-NANO RMII pin assignments (required for ESP32-P4 rev 1.0+) */
    emac_config.smi_gpio.mdc_num  = P4NANO_ETH_MDC;
    emac_config.smi_gpio.mdio_num = P4NANO_ETH_MDIO;

    /* Explicitly set RMII data interface GPIOs (not optional on P4 rev 1.0+) */
    emac_config.emac_dataif_gpio.rmii.tx_en_num  = P4NANO_ETH_TX_EN;
    emac_config.emac_dataif_gpio.rmii.txd0_num   = P4NANO_ETH_TXD0;
    emac_config.emac_dataif_gpio.rmii.txd1_num   = P4NANO_ETH_TXD1;
    emac_config.emac_dataif_gpio.rmii.crs_dv_num = P4NANO_ETH_CRS_DV;
    emac_config.emac_dataif_gpio.rmii.rxd0_num   = P4NANO_ETH_RXD0;
    emac_config.emac_dataif_gpio.rmii.rxd1_num   = P4NANO_ETH_RXD1;

    /* REF_CLK: 50 MHz from PHY (external input to ESP32-P4 EMAC) */
    emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    emac_config.clock_config.rmii.clock_gpio = P4NANO_ETH_REF_CLK;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = P4NANO_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = P4NANO_ETH_PHY_RST;

    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* Attach to TCP/IP stack so normal IP traffic works alongside AudioBus */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "Ethernet initializing (IP101GRI on RMII, PHY addr=%d, RST=GPIO%d)...",
             P4NANO_ETH_PHY_ADDR, P4NANO_ETH_PHY_RST);
    vTaskDelay(pdMS_TO_TICKS(2000));

    return eth_handle;
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

void app_main(void) {
    /* Generate a unique board name from MAC address */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    snprintf(board_name, sizeof(board_name), "P4-Nano-%02X%02X", mac[4], mac[5]);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  AudioBus Ethernet Test");
    ESP_LOGI(TAG, "  Board: %s", board_name);
    ESP_LOGI(TAG, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "========================================");

    /* 1. Init Ethernet */
    esp_eth_handle_t eth = init_ethernet();

    /* 2. Init AudioBus network transport */
    abus_net_config_t config = {
        .uid = 0,                               /* Auto from MAC */
        .hw_type = 3,                           /* speaker + mic */
        .ptp_priority = 128,                    /* Equal — lowest MAC wins GM */
        .ptp_clock_class = 248,                 /* Freerun */
        .default_sample_rate = 48000,
        .default_bit_depth = 32,
        .default_packet_interval_us = 1000,     /* 1ms = 48 samples */
        .presentation_latency_us = 2000,        /* 2ms playout buffer */
        .dscp = 46,                             /* EF priority */
        .vlan_id = 0,
        .on_node_discovered = on_node_discovered,
        .on_node_lost = on_node_lost,
        .on_stream_announced = on_stream_announced,
    };
    strncpy(config.name, board_name, sizeof(config.name) - 1);

    ESP_ERROR_CHECK(abus_net_init(&config, &net));
    abus_net_attach_eth(net, eth);

    /* Register tunnel callbacks */
    abus_net_tunnel_register(net, on_tunnel_rx, NULL);
    abus_net_midi_register(net, on_midi_rx, NULL);

    ESP_ERROR_CHECK(abus_net_start(net));

    /* 3. Create our published audio stream */
    char stream_name[48];
    snprintf(stream_name, sizeof(stream_name), "%s Stereo", board_name);
    const char *labels[] = {"Left", "Right"};

    ESP_ERROR_CHECK(abus_net_stream_create(net, stream_name, 2, 48000, 32, 1000,
                                            &my_stream_id));
    abus_net_stream_set_labels(net, my_stream_id, labels, 2);

    /* Set some metadata */
    abus_net_set_metadata(net, "firmware", "audiobus-test-v1");
    abus_net_set_metadata(net, "board", "waveshare-esp32-p4-nano");
    abus_net_stream_set_metadata(net, my_stream_id, "purpose", "test-tone-440hz");

    ESP_LOGI(TAG, "Publishing: \"%s\" (stream 0x%04X)", stream_name, my_stream_id);
    ESP_LOGI(TAG, "Waiting for other nodes on the network...");

    /* 4. Start worker tasks */
    xTaskCreate(talker_task,      "talker",   4096, NULL, 10, NULL);
    xTaskCreate(listener_task,    "listener", 4096, NULL, 10, NULL);
    xTaskCreate(tunnel_test_task, "tunnel",   4096, NULL, 5,  NULL);
    xTaskCreate(stats_task,       "stats",    8192, NULL, 3,  NULL);

    /* Main task can do nothing — everything runs in the worker tasks */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
