#include "multitouch/multitouch.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x2E8A,  // Raspberry Pi
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] = {	
	0x05, 0x0D, 			// Usage Page (Digitizer)
	0x09, 0x04, 			// Usage (Touch Screen)
	0xA1, 0x01, 			// Collection (Application)
	
	// --- Define Feature Report (for Max Contact Count) ---
    0x85, 0x02,	   		    // Report ID (2)
    0x09, 0x55,       		// Usage (Contact Count Maximum)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, MAX_FINGERS,      // Logical Maximum
    0x75, 0x08,       		// Report Size (8 bits)
    0x95, 0x01,       		// Report Count (1)
    0xB1, 0x02,       		// Feature (Data,Var,Abs) (This defines it as a Feature Report)
	
	#define FINGER_BLOCK \
		0x09, 0x22,        /* Usage (Finger) */ \
		0xA1, 0x02,        /* Collection (Logical) */ \
		/* Status bits: Tip Switch and In Range */ \
		0x05, 0x0D,        /*   Usage Page (Digitizer) */ \
		0x09, 0x42,        /*   Usage (Tip Switch) */ \
		0x09, 0x32,        /*   Usage (In Range) */ \
		0x15, 0x00,        /*   Logical Minimum (0) */ \
		0x25, 0x01,        /*   Logical Maximum (1) */ \
		0x75, 0x01,        /*   Report Size (1 bit) */ \
		0x95, 0x02,        /*   Report Count (2) */ \
		0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
		0x95, 0x06,        /*   Report Count (6) - Padding */ \
		0x81, 0x03,        /*   Input (Cnst,Var,Abs) */ \
		/* Contact ID */ \
		0x09, 0x51,        /*   Usage (Contact Identifier) */ \
		0x75, 0x08,        /*   Report Size (8 bits) */ \
		0x95, 0x01,        /*   Report Count (1) */ \
		0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
		/* X and Y Coordinates */ \
		0x05, 0x01,        /*   Usage Page (Generic Desktop) */ \
		0x26, 0xFF, 0x7F,  /*   Logical Maximum (32767) */ \
		0x75, 0x10,        /*   Report Size (16 bits) */ \
		0x09, 0x30,        /*   Usage (X) */ \
		0x09, 0x31,        /*   Usage (Y) */ \
		0x95, 0x02,        /*   Report Count (2) */ \
		0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
		0xC0,              /* End Collection */

	// --- Define Input Report (for Contacts) ---
	0x85, 0x01, 			//     Report ID (1)
    FINGER_BLOCK
    FINGER_BLOCK
    FINGER_BLOCK
    FINGER_BLOCK
    FINGER_BLOCK
	FINGER_BLOCK
    FINGER_BLOCK
    FINGER_BLOCK
    FINGER_BLOCK
    FINGER_BLOCK

	// --- Define Contact Count ---
    // This value at the end of the report tells the host how many of the
    // preceding finger structures contain valid data.
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x54,        //   Usage (Contact Count)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, MAX_FINGERS, //   Logical Maximum
    0x75, 0x08,        //   Report Size (8 bits)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)

	0xC0,                   // End Collection
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum {
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID 0x81

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    // Changed polling interval from 10ms to 1ms for better touch responsiveness
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1)
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; // for multiple configurations
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 },   // 0: English (0x0409)
    "Custom Device",                 // 1: Manufacturer
    "iPad 3 Touch Screen",           // 2: Product
    "123456",                        // 3: Serial Number
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Convert ASCII string into UTF-16
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}
