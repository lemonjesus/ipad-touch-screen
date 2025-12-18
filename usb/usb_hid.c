#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/bootrom.h>

#include "pico/stdlib.h"
#include "tusb.h"
#include "multitouch/multitouch.h"
#include "../debug_utils.h"

typedef struct __attribute__((packed)) {
    uint8_t status;        // Bit 0: Tip Switch, Bit 1: In Range
    uint8_t contact_id;    // Contact identifier
    uint16_t x;            // X position (0-32767)
    uint16_t y;            // Y position (0-32767)
} hid_finger_report_t;

typedef struct __attribute__((packed)) {
    hid_finger_report_t fingers[MAX_FINGERS];
    uint8_t contact_count; // Number of active contacts
} hid_multitouch_report_t;

// Invoked when received GET_REPORT control request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;

    if (report_type == HID_REPORT_TYPE_FEATURE && report_id == 2) {
        if (reqlen > 0) {
            debugf("GET_REPORT: Feature Report ID 2 requested\n");
            buffer[0] = MAX_FINGERS; // Report the maximum number of contacts
            return 1;
        }
    } else if (report_type == HID_REPORT_TYPE_FEATURE && report_id == 69) {
        // a secret report to reboot into bootloader mode in case no buttons are available
        // sudo ./hidapitester --vidpid 2e8a:4001 --open --read-feature 69 --length 0
        reset_usb_boot(0, 0);
        return 0;
    }

  return 0; // Stall for other requests
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {}

uint32_t last_send_time = 0;
uint8_t last_finger_count = 0;
uint8_t pending_release_sent[10] = {0}; // Track which pending releases have been sent

void usb_hid_init(void) {
    printf("Initializing USB HID...\n");
    tusb_init();
    printf("TinyUSB initialized\n");
}

void usb_hid_task(MTDevice* z2) {
    tud_task();
        
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (tud_mounted() && tud_hid_ready()) {
        hid_multitouch_report_t report;
        memset(&report, 0, sizeof(report));
        
        uint8_t report_index = 0;
        uint8_t active_count = 0;
        uint8_t pending_count = 0;
        
        // First pass: count active and pending contacts
        for (uint8_t i = 0; i < 10; i++) {
            if (z2->tracked_fingers[i].state == CONTACT_ACTIVE) {
                active_count++;
            } else if (z2->tracked_fingers[i].state == CONTACT_PENDING_RELEASE) {
                pending_count++;
            }
        }
        
        for (uint8_t i = 0; i < 10 && report_index < MAX_FINGERS; i++) {
            FingerData* finger = &z2->tracked_fingers[i].data;
            report.fingers[report_index].contact_id = z2->tracked_fingers[i].contact_id;

            // Scale coordinates
            int16_t raw_x = finger->abs_x;
            int16_t raw_y = finger->abs_y;
            uint16_t scaled_x = (z2->sensor_height > 0) ? (uint16_t)(((z2->sensor_height - raw_y) * 32767) / z2->sensor_height) : 0;
            uint16_t scaled_y = (z2->sensor_width > 0) ? (uint16_t)(((z2->sensor_width - raw_x) * 32767) / z2->sensor_width) : 0;
            if (scaled_x > 32767) scaled_x = 32767;
            if (scaled_y > 32767) scaled_y = 32767;
            
            report.fingers[report_index].x = scaled_x;
            report.fingers[report_index].y = scaled_y;
            
            
            if (z2->tracked_fingers[i].state == CONTACT_ACTIVE) {
                report.fingers[report_index].status = 0x03; // Tip switch ON, In Range
            } else if (z2->tracked_fingers[i].state == CONTACT_PENDING_RELEASE) {
                report.fingers[report_index].status = 0x00; // Tip switch OFF, In Range
                pending_release_sent[i] = 1; // Mark as sent
            }

            report_index++;
        }

        report.contact_count = report_index;
        
        // Send report if there are any contacts (active or pending release) or if count changed
        if (report_index > 0 || report_index != last_finger_count) {
            bool success = tud_hid_report(1, &report, sizeof(report));
            
            if (success) {
                // After successfully sending pending releases, free those slots
                for (uint8_t i = 0; i < 10; i++) {
                    if (z2->tracked_fingers[i].state == CONTACT_PENDING_RELEASE && pending_release_sent[i]) {
                        z2->tracked_fingers[i].state = CONTACT_FREE;
                        pending_release_sent[i] = 0;
                        debugf("  Freed contact ID %d (hw_id=%d)\n", z2->tracked_fingers[i].contact_id, z2->tracked_fingers[i].hw_finger_id);
                    }
                }
                
                // Recount after freeing
                z2->active_finger_count = 0;
                for (int i = 0; i < 10; i++) {
                    if (z2->tracked_fingers[i].state != CONTACT_FREE) z2->active_finger_count++;
                }
            }
            
            last_finger_count = report_index;
            last_send_time = current_time;
        } else if (active_count > 0 && (current_time - last_send_time) >= 10) {
            // Send periodic updates for active touches (every 10ms)
            tud_hid_report(1, &report, sizeof(report));
            last_send_time = current_time;
        }
    } else {
        if (!tud_mounted()) {
            // Wait for USB to be mounted
            if (current_time - last_send_time >= 1000) {
                printf("Waiting for USB connection...\n");
                last_send_time = current_time;
            }
        }
    }
    
    // Small delay to avoid busy-waiting (1ms for better responsiveness)
    sleep_ms(1);
}
