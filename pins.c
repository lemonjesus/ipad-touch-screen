#include <hardware/gpio.h>
#include "pins.h"

void turn_led_on() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

void debug_pin_init() {
    gpio_init(DEBUG_INDICATOR);
    gpio_set_dir(DEBUG_INDICATOR, 1);
    gpio_put(DEBUG_INDICATOR, 0);
}

void debug_pin_mark() {
    gpio_put(DEBUG_INDICATOR, 1);
}

void debug_pin_unmark() {
    gpio_put(DEBUG_INDICATOR, 0);
}

void gpio_init_and_set(uint gpio, bool out, bool value) {
    gpio_init(gpio);
    gpio_set_dir(gpio, out);
    gpio_put(gpio, value);
}
