#ifndef TRAINING_H
#define TRAINING_H

#include <stdint.h>

#define FLASH_START ((uint8_t*) XIP_BASE)
#define DATA_FLASH_START (PICO_FLASH_SIZE_BYTES - 16 * FLASH_SECTOR_SIZE)
#define TOC_FLASH_START (PICO_FLASH_SIZE_BYTES - 17 * FLASH_SECTOR_SIZE)
#define META_FLASH_START (PICO_FLASH_SIZE_BYTES - 18 * FLASH_SECTOR_SIZE)

typedef struct {
    uint32_t start;
    uint32_t size;
} Block;

typedef struct {
    char magic[4]; // "MTFT"
    uint8_t version; // and other funny jokes you can tell yourself
    uint8_t block_count;
} Meta;

void train_firmware_cycle();
void replay_firmware_cycle();
uint8_t is_trained();

#endif
