#include <stdint.h>

#ifndef MULTITOUCH_H
#define MULTITOUCH_H

#define TRUE 1
#define FALSE 0

#define MAX_FINGERS 10

#define MT_INFO_FAMILYID 0xD1
#define MT_INFO_SENSORINFO 0xD3
#define MT_INFO_SENSORREGIONDESC 0xD0
#define MT_INFO_SENSORREGIONPARAM 0xA1
#define MT_INFO_SENSORDIM 0xD9

#define MT_CMD_GET_CMD_STATUS        0xE1
#define MT_CMD_GET_INTERFACE_VERSION 0xE2
#define MT_CMD_GET_REPORT_INFO       0xE3
#define MT_CMD_SHORT_CONTROL_WRITE   0xE4
#define MT_CMD_LONG_CONTROL_WRITE    0xE5
#define MT_CMD_SHORT_CONTROL_READ    0xE6
#define MT_CMD_LONG_CONTROL_READ     0xE7
#define MT_CMD_FRAME_READ            0xEA
#define MT_CMD_READ_INTERRUPT_DATA   0xEB

// frame event types
#define MT_EVENT_TOUCH_FULL_END 0x0
#define MT_EVENT_TOUCH_START 0x3
#define MT_EVENT_TOUCH_MOVED 0x4
#define MT_EVENT_TOUCH_ENDED 0x7

typedef struct MTFrameLengthPacket {
    uint8_t cmd;
    uint8_t length1;
    uint8_t length2;
    uint8_t unused[11];
    uint8_t checksum1;
    uint8_t checksum2;
} __attribute__((__packed__)) MTFrameLengthPacket;

typedef struct MTFrameHeader {
    uint8_t type;
    uint8_t frameNum;
    uint8_t headerLen;
    uint8_t unk_3;
    uint32_t timestamp;
    uint8_t unk_8;
    uint8_t unk_9;
    uint8_t unk_A;
    uint8_t unk_B;
    uint16_t unk_C;
    uint16_t isImage;

    uint8_t numFingers;
    uint8_t fingerDataLen;
    uint16_t unk_12;
    uint16_t unk_14;
    uint16_t unk_16;
} __attribute__((__packed__)) MTFrameHeader;

typedef struct MTFramePacket {
    uint8_t cmd;
    uint8_t unused1;
    uint8_t length1;
    uint8_t length2;
    uint8_t checksum_pad;
    MTFrameHeader header;
} __attribute__((__packed__)) MTFramePacket;

typedef struct __attribute__((__packed__)) {
    uint8_t finger;
    uint8_t state;
    uint16_t unknown2;
    int16_t abs_x;
    int16_t abs_y;
    int16_t rel_x;
    int16_t rel_y;
    uint16_t tool_major;
    uint16_t tool_minor;
    uint16_t orientation;
    uint16_t touch_major;
    uint16_t touch_minor;
    uint16_t unused[2];
    uint16_t pressure;
    uint16_t multi;
} FingerData;

typedef struct MTFrame {
    MTFrameLengthPacket frame_length;
    MTFramePacket frame_packet;
    FingerData finger_data[10];
    uint8_t checksum1;
    uint8_t checksum2;
} __attribute__((__packed__)) MTFrame;

typedef struct {
    uint8_t cmd;
    uint8_t counter;
    uint8_t unused[12];
    uint16_t checksum;
} MTDeviceReadInterruptCmd;

// Contact lifecycle states
typedef enum {
    CONTACT_FREE = 0,           // Slot is available
    CONTACT_ACTIVE = 1,         // Finger is on screen
    CONTACT_PENDING_RELEASE = 2 // Finger lifted, waiting to send final "up" report
} ContactState;

typedef struct {
    ContactState state;  // Contact lifecycle state
    uint8_t contact_id;  // The HID contact ID (stable for this contact's lifetime)
    uint8_t hw_finger_id; // The hardware finger ID from the sensor
    FingerData data;     // The finger data
} TrackedFinger;

typedef struct MTDevice {
	int index_parity;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
    uint8_t fingers;
    FingerData finger_data[10];  // Raw frame data (deprecated, kept for compatibility)
    TrackedFinger tracked_fingers[10];  // Tracked finger states by ID
    uint8_t active_finger_count;  // Number of currently active fingers
    int (*spi_txrx)(const uint8_t*, int, uint8_t*, int);
    int (*spi_txrx_nostop)(const uint8_t*, int, uint8_t*, int);
    
    int interface_version;
    int max_packet_size;
    int family_id;
    int flip_nop;
    int sensor_width;
    int sensor_height;
    int sensor_columns;
    int sensor_rows;
    int bcd_version;
    int endianness;
    int sensor_region_descriptor_len;
    uint8_t sensor_region_descriptor[8192];
    int sensor_region_param_len;
    uint8_t sensor_region_param[8192];
} MTDevice;

void mt_wait_for_ready(MTDevice*);
int mt_get_report(MTDevice*, int, uint8_t*, int*);
int mt_get_report_info(MTDevice*, int id, uint8_t* err, uint16_t* len);
int mt_short_control_read(MTDevice*, int id, uint8_t* buffer, int size);
int mt_long_control_read(MTDevice*, int id, uint8_t* buffer, int size);
int mt_get_report(MTDevice*, int id, uint8_t* buffer, int* outLen);
int mt_interface_version(MTDevice*);
int mt_populate_reports(MTDevice*);
void mt_read_frame(MTDevice*);

#endif