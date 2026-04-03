/*
 * AudioBus — PHY abstraction layer
 *
 * Primary physical layer: LVDS SerDes (DS92LV1021A + DS92LV1212A)
 *   - ESP32-P4 PARLIO TX (10-bit @ 49.152MHz) → DS92LV1021A serializer → LVDS
 *   - LVDS → DS92LV1212A deserializer (CDR recovers clock) → PARLIO RX
 *   - Half-duplex on single twisted pair: serializer OE pin controls direction
 *   - Daisy-chain: each node has 2 port pairs (upstream + downstream)
 *   - Self-clocking: deserializer CDR PLL locks to 8b10b transitions
 *   - ESP32-P4 EMAC is NOT used — free for normal Ethernet/WiFi
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "audiobus_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PHY driver vtable */

typedef struct abus_phy_ops {
    /**
     * Initialize PHY hardware (PARLIO, GPIO for OE/LOCK, clock source).
     * Called once during abus_init().
     */
    esp_err_t (*init)(void *phy_ctx);

    /**
     * Start PHY — enable transceivers, start DMA, begin sending idle (K28.3).
     */
    esp_err_t (*start)(void *phy_ctx);

    /**
     * Stop PHY — disable transceivers, stop DMA.
     */
    esp_err_t (*stop)(void *phy_ctx);

    /**
     * Deinitialize PHY, free resources.
     */
    esp_err_t (*deinit)(void *phy_ctx);

    /**
     * Submit a frame for transmission on a specific port.
     * @param port  0 = upstream, 1 = downstream
     * @param data  Frame data bytes (will be 8b10b encoded by PHY driver)
     * @param len   Frame length in bytes
     *
     * The PHY driver prepends K28.5+K28.1 sync, appends CRC,
     * 8b10b-encodes everything, and DMA's the 10-bit symbol stream
     * to the DS92LV1021A via PARLIO TX.
     */
    esp_err_t (*tx_frame)(void *phy_ctx, uint8_t port, const uint8_t *data, uint16_t len);

    /**
     * Register callback for received frames.
     * @param port  0 = upstream, 1 = downstream
     * @param cb    Called from DMA ISR context with decoded frame bytes
     */
    esp_err_t (*set_rx_callback)(void *phy_ctx, uint8_t port,
                                 void (*cb)(uint8_t port, const uint8_t *data,
                                           uint16_t len, void *arg),
                                 void *arg);

    /**
     * Check if a link is active on a given port.
     * Reads DS92LV1212A LOCK pin — high = CDR PLL locked = link active.
     */
    bool (*link_status)(void *phy_ctx, uint8_t port);

    /**
     * Set half-duplex direction on a port.
     * @param port  0 = upstream, 1 = downstream
     * @param tx    true = enable DS92LV1021A output (transmit)
     *              false = disable output (high-Z), listen via DS92LV1212A
     */
    esp_err_t (*set_direction)(void *phy_ctx, uint8_t port, bool tx);

    /**
     * Get recovered clock frequency in Hz (from DS92LV1212A RCLK).
     * Returns 0 if CDR not locked.
     * Slave nodes use this to phase-lock their downstream serializer clock
     * to the upstream recovered clock — propagating master timing through chain.
     */
    uint32_t (*get_recovered_clock)(void *phy_ctx, uint8_t port);

} abus_phy_ops_t;

typedef struct {
    const abus_phy_ops_t *ops;
    void *ctx;
} abus_phy_t;

/**
 * Create single-chip LVDS transceiver PHY (SN65LVDT41). RECOMMENDED.
 * 1-bit PARLIO at 98.304 MHz. 5 GPIO pins per port. ~$2/port.
 * Up to 25 channels/direction at 48 kHz/32-bit.
 *
 * @param pin_config  Pointer to oneic_pins section of abus_pin_config_t
 * @param sr          Sample rate
 * @param role        Master uses crystal osc; slave uses Si5351A + software PLL
 */
esp_err_t abus_phy_oneic_create(const void *pin_config, abus_sample_rate_t sr,
                                abus_role_t role, abus_phy_t *out_phy);

/**
 * Create 10:1 LVDS SerDes PHY (DS92LV1021A + DS92LV1212A). Maximum performance.
 * 16-bit PARLIO at 49.152 MHz. 24 GPIO pins per port. ~$7/port.
 * Up to 64 channels/direction at 48 kHz/32-bit. Hardware CDR self-clocking.
 *
 * @param pin_config  Pointer to serdes_pins section of abus_pin_config_t
 * @param sr          Sample rate
 * @param role        Master generates clock from crystal; slave uses CDR recovered clock
 */
esp_err_t abus_phy_lvds_create(const void *pin_config, abus_sample_rate_t sr,
                               abus_role_t role, abus_phy_t *out_phy);

#ifdef __cplusplus
}
#endif
