// fastboot's USB side: find the phone's fastboot interface and move
// bytes over its two bulk endpoints (libusb)
#ifndef FASTBOOT_USB_H
#define FASTBOOT_USB_H

// open the first fastboot device; its serial in serial (cap bytes);
// 1 when open, 0 when none, -2 when one is there but access is
// refused (no udev rule), -1 on a USB error
int fb_usb_open(char* serial, int cap);

// bytes out; the count written, or -1
int fb_usb_write(const void* data, int len);

// one reply packet in (at most cap bytes); its length, or -1
int fb_usb_read(void* data, int cap, int timeout_ms);

void fb_usb_close(void);

#endif
