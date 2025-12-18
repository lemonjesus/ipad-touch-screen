#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/malloc.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/spi.h>

#include "pins.h"
#include "multitouch/multitouch.h"
#include "power/power.h"
#include "training/training.h"
#include "debug_utils.h"
#include "usb/usb_hid.h"

#define TRUE 1
#define FALSE 0

static MTDevice z2;

int spi_txrx(const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen) {
    gpio_put(SPI_CS_PIN, 0);
	int read = spi_write_read_blocking(spi_default, outBuffer, inBuffer, inLen);
    gpio_put(SPI_CS_PIN, 1);
    return read;
}

int spi_txrx_nostop(const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen) {
    gpio_put(SPI_CS_PIN, 0);
	int read = spi_write_read_blocking(spi_default, outBuffer, inBuffer, inLen);
    return read;
}

void core1_entry() {
    multicore_fifo_push_blocking(0x69);

    while(true) {        
        // Show tracked finger information
        if (z2.active_finger_count > 0) {
            debugf("Active fingers: %d\n", z2.active_finger_count);
            for(int i = 0; i < 10; ++i) {
                if (z2.tracked_fingers[i].state != CONTACT_FREE) {
                    FingerData* finger = &z2.tracked_fingers[i].data;
                    debugf("  Tracked contact ID %d (hw_id=%d): state=%d abs=(%d,%d) touch=(%d,%d)\n",
                        z2.tracked_fingers[i].contact_id,
                        z2.tracked_fingers[i].hw_finger_id,
                        finger->state,
                        finger->abs_x,
                        finger->abs_y,
                        finger->touch_major,
                        finger->touch_minor
                    );
                }
            }
        }

        usb_hid_task(&z2);
    }
}

int main() {
    setup_default_uart();
    stdio_init_all();
    turn_led_on(); // show proof of life
    debug_pin_init();

    for(int i = 0; i < 15; ++i) {
        printf("\n");
    }
    printf("ALIVE!\n");

    // Initialize USB HID
    usb_hid_init();

    multicore_launch_core1(core1_entry);
    multicore_fifo_pop_blocking();

    z2.index_parity = 0;
    z2.tx_buf = malloc(8192);
    z2.rx_buf = malloc(8192);
    z2.spi_txrx = spi_txrx;
    z2.spi_txrx_nostop = spi_txrx_nostop;
    z2.active_finger_count = 0;
    
    // Initialize tracked fingers
    for (int i = 0; i < 10; i++) {
        z2.tracked_fingers[i].state = CONTACT_FREE;
        z2.tracked_fingers[i].contact_id = 0;
        z2.tracked_fingers[i].hw_finger_id = 0;
    }

    // 1. hold the iPad and multitouch system in reset for now
	// RESET_PIN holds the iPad in reset
    gpio_init_and_set(RESET_PIN, 1, 1); // hold the iPad in reset
    gpio_pull_down(RESET_PIN);

    debug_pin_mark();
    printf("Booting...\n");

    // If we haven't trained the firmware, do so now
	if(!is_trained()) train_firmware_cycle();

    // 2. initialize SPI and the PMU
    spi_init(spi_default, 12 * 1000 * 1000);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);

    gpio_init_and_set(SPI_CS_PIN, 1, 0);
    gpio_init_and_set(MT_SELECT, 1, 0);

    // MT_RESET_PIN holds the Z2 in reset
    gpio_init_and_set(MT_RESET_PIN, 1, 0); // hold the Z2 in reset
    gpio_pull_down(MT_RESET_PIN);
    
    pmu_init();

    // 3. bring up the multitouch system (power on, clock on)
    sleep_ms(50); // wait a bit
    pmu_write_register_with_retry(0x23, 0xFF, 10);
    gpio_put(SPI_CS_PIN, 1);
    pmu_write_register_with_retry(0x64, 0x51, 10);
    gpio_put(MT_RESET_PIN, 1); // take the MT system out of reset
    mt_wait_for_ready(&z2); // wait for the MT system to be ready

    // 4. replay the recorded boot data
    gpio_put(MT_SELECT, 0);
    replay_firmware_cycle();
    sleep_ms(45); // wait a bit to ensure the firmware is running
    gpio_put(MT_SELECT, 1);
    
    // 5. now we can talk to the multitouch system and recieve touch data
    for (size_t i = 0; ; ++i) {
        printf("multitouch: Determining interface version...\n");
        debug_pin_mark();
        mt_interface_version(&z2);
        mt_populate_reports(&z2);

        printf("Family ID                : 0x%x\n", z2.family_id);
        printf("Sensor rows              : 0x%x\n", z2.sensor_rows);
        printf("Sensor columns           : 0x%x\n", z2.sensor_columns);
        printf("Sensor width             : 0x%x\n", z2.sensor_width);
        printf("Sensor height            : 0x%x\n", z2.sensor_height);
        printf("BCD Version              : 0x%x\n", z2.bcd_version);
        printf("endianness               : 0x%x\n", z2.endianness);
        printf("Sensor region descriptor :");
        for(i = 0; i < z2.sensor_region_descriptor_len; ++i)
            printf(" %02x", z2.sensor_region_descriptor[i]);
        printf("\n");

        printf("Sensor region param      :");
        for(i = 0; i < z2.sensor_region_param_len; ++i)
            printf(" %02x", z2.sensor_region_param[i]);

        printf("\nwaiting to read frame for a bit...\n");
        sleep_ms(500);

        while(true) {
            while(gpio_get(MT_ATTN_PIN)); // wait for MT_ATTN_PIN to go low
                mt_read_frame(&z2);
        }
    }
}
