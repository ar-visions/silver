// dbus-win.h: the libdbus surface tray.ag calls, for a system with no
// session bus. windows has none, so a connection never opens and the
// tray simply holds nothing -- the same shape as a linux box with the
// bus down. a real windows tray would use Shell_NotifyIcon instead
#ifndef AU_DBUS_WIN_H
#define AU_DBUS_WIN_H

#include <posix.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int dbus_bool_t;
typedef unsigned int dbus_uint32_t;
typedef int          dbus_int32_t;

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage    DBusMessage;

// laid out as libdbus has it: both are held by value by callers
typedef struct DBusError {
    const char*  name;
    const char*  message;
    unsigned int dummy1 : 1;
    unsigned int dummy2 : 1;
    unsigned int dummy3 : 1;
    unsigned int dummy4 : 1;
    unsigned int dummy5 : 1;
    void*        padding1;
} DBusError;

typedef struct DBusMessageIter {
    void*        dummy1;
    void*        dummy2;
    dbus_uint32_t dummy3;
    int          dummy4;
    int          dummy5;
    int          dummy6;
    int          dummy7;
    int          dummy8;
    int          dummy9;
    int          dummy10;
    int          dummy11;
    int          pad1;
    void*        pad2;
    void*        pad3;
} DBusMessageIter;

typedef enum DBusBusType {
    DBUS_BUS_SESSION = 0,
    DBUS_BUS_SYSTEM  = 1,
    DBUS_BUS_STARTER = 2
} DBusBusType;

// the type codes are the signature characters themselves
#define DBUS_TYPE_INVALID     0
#define DBUS_TYPE_BOOLEAN     'b'
#define DBUS_TYPE_INT32       'i'
#define DBUS_TYPE_UINT32      'u'
#define DBUS_TYPE_DOUBLE      'd'
#define DBUS_TYPE_STRING      's'
#define DBUS_TYPE_OBJECT_PATH 'o'
#define DBUS_TYPE_ARRAY       'a'
#define DBUS_TYPE_VARIANT     'v'
#define DBUS_TYPE_STRUCT      'r'
#define DBUS_TYPE_DICT_ENTRY  'e'

#define DBUS_MESSAGE_TYPE_INVALID       0
#define DBUS_MESSAGE_TYPE_METHOD_CALL   1
#define DBUS_MESSAGE_TYPE_METHOD_RETURN 2
#define DBUS_MESSAGE_TYPE_ERROR         3
#define DBUS_MESSAGE_TYPE_SIGNAL        4

#define DBUS_NAME_FLAG_ALLOW_REPLACEMENT  1
#define DBUS_NAME_FLAG_REPLACE_EXISTING   2
#define DBUS_NAME_FLAG_DO_NOT_QUEUE       4

#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1
#define DBUS_REQUEST_NAME_REPLY_IN_QUEUE      2
#define DBUS_REQUEST_NAME_REPLY_EXISTS        3
#define DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER 4

#define DBUS_TIMEOUT_USE_DEFAULT (-1)

AU_EXPORT void            dbus_error_init    (DBusError* e);
AU_EXPORT void            dbus_error_free    (DBusError* e);
AU_EXPORT dbus_bool_t     dbus_error_is_set  (const DBusError* e);

AU_EXPORT DBusConnection* dbus_bus_get_private(DBusBusType t, DBusError* e);
AU_EXPORT DBusConnection* dbus_connection_open_private(const char* addr,
                                                       DBusError* e);
AU_EXPORT dbus_bool_t     dbus_bus_register  (DBusConnection* c, DBusError* e);
AU_EXPORT int             dbus_bus_request_name(DBusConnection* c,
                              const char* name, unsigned int flags,
                              DBusError* e);
AU_EXPORT void            dbus_bus_add_match (DBusConnection* c,
                              const char* rule, DBusError* e);

AU_EXPORT void            dbus_connection_close   (DBusConnection* c);
AU_EXPORT void            dbus_connection_unref   (DBusConnection* c);
AU_EXPORT void            dbus_connection_flush   (DBusConnection* c);
AU_EXPORT dbus_bool_t     dbus_connection_get_is_connected(DBusConnection* c);
AU_EXPORT void            dbus_connection_set_exit_on_disconnect(
                              DBusConnection* c, dbus_bool_t v);
AU_EXPORT dbus_bool_t     dbus_connection_read_write(DBusConnection* c,
                              int timeout_ms);
AU_EXPORT DBusMessage*    dbus_connection_pop_message(DBusConnection* c);
AU_EXPORT dbus_bool_t     dbus_connection_send(DBusConnection* c,
                              DBusMessage* m, dbus_uint32_t* serial);
AU_EXPORT DBusMessage*    dbus_connection_send_with_reply_and_block(
                              DBusConnection* c, DBusMessage* m,
                              int timeout_ms, DBusError* e);

AU_EXPORT DBusMessage*    dbus_message_new_method_call(const char* dest,
                              const char* path, const char* iface,
                              const char* method);
AU_EXPORT DBusMessage*    dbus_message_new_method_return(DBusMessage* call);
AU_EXPORT DBusMessage*    dbus_message_new_signal(const char* path,
                              const char* iface, const char* name);
AU_EXPORT DBusMessage*    dbus_message_new_error(DBusMessage* reply_to,
                              const char* name, const char* message);
AU_EXPORT void            dbus_message_unref (DBusMessage* m);
AU_EXPORT const char*     dbus_message_get_sender(DBusMessage* m);
AU_EXPORT int             dbus_message_get_type(DBusMessage* m);
AU_EXPORT const char*     dbus_message_get_interface(DBusMessage* m);
AU_EXPORT const char*     dbus_message_get_member(DBusMessage* m);
AU_EXPORT const char*     dbus_message_get_path(DBusMessage* m);
AU_EXPORT const char*     dbus_message_get_error_name(DBusMessage* m);
AU_EXPORT dbus_uint32_t   dbus_message_get_reply_serial(DBusMessage* m);
AU_EXPORT dbus_bool_t     dbus_message_set_destination(DBusMessage* m,
                              const char* dest);
AU_EXPORT dbus_bool_t     dbus_message_is_method_call(DBusMessage* m,
                              const char* iface, const char* method);
AU_EXPORT dbus_bool_t     dbus_message_is_signal(DBusMessage* m,
                              const char* iface, const char* name);

AU_EXPORT dbus_bool_t     dbus_message_iter_init(DBusMessage* m,
                              DBusMessageIter* i);
AU_EXPORT void            dbus_message_iter_init_append(DBusMessage* m,
                              DBusMessageIter* i);
AU_EXPORT int             dbus_message_iter_get_arg_type(DBusMessageIter* i);
AU_EXPORT void            dbus_message_iter_get_basic(DBusMessageIter* i,
                              void* value);
AU_EXPORT void            dbus_message_iter_get_fixed_array(DBusMessageIter* i,
                              void* value, int* n);
AU_EXPORT dbus_bool_t     dbus_message_iter_next(DBusMessageIter* i);
AU_EXPORT void            dbus_message_iter_recurse(DBusMessageIter* i,
                              DBusMessageIter* sub);
AU_EXPORT dbus_bool_t     dbus_message_iter_append_basic(DBusMessageIter* i,
                              int type, const void* value);
AU_EXPORT dbus_bool_t     dbus_message_iter_open_container(DBusMessageIter* i,
                              int type, const char* sig, DBusMessageIter* sub);
AU_EXPORT dbus_bool_t     dbus_message_iter_close_container(DBusMessageIter* i,
                              DBusMessageIter* sub);

#ifdef __cplusplus
}
#endif
#endif
