#ifndef POWER_H
#define POWER_H

uint8_t pmu_write_register(uint8_t reg, uint8_t value);
void pmu_write_register_with_retry(uint8_t reg, uint8_t value, int max_retries);
void pmu_init();

#endif
