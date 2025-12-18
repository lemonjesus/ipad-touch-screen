#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stdbool.h>

void usb_hid_init(void);
void usb_hid_task(MTDevice* z2);

#endif // USB_HID_H
