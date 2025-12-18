#ifndef PINS_H
#define PINS_H

#define PIO_INPUT_PIN_BASE 3

#define SPI_CS_PIN 1
#define SPI_SCK_PIN 2
#define SPI_MOSI_PIN 3
#define SPI_MISO_PIN 4

#define MT_RESET_PIN 5
#define MT_ATTN_PIN 6
#define MT_SELECT 7

#define RESET_PIN 10

#define PMU_I2C_SDA 12
#define PMU_I2C_SCL 13

#define DEBUG_INDICATOR 14

void turn_led_on();
void debug_pin_init();
void debug_pin_mark();
void debug_pin_unmark();
void gpio_init_and_set(uint gpio, bool out, bool value);

#endif