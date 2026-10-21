// fastboot's USB side over libusb; the protocol is fastboot.ag's
#include <string.h>
#include <libusb-1.0/libusb.h>
#include "fastboot_usb.h"

static libusb_context* ctx;
static libusb_device_handle* dev;
static unsigned char ep_in, ep_out;
static int iface = -1;

// the interface fastboot answers on: class ff, subclass 42, proto 03
static int find_iface(libusb_device* d, unsigned char* in,
                      unsigned char* out) {
    struct libusb_config_descriptor* cfg;
    if (libusb_get_active_config_descriptor(d, &cfg)) return -1;
    int found = -1;
    for (int i = 0; i < cfg->bNumInterfaces && found < 0; i++) {
        const struct libusb_interface* it = &cfg->interface[i];
        for (int a = 0; a < it->num_altsetting && found < 0; a++) {
            const struct libusb_interface_descriptor* s = &it->altsetting[a];
            if (s->bInterfaceClass != 0xff || s->bInterfaceSubClass != 0x42
                || s->bInterfaceProtocol != 0x03) continue;
            *in = *out = 0;
            for (int e = 0; e < s->bNumEndpoints; e++) {
                const struct libusb_endpoint_descriptor* ep = &s->endpoint[e];
                if ((ep->bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_BULK) continue;
                if (ep->bEndpointAddress & 0x80) *in = ep->bEndpointAddress;
                else *out = ep->bEndpointAddress;
            }
            if (*in && *out) found = s->bInterfaceNumber;
        }
    }
    libusb_free_config_descriptor(cfg);
    return found;
}

int fb_usb_open(char* serial, int cap) {
    if (serial && cap > 0) serial[0] = 0;
    if (!ctx && libusb_init(&ctx)) return -1;
    libusb_device** list;
    ssize_t n = libusb_get_device_list(ctx, &list);
    if (n < 0) return -1;
    int got = 0, refused = 0;
    for (ssize_t i = 0; i < n && !got; i++) {
        unsigned char in, out;
        int f = find_iface(list[i], &in, &out);
        if (f < 0) continue;
        if (libusb_open(list[i], &dev)) {
            refused = 1;
            continue;
        }
        libusb_set_auto_detach_kernel_driver(dev, 1);
        if (libusb_claim_interface(dev, f)) {
            libusb_close(dev);
            dev = NULL;
            continue;
        }
        struct libusb_device_descriptor dd;
        if (serial && cap > 0 && !libusb_get_device_descriptor(list[i], &dd)
            && dd.iSerialNumber)
            libusb_get_string_descriptor_ascii(dev, dd.iSerialNumber,
                (unsigned char*)serial, cap);
        ep_in = in;
        ep_out = out;
        iface = f;
        got = 1;
    }
    libusb_free_device_list(list, 1);
    return got ? 1 : (refused ? -2 : 0);
}

int fb_usb_write(const void* data, int len) {
    if (!dev) return -1;
    int done = 0;
    while (done < len) {
        int part = len - done > (1 << 20) ? (1 << 20) : len - done, k = 0;
        if (libusb_bulk_transfer(dev, ep_out, (unsigned char*)data + done,
                                 part, &k, 30000)) return -1;
        done += k;
    }
    // a write ending on a packet boundary is closed by an empty one
    if (len > 0 && len % 512 == 0) {
        int k;
        libusb_bulk_transfer(dev, ep_out, (unsigned char*)data, 0, &k, 1000);
    }
    return done;
}

int fb_usb_read(void* data, int cap, int timeout_ms) {
    if (!dev) return -1;
    int k = 0;
    if (libusb_bulk_transfer(dev, ep_in, data, cap, &k, timeout_ms))
        return -1;
    return k;
}

void fb_usb_close(void) {
    if (dev) {
        libusb_release_interface(dev, iface);
        libusb_close(dev);
    }
    dev = NULL;
}
