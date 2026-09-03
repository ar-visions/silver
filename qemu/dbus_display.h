#ifndef QEMU_DBUS_DISPLAY_H
#define QEMU_DBUS_DISPLAY_H
#include <stdint.h>
// start the peer-to-peer qemu D-Bus display client (its own glib thread).
// sock_path is qemu's `-display dbus,addr=unix:path=<sock>` socket.
int qemu_dbus_start(const char* sock_path);
// current guest scanout as a dma-buf fd (a dup the caller owns; -1 if none).
// gen bumps per new buffer (re-import on change); dirty is 1 if the frame
// changed since the last call, then cleared.
int qemu_dbus_scanout(int* w, int* h, int* stride, unsigned* fourcc,
                      uint64_t* modifier, int* gen, int* dirty, int* y0top);
#endif
