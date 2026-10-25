// the libdbus calls tray.ag makes, for a system with no session bus.
// windows has none: a connection never opens, so the tray holds
// nothing -- the same shape as a linux box with the bus down. every
// definition here is windows-only; elsewhere the real libdbus serves
#ifdef _WIN32

#include <dbus-win.h>
#include <string.h>

static const char* no_bus_name = "org.freedesktop.DBus.Error.NotSupported";
static const char* no_bus_msg  = "no session bus on this system";

static void set_no_bus(DBusError* e) {
    if (!e) return;
    e->name    = no_bus_name;
    e->message = no_bus_msg;
}

void dbus_error_init(DBusError* e) { if (e) memset(e, 0, sizeof(*e)); }
void dbus_error_free(DBusError* e) { if (e) memset(e, 0, sizeof(*e)); }

dbus_bool_t dbus_error_is_set(const DBusError* e) {
    return (e && e->name) ? 1 : 0;
}

DBusConnection* dbus_bus_get_private(DBusBusType t, DBusError* e) {
    (void)t; set_no_bus(e); return 0;
}

DBusConnection* dbus_connection_open_private(const char* addr, DBusError* e) {
    (void)addr; set_no_bus(e); return 0;
}

dbus_bool_t dbus_bus_register(DBusConnection* c, DBusError* e) {
    (void)c; set_no_bus(e); return 0;
}

int dbus_bus_request_name(DBusConnection* c, const char* name,
                          unsigned int flags, DBusError* e) {
    (void)c; (void)name; (void)flags; set_no_bus(e); return -1;
}

void dbus_bus_add_match(DBusConnection* c, const char* rule, DBusError* e) {
    (void)c; (void)rule; set_no_bus(e);
}

void dbus_connection_close(DBusConnection* c) { (void)c; }
void dbus_connection_unref(DBusConnection* c) { (void)c; }
void dbus_connection_flush(DBusConnection* c) { (void)c; }

dbus_bool_t dbus_connection_get_is_connected(DBusConnection* c) {
    (void)c; return 0;
}

void dbus_connection_set_exit_on_disconnect(DBusConnection* c,
                                            dbus_bool_t v) {
    (void)c; (void)v;
}

dbus_bool_t dbus_connection_read_write(DBusConnection* c, int timeout_ms) {
    (void)c; (void)timeout_ms; return 0;
}

DBusMessage* dbus_connection_pop_message(DBusConnection* c) {
    (void)c; return 0;
}

dbus_bool_t dbus_connection_send(DBusConnection* c, DBusMessage* m,
                                 dbus_uint32_t* serial) {
    (void)c; (void)m;
    if (serial) *serial = 0;
    return 0;
}

DBusMessage* dbus_connection_send_with_reply_and_block(DBusConnection* c,
        DBusMessage* m, int timeout_ms, DBusError* e) {
    (void)c; (void)m; (void)timeout_ms; set_no_bus(e); return 0;
}

// no bus means no message to carry: nothing is ever built or read
DBusMessage* dbus_message_new_method_call(const char* dest, const char* path,
        const char* iface, const char* method) {
    (void)dest; (void)path; (void)iface; (void)method; return 0;
}

DBusMessage* dbus_message_new_method_return(DBusMessage* call) {
    (void)call; return 0;
}

DBusMessage* dbus_message_new_signal(const char* path, const char* iface,
                                     const char* name) {
    (void)path; (void)iface; (void)name; return 0;
}

DBusMessage* dbus_message_new_error(DBusMessage* reply_to, const char* name,
                                    const char* message) {
    (void)reply_to; (void)name; (void)message; return 0;
}

void dbus_message_unref(DBusMessage* m) { (void)m; }

const char* dbus_message_get_sender(DBusMessage* m) { (void)m; return ""; }

int dbus_message_get_type(DBusMessage* m) {
    (void)m; return DBUS_MESSAGE_TYPE_INVALID;
}

// unset is what libdbus reports for a message that carries none; here
// there is never a message at all
const char* dbus_message_get_interface(DBusMessage* m) { (void)m; return ""; }
const char* dbus_message_get_member(DBusMessage* m)    { (void)m; return ""; }
const char* dbus_message_get_path(DBusMessage* m)      { (void)m; return ""; }
const char* dbus_message_get_error_name(DBusMessage* m){ (void)m; return ""; }

dbus_uint32_t dbus_message_get_reply_serial(DBusMessage* m) {
    (void)m; return 0;
}

dbus_bool_t dbus_message_set_destination(DBusMessage* m, const char* dest) {
    (void)m; (void)dest; return 0;
}

dbus_bool_t dbus_message_is_method_call(DBusMessage* m, const char* iface,
                                        const char* method) {
    (void)m; (void)iface; (void)method; return 0;
}

dbus_bool_t dbus_message_is_signal(DBusMessage* m, const char* iface,
                                   const char* name) {
    (void)m; (void)iface; (void)name; return 0;
}

dbus_bool_t dbus_message_iter_init(DBusMessage* m, DBusMessageIter* i) {
    (void)m;
    if (i) memset(i, 0, sizeof(*i));
    return 0;
}

void dbus_message_iter_init_append(DBusMessage* m, DBusMessageIter* i) {
    (void)m;
    if (i) memset(i, 0, sizeof(*i));
}

int dbus_message_iter_get_arg_type(DBusMessageIter* i) {
    (void)i; return DBUS_TYPE_INVALID;
}

// the caller's slot is of a width only the arg type knows, and there
// is no value to report, so both readers leave it untouched
void dbus_message_iter_get_basic(DBusMessageIter* i, void* value) {
    (void)i; (void)value;
}

void dbus_message_iter_get_fixed_array(DBusMessageIter* i, void* value,
                                       int* n) {
    (void)i; (void)value;
    if (n) *n = 0;
}

dbus_bool_t dbus_message_iter_next(DBusMessageIter* i) { (void)i; return 0; }

void dbus_message_iter_recurse(DBusMessageIter* i, DBusMessageIter* sub) {
    (void)i;
    if (sub) memset(sub, 0, sizeof(*sub));
}

dbus_bool_t dbus_message_iter_append_basic(DBusMessageIter* i, int type,
                                           const void* value) {
    (void)i; (void)type; (void)value; return 0;
}

dbus_bool_t dbus_message_iter_open_container(DBusMessageIter* i, int type,
        const char* sig, DBusMessageIter* sub) {
    (void)i; (void)type; (void)sig;
    if (sub) memset(sub, 0, sizeof(*sub));
    return 0;
}

dbus_bool_t dbus_message_iter_close_container(DBusMessageIter* i,
                                              DBusMessageIter* sub) {
    (void)i; (void)sub; return 0;
}

#endif
