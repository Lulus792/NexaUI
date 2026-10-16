#ifndef NEXAUI_LINUX_DBUS_API_H
#define NEXAUI_LINUX_DBUS_API_H
/* Private Linux SDK ABI; loaded at runtime, never part of the installed API. */
#include <dbus/dbus.h>
#define NX_DBUS_FUNCTIONS(X) \
 X(dbus_bool_t, threads_init_default, (void)) \
 X(void, error_init, (DBusError *)) \
 X(void, error_free, (DBusError *)) \
 X(DBusConnection *, connection_open_private, (const char *, DBusError *)) \
 X(dbus_bool_t, bus_register, (DBusConnection *, DBusError *)) \
 X(const char *, bus_get_unique_name, (DBusConnection *)) \
 X(void, connection_set_exit_on_disconnect, (DBusConnection *, dbus_bool_t)) \
 X(dbus_bool_t, connection_register_fallback, (DBusConnection *, const char *, const DBusObjectPathVTable *, void *)) \
 X(void, connection_close, (DBusConnection *)) \
 X(void, connection_unref, (DBusConnection *)) \
 X(dbus_bool_t, connection_get_is_connected, (DBusConnection *)) \
 X(dbus_bool_t, connection_get_unix_fd, (DBusConnection *, int *)) \
 X(dbus_bool_t, connection_has_messages_to_send, (DBusConnection *)) \
 X(long, connection_get_outgoing_size, (DBusConnection *)) \
 X(void, connection_set_max_message_size, (DBusConnection *, long)) \
 X(void, connection_set_max_received_size, (DBusConnection *, long)) \
 X(dbus_bool_t, connection_read_write, (DBusConnection *, int)) \
 X(DBusDispatchStatus, connection_dispatch, (DBusConnection *)) \
 X(DBusDispatchStatus, connection_get_dispatch_status, (DBusConnection *)) \
 X(dbus_bool_t, connection_send, (DBusConnection *, DBusMessage *, dbus_uint32_t *)) \
 X(dbus_bool_t, connection_send_with_reply, (DBusConnection *, DBusMessage *, DBusPendingCall **, int)) \
 X(DBusMessage *, connection_send_with_reply_and_block, (DBusConnection *, DBusMessage *, int, DBusError *)) \
 X(dbus_bool_t, pending_call_get_completed, (DBusPendingCall *)) \
 X(DBusMessage *, pending_call_steal_reply, (DBusPendingCall *)) \
 X(void, pending_call_cancel, (DBusPendingCall *)) \
 X(void, pending_call_unref, (DBusPendingCall *)) \
 X(DBusMessage *, message_new_method_call, (const char *, const char *, const char *, const char *)) \
 X(DBusMessage *, message_new_method_return, (DBusMessage *)) \
 X(DBusMessage *, message_new_error, (DBusMessage *, const char *, const char *)) \
 X(DBusMessage *, message_new_signal, (const char *, const char *, const char *)) \
 X(void, message_unref, (DBusMessage *)) \
 X(const char *, message_get_path, (DBusMessage *)) \
 X(const char *, message_get_interface, (DBusMessage *)) \
 X(const char *, message_get_member, (DBusMessage *)) \
 X(int, message_get_type, (DBusMessage *)) \
 X(dbus_bool_t, message_has_signature, (DBusMessage *, const char *)) \
 X(dbus_bool_t, message_get_args, (DBusMessage *, DBusError *, int, ...)) \
 X(dbus_bool_t, message_append_args, (DBusMessage *, int, ...)) \
 X(void, message_iter_init_append, (DBusMessage *, DBusMessageIter *)) \
 X(dbus_bool_t, message_iter_init, (DBusMessage *, DBusMessageIter *)) \
 X(dbus_bool_t, message_iter_next, (DBusMessageIter *)) \
 X(int, message_iter_get_arg_type, (DBusMessageIter *)) \
 X(void, message_iter_get_basic, (DBusMessageIter *, void *)) \
 X(void, message_iter_recurse, (DBusMessageIter *, DBusMessageIter *)) \
 X(dbus_bool_t, message_iter_append_basic, (DBusMessageIter *, int, const void *)) \
 X(dbus_bool_t, message_iter_open_container, (DBusMessageIter *, int, const char *, DBusMessageIter *)) \
 X(dbus_bool_t, message_iter_close_container, (DBusMessageIter *, DBusMessageIter *))
typedef struct nx_dbus_api {
#define NX_DBUS_MEMBER(result, name, args) result (*name) args;
 NX_DBUS_FUNCTIONS(NX_DBUS_MEMBER)
#undef NX_DBUS_MEMBER
} nx_dbus_api;
#endif
