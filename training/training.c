#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/bootrom.h>
#include <pico/malloc.h>
#include <pico/stdlib.h>
#include <hardware/dma.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/structs/bus_ctrl.h>
#include <hardware/pio.h>
#include <hardware/spi.h>

#include "training.h"
#include "clocked_input.pio.h"
#include "../pins.h"
#include "../debug_utils.h"

void train_firmware_cycle() {
	printf("==== BEGIN TRAINING ====\n");
    if(is_trained()) {
        printf("*Firmware is already trained, but we are training again.\n");
    }
	// step 0: this process requires different GPIO state and a PIO
    // like setting the SPI selector to be an input so we know when to start and stop recording
    gpio_init_and_set(MT_SELECT, 0, 0);
    gpio_init_and_set(SPI_CS_PIN, 0, 0);

    // set up DMA and PIO
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

	PIO pio = pio0;
	uint offset = pio_add_program(pio, &clocked_input_program);
	uint sm = pio_claim_unused_sm(pio, true);

    uint dma_chan = 0;

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    // allocate the required memory to record the data
    Block* toc = (Block*)malloc(32*sizeof(Block));
    uint8_t* data = (uint8_t*)malloc(64*1024);
    uint8_t block_count = 0;

    // step 1: reset the iPad
    printf("Resetting the iPad to begin training...\n");
    gpio_put(RESET_PIN, 1);
	sleep_ms(100);
	gpio_put(RESET_PIN, 0);

	// step 2: wait for a short amount of time to skip over noise, and then wait for MT_SELECT to go low
    printf("Training... (this could take up to 30 seconds)\n");
	sleep_ms(3*1000);
	while(gpio_get(MT_SELECT));

	// ensure CS is high before moving on
	while(!gpio_get(SPI_CS_PIN));

    // start the DMA transfer and the PIO at the same time
    dma_channel_configure(dma_chan, &c,
        data,               // Destination pointer
        &pio->rxf[sm],      // Source pointer
        64*1024,            // Number of transfers
        true                // Start immediately
    );
    clocked_input_program_init(pio, sm, offset, SPI_MOSI_PIN);

    // DMA will throw data into our buffer. We need to keep track of each block of data.
    bool done = false;
    while(!done) {
        // wait for CS to go low, marking a new block of data
		while(gpio_get(SPI_CS_PIN));
        toc[block_count].start = (64*1024) - dma_channel_hw_addr(dma_chan)->transfer_count;

        // wait for CS to go high, marking the end of the block.
        while(!gpio_get(SPI_CS_PIN))
        toc[block_count].size = (64*1024) - dma_channel_hw_addr(dma_chan)->transfer_count - toc[block_count].start;

        if(data[toc[block_count].start] == 0x1D && data[toc[block_count].start+1] == 0x53) break;
		block_count++;
    }
    dma_channel_abort(dma_chan);
    block_count++;

    for(int i = 0; i < block_count; i++) {
        debugf("Recorded block %d of %d bytes\n", i, toc[i].size);
    }

    printf("Training complete! Saving... ");
    
    // calculate where to put everything in flash:
    // ---|-----------------------|------------------|---------------------||
    // ...| 1 sector for metadata | 1 sector for toc | 16 sectors for data || <- flash end
    // ---|-----------------------|------------------|---------------------||
    
    Meta* meta = (Meta*)malloc(FLASH_PAGE_SIZE);
    memset(meta, 0xFF, FLASH_PAGE_SIZE);
    memcpy(meta->magic, "MTFT", 4);
    meta->version = 1;
    meta->block_count = block_count;

    // save data
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(META_FLASH_START, 18 * FLASH_SECTOR_SIZE);
    flash_range_program(META_FLASH_START, (uint8_t*)meta, FLASH_PAGE_SIZE);
    flash_range_program(TOC_FLASH_START, (uint8_t*)toc, FLASH_SECTOR_SIZE);
    flash_range_program(DATA_FLASH_START, data, 16 * FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    printf("done!\nverifying save data... ");
    if(memcmp((void*)FLASH_START + META_FLASH_START, meta, sizeof(Meta)) || memcmp((void*)FLASH_START + TOC_FLASH_START, (uint8_t*)toc, 32*sizeof(Block)) || memcmp((void*)FLASH_START + DATA_FLASH_START, data, 64*1024)) {
        printf("error!\nThere was an error verifying flash. Exiting...\n");
        while(1);
    } else {
        printf("done!\nTraining is complete, resetting the iPad so we can continue with the boot process...\n");
        gpio_put(RESET_PIN, 1);
    }

    free(meta);
    free(toc);
    free(data);
    sleep_ms(100);
}

void replay_firmware_cycle() {
    gpio_put(MT_SELECT, 0);

    Meta* meta = (Meta*)(FLASH_START + META_FLASH_START);
    Block* toc = (Block*)(FLASH_START + TOC_FLASH_START);
    uint8_t* data = (uint8_t*)(FLASH_START + DATA_FLASH_START);

    printf("About to write %d blocks...\n", meta->block_count);

    for(int i = 5; i < meta->block_count; i++) {
        sleep_ms(100);
        debugf("Writing %d Bytes.\n", toc[i].size);
        gpio_put(SPI_CS_PIN, 0);
        spi_write_blocking(spi_default, data + toc[i].start, toc[i].size);
        gpio_put(SPI_CS_PIN, 1);
        sleep_ms(10);
    }
    gpio_put(MT_SELECT, 1);

    sleep_ms(100);
}

uint8_t is_trained() {
    Meta* meta = (Meta*)(FLASH_START + META_FLASH_START);
    if(memcmp(meta->magic, "MTFT", 4) == 0) {
        debugf("Firmware is trained.\n");
        return 1;
    } else {
        debugf("Firmware is NOT trained. Please run the training process.\n");
        return 0;
    }
}