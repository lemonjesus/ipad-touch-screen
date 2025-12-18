#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#include "multitouch.h"
#include "../pins.h"
#include "../debug_utils.h"

void mt_wait_for_ready(MTDevice* z2) {
	const uint8_t outBuffer[16] = {0x1A, 0xA1, 0x18, 0xE1, 0x18, 0xE1, 0x18, 0xE1, 0x18, 0xE1, 0x18, 0xE1, 0x18, 0xE1, 0x18, 0xE1};
    uint8_t inBuffer[16] = {0};
    debugf("multitouch: waiting for chip to be ready");
    while(true) {
        gpio_put(MT_SELECT, 0);
        z2->spi_txrx(outBuffer, sizeof(outBuffer), inBuffer, sizeof(inBuffer));
        gpio_put(MT_SELECT, 1);
        if (inBuffer[2] == 0x48 && inBuffer[3] == 0x79) {
            debugf("\nmultitouch: chip is ready!\n");
            break;
        } else {
            debugf(".");
            sleep_ms(5);
        }
    }
    sleep_ms(50);
}

int mt_get_report_info(MTDevice *z2, int id, uint8_t* err, uint16_t* len) {
	uint32_t checksum = 0;
	uint8_t i, try;

	for(try = 0; try < 4; ++try) {
		memset(z2->tx_buf, 0, 16);

		z2->tx_buf[0] = MT_CMD_GET_REPORT_INFO;
		z2->tx_buf[1] = id;

		checksum = 0;
		for(i = 0; i < 14; ++i) checksum += z2->tx_buf[i];
		z2->tx_buf[14] = checksum & 0xFF;
		z2->tx_buf[15] = (checksum >> 8) & 0xFF;

		z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);

		sleep_ms(5);

		z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);

		if(z2->rx_buf[0] != MT_CMD_GET_REPORT_INFO)
			continue;

		checksum = 0;
		for(i = 0; i < 14; ++i) checksum += z2->rx_buf[i];

		if((z2->rx_buf[14] | (z2->rx_buf[15] << 8)) != checksum) continue;

		*err = z2->rx_buf[2];
		*len = (z2->rx_buf[4] << 8) | z2->rx_buf[3];

		return TRUE;
	}

	return FALSE;
}

int mt_short_control_read(MTDevice *z2, int id, uint8_t* buffer, int size) {
	uint32_t checksum = 0;
	uint8_t i;

	memset(z2->tx_buf, 0, 0x200);
	z2->tx_buf[0] = MT_CMD_SHORT_CONTROL_READ;
	z2->tx_buf[1] = id;
	z2->tx_buf[2] = 0;
	z2->tx_buf[3] = size & 0xFF;
	z2->tx_buf[4] = (size >> 8) & 0xFF;

	for(i = 0; i < 5; ++i) checksum += z2->tx_buf[i];

	z2->tx_buf[14] = checksum & 0xFF;
	z2->tx_buf[15] = (checksum >> 8) & 0xFF;
	z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);

	sleep_ms(5);

	z2->tx_buf[2] = 1;
	z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);

	checksum = 0;
	for(i = 0; i < 14; ++i) checksum += z2->rx_buf[i];

	if((z2->rx_buf[14] | (z2->rx_buf[15] << 8)) != checksum) {
        debugf("longControlRead checksum didn't match!\n");
		return FALSE;
    }

	memcpy(buffer, &z2->rx_buf[3], size);
	return TRUE;
}

int mt_long_control_read(MTDevice *z2, int id, uint8_t* buffer, int size) {
	uint32_t checksum = 0;
	int i;

	memset(z2->tx_buf, 0, 0x200);
	z2->tx_buf[0] = MT_CMD_LONG_CONTROL_READ;
	z2->tx_buf[1] = id;
	z2->tx_buf[2] = 0;
	z2->tx_buf[3] = size & 0xFF;
	z2->tx_buf[4] = (size >> 8) & 0xFF;

	for(i = 0; i < 5; ++i) checksum += z2->tx_buf[i];
	z2->tx_buf[14] = checksum & 0xFF;
	z2->tx_buf[15] = (checksum >> 8) & 0xFF;

	z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);
	sleep_ms(5);

	z2->tx_buf[2] = 1;
	z2->tx_buf[14] = 0;
	z2->tx_buf[15] = 0;
	z2->tx_buf[3 + size] = checksum & 0xFF;
	z2->tx_buf[3 + size + 1] = (checksum >> 8) & 0xFF;

	z2->spi_txrx(z2->tx_buf, size + 5, z2->rx_buf, size + 5);

	checksum = 0;
	for(i = 0; i < (size + 3); ++i)
		checksum += z2->rx_buf[i];

	if((z2->rx_buf[3 + size] | (z2->rx_buf[3 + size + 1] << 8)) != checksum) {
        debugf("longControlRead checksum didn't match!\n");
        debugf("\tgot back ");
        for(i = 0; i < (size + 5); ++i) debugf("%02X", z2->rx_buf[i]);
        debugf("\n\tchecksum mismatch! %X (actual) != %X (expected)\n", (z2->rx_buf[3 + size] | (z2->rx_buf[3 + size + 1] << 8)), checksum);
		return FALSE;
    }

	memcpy(buffer, &z2->rx_buf[3], size);
	return TRUE;
}

int mt_get_report(MTDevice *z2, int id, uint8_t* buffer, int* outLen) {
	uint8_t err;
	uint16_t len;
	if(!mt_get_report_info(z2, id, &err, &len)) return FALSE;

	if(err) return FALSE;

    sleep_ms(5);

	*outLen = len;

	int try = 0;
	for(try = 0; try < 4; ++try) {
		if(len < 12) {
			if(mt_short_control_read(z2, id, buffer, len)) return TRUE;
		} else {
			if(mt_long_control_read(z2, id, buffer, len)) return TRUE;
		}
	}

	return FALSE;
}

int mt_interface_version(MTDevice *z2) {
	uint8_t i, try;
	uint32_t checksum = 0;

	memset(z2->tx_buf, 0, 16);

	z2->tx_buf[0] = MT_CMD_GET_INTERFACE_VERSION;

	for(i = 0; i < 14; ++i) checksum += z2->tx_buf[i];
	z2->tx_buf[14] = checksum & 0xFF;
	z2->tx_buf[15] = (checksum >> 8) & 0xFF;

	z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);

	sleep_ms(100);

	for(try = 0; try < 4; ++try) {
		z2->spi_txrx(z2->tx_buf, 16, z2->rx_buf, 16);

		if(z2->rx_buf[0] == MT_CMD_GET_INTERFACE_VERSION) {
			checksum = 0;
			for(i = 0; i < 14; ++i)
				checksum += z2->rx_buf[i];

			if((z2->rx_buf[14] | (z2->rx_buf[15] << 8)) == checksum) {
				z2->interface_version = z2->rx_buf[2];
				z2->max_packet_size = (z2->rx_buf[4] << 8) | z2->rx_buf[3];
				debugf("multitouch: interface version %d, max packet size: %d\r\n", z2->interface_version, z2->max_packet_size);
				return TRUE;
			}
		}
		sleep_ms(5);
	}

	debugf("multitouch: failed getting interface version!\r\n");
	return FALSE;
}

int mt_populate_reports(MTDevice* z2) {
	uint8_t* reportBuffer = malloc(8192);
	int reportLen;

	sleep_ms(10);
	if(!mt_get_report(z2, MT_INFO_FAMILYID, reportBuffer, &reportLen)) {
		printf("multitouch: failed getting family id!\n");
	}
	z2->family_id = reportBuffer[0];

	sleep_ms(10);
	if(!mt_get_report(z2, MT_INFO_SENSORINFO, reportBuffer, &reportLen)) {
		printf("multitouch: failed getting sensor info!\n");
	}

	z2->endianness = reportBuffer[0];
	z2->sensor_rows = reportBuffer[1];
	z2->sensor_columns = reportBuffer[2];
	z2->bcd_version = ((reportBuffer[3] & 0xFF) << 8) | (reportBuffer[4] & 0xFF);

	sleep_ms(10);
	if(!mt_get_report(z2, MT_INFO_SENSORREGIONDESC, reportBuffer, &reportLen)) {
		printf("multitouch: failed getting sensor region descriptor!\n");
	}

	z2->sensor_region_descriptor_len = reportLen;
	memcpy(z2->sensor_region_descriptor, reportBuffer, reportLen);

	sleep_ms(10);
	if(!mt_get_report(z2, MT_INFO_SENSORREGIONPARAM, reportBuffer, &reportLen)) {
		printf("multitouch: failed getting sensor region param!\n");
	}

	z2->sensor_region_param_len = reportLen;
	memcpy(z2->sensor_region_param, reportBuffer, reportLen);

	sleep_ms(10);
	if(!mt_get_report(z2, MT_INFO_SENSORDIM, reportBuffer, &reportLen)) {
		printf("multitouch: failed getting sensor surface dimensions!\n");
	}

	z2->sensor_width = *((uint32_t*)&reportBuffer[0]);
	z2->sensor_height = *((uint32_t*)&reportBuffer[4]);
	free(reportBuffer);
	return TRUE;
}

