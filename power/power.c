#include <stdio.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>

#include "../pins.h"
#include "power.h"

uint8_t pmu_read_register(uint8_t reg) {
    uint8_t data;
    i2c_write_blocking(i2c_default, 0x78 >> 1, &reg, 1, false);
    i2c_read_blocking(i2c_default, 0x79 >> 1, &data, 1, false);
    return data;
}

uint8_t pmu_write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(i2c_default, 0x78 >> 1, data, 2, false);
    sleep_us(145);
    uint8_t verify = pmu_read_register(reg);
    if (verify != value) {
        printf("pmu_write_register: failed to verify write to register %d: wrote %d, read %d\n", reg, value, verify);
    }
    sleep_ms(10);
    return (verify != value);
}

void pmu_write_register_with_retry(uint8_t reg, uint8_t value, int max_retries) {
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        if (pmu_write_register(reg, value) == 0) {
            return; // Success
        }
        printf("pmu_write_register_with_retry: retrying write to register %d (attempt %d)\n", reg, attempt + 1);
        sleep_ms(10);
    }
    printf("pmu_write_register_with_retry: failed to write to register %d after %d attempts\n", reg, max_retries);
}

void pmu_init() {
    i2c_init(i2c_default, 300 * 1000);
    gpio_set_function(PMU_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PMU_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PMU_I2C_SDA);
    gpio_pull_up(PMU_I2C_SCL);

    sleep_ms(10);
}
