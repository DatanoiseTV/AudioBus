/**
 * @file esp32_p4_nano_pinmap.h
 * @brief Usable GPIO pin map for Waveshare ESP32-P4-NANO
 *
 * Maximises available GPIOs from the two 2x13 headers.
 * Touch / ADC alternate functions are noted but do NOT prevent GPIO use.
 *
 * Reserved / excluded:
 *   - I2C bus     : GPIO7 (SDA), GPIO8 (SCL)
 *   - USB         : GPIO24, GPIO25, GPIO26, GPIO27
 *   - XTAL 32 K   : GPIO0, GPIO1
 *   - UART0       : GPIO37, GPIO38
 *   - ESP32-C6    : C6_IO9, C6_IO12, C6_IO13, C6_U0RXD, C6_U0TXD
 *   - Power / GND : 3V3, 5V, GND, ESP_LDO_VO4
 */

#ifndef ESP32_P4_NANO_PINMAP_H
#define ESP32_P4_NANO_PINMAP_H

/* Flat enum - every usable GPIO (18 total) */
typedef enum {
    /* Left header  (9 pins) */
    P4N_GPIO4          =  4,
    P4N_GPIO5          =  5,
    P4N_GPIO20         = 20,
    P4N_GPIO21         = 21,
    P4N_GPIO22         = 22,
    P4N_GPIO23         = 23,
    P4N_GPIO32         = 32,
    P4N_GPIO33         = 33,
    P4N_GPIO36         = 36,

    /* Right header (9 pins) */
    P4N_GPIO2          =  2,
    P4N_GPIO3          =  3,
    P4N_GPIO6          =  6,
    P4N_GPIO45         = 45,
    P4N_GPIO46         = 46,
    P4N_GPIO47         = 47,
    P4N_GPIO48         = 48,
    P4N_GPIO53         = 53,
    P4N_GPIO54         = 54,
} esp32_p4_nano_pin_t;

#define ESP32_P4_NANO_USABLE_GPIO_COUNT  18

/* Reserved I2C bus */
typedef enum {
    P4N_I2C_SDA        =  7,
    P4N_I2C_SCL        =  8,
} esp32_p4_nano_i2c_pin_t;

#endif /* ESP32_P4_NANO_PINMAP_H */