void mt_read_frame(MTDevice *z2) {
	MTDeviceReadInterruptCmd *len_cmd = (void *)z2->tx_buf;
	MTFrame frame;

	memset(z2->tx_buf, 0, sizeof(MTDeviceReadInterruptCmd));
	len_cmd->cmd = MT_CMD_READ_INTERRUPT_DATA;
	len_cmd->counter = z2->index_parity + 1;
	len_cmd->checksum = MT_CMD_READ_INTERRUPT_DATA + len_cmd->counter;
	z2->index_parity = !z2->index_parity;

	// read the frame length
	z2->spi_txrx(z2->tx_buf, sizeof(MTFrameLengthPacket), (void*) &frame.frame_length, sizeof(MTFrameLengthPacket));
	while(gpio_get(MT_ATTN_PIN) == 0); // wait for MT_ATTN_PIN to go high again
	sleep_us(100); // wait a bit to ensure the Z2 is ready for the next read

	// read the frame packet
	z2->spi_txrx_nostop(z2->tx_buf, sizeof(MTFramePacket), (void*) &frame.frame_packet, sizeof(MTFramePacket));
	uint8_t fingers = frame.frame_packet.header.numFingers;

	// read the finger data
	if (fingers > 10) fingers = 10;
	if (fingers > 0) {
		z2->spi_txrx_nostop(z2->tx_buf, fingers * sizeof(FingerData), (void*) &frame.finger_data, fingers * sizeof(FingerData));
	}

	// read the checksums
	z2->spi_txrx(z2->tx_buf, 2, &frame.checksum1, 2);

	z2->fingers = fingers;
	memcpy(z2->finger_data, frame.finger_data, fingers * sizeof(FingerData));

	// Mark all currently active contacts as "not seen this frame"
	uint8_t seen[10] = {0};

	// Process each finger in the current frame
	for (int i = 0; i < fingers; i++) {
		FingerData* finger = &frame.finger_data[i];
		uint8_t hw_finger_id = finger->finger;
		
		// Find if this hardware finger ID is already tracked
		int found_slot = -1;
		for (int j = 0; j < 10; j++) {
			if (z2->tracked_fingers[j].state != CONTACT_FREE && 
			    z2->tracked_fingers[j].hw_finger_id == hw_finger_id) {
				found_slot = j;
				break;
			}
		}
		
		// Check if this is a touch end event
		if (finger->state == MT_EVENT_TOUCH_ENDED || finger->state == MT_EVENT_TOUCH_FULL_END) {
			if (found_slot != -1) {
				// Mark for pending release - we need to send one final "up" report
				z2->tracked_fingers[found_slot].state = CONTACT_PENDING_RELEASE;
				// Update the final position data
				memcpy(&z2->tracked_fingers[found_slot].data, finger, sizeof(FingerData));
				seen[found_slot] = 1;
			}
		} else if (finger->state == MT_EVENT_TOUCH_START || finger->state == MT_EVENT_TOUCH_MOVED) {
			if (found_slot != -1) {
				// Update existing active contact
				z2->tracked_fingers[found_slot].state = CONTACT_ACTIVE;
				memcpy(&z2->tracked_fingers[found_slot].data, finger, sizeof(FingerData));
				seen[found_slot] = 1;
			} else {
				// New finger - find a free slot and assign a stable contact ID
				for (int j = 0; j < 10; j++) {
					if (z2->tracked_fingers[j].state == CONTACT_FREE) {
						z2->tracked_fingers[j].state = CONTACT_ACTIVE;
						z2->tracked_fingers[j].contact_id = j + 1; // Stable contact ID (1-10)
						z2->tracked_fingers[j].hw_finger_id = hw_finger_id;
						memcpy(&z2->tracked_fingers[j].data, finger, sizeof(FingerData));
						seen[j] = 1;
						break;
					}
				}
			}
		}
	}

	// Any active contacts not seen in this frame are implicitly lifted
	for (int i = 0; i < 10; i++) {
		if (z2->tracked_fingers[i].state == CONTACT_ACTIVE && seen[i] == 0) {
			// Finger was active but not in this frame - mark for release
			z2->tracked_fingers[i].state = CONTACT_PENDING_RELEASE;
		}
	}

	// If the frame has 0 fingers, mark all active contacts as pending release
	if (fingers == 0) {
		for (int i = 0; i < 10; i++) {
			if (z2->tracked_fingers[i].state == CONTACT_ACTIVE) {
				z2->tracked_fingers[i].state = CONTACT_PENDING_RELEASE;
			}
		}
	}

	// Count active and pending contacts
	z2->active_finger_count = 0;
	for (int i = 0; i < 10; i++) {
		if (z2->tracked_fingers[i].state != CONTACT_FREE) {
			z2->active_finger_count++;
		}
	}
}