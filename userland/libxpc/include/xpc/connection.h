/* Copyright (c) 2026 Vihaan Nathan
 *
 * xpc_connection_t: a real listener/peer or client connection backed by
 * one Mach receive right + one send right to the peer (xpc_connection.c).
 * A listener demultiplexes several simultaneous peers off its single
 * receive right by the peer's send-right name (learned from
 * msgh_remote_port on each receive -- see the field-swap note in
 * userland/mach_test/machtest_main.c, the same mechanism this is built
 * on); each accepted peer gets delivered to the listener's own event
 * handler as a plain xpc_object_t of type XPC_TYPE_CONNECTION, exactly
 * like real XPC.
 *
 * Async delivery is real dispatch_async_f() onto the connection's target
 * queue from a dedicated receive pthread blocked in mach_msg() -- there's
 * no dispatch_source_t/kevent in this tree's libdispatch to hang a
 * MACH_RECV event source off instead (see userland/libdispatch's own
 * scope notes), so this is the same "own blocking thread feeding
 * dispatch_async" shape dispatch_after's timer thread already uses.
 */
#ifndef __XPC_CONNECTION_H__
#define __XPC_CONNECTION_H__

#ifdef __cplusplus
extern "C" {
#endif

#define XPC_CONNECTION_MACH_SERVICE_LISTENER ((uint64_t)(1ULL << 0))
#define XPC_CONNECTION_MACH_SERVICE_PRIVILEGED ((uint64_t)(1ULL << 1))

typedef void (*xpc_finalizer_t)(void *value);

/* `name` is accepted for API compatibility but not used for lookup (see
 * xpc.h's scope note) -- XPC_CONNECTION_MACH_SERVICE_LISTENER selects the
 * one well-known service this process tree's bootstrap port names;
 * without that flag this is the client side of the same connection. */
XPC_EXPORT xpc_connection_t xpc_connection_create_mach_service(const char *name, dispatch_queue_t targetq, uint64_t flags);

XPC_EXPORT void xpc_connection_set_target_queue(xpc_connection_t connection, dispatch_queue_t targetq);
XPC_EXPORT void xpc_connection_set_event_handler(xpc_connection_t connection, xpc_handler_t handler);
XPC_EXPORT void xpc_connection_set_event_handler_f(xpc_connection_t connection, void *context, xpc_handler_f_t handler);

XPC_EXPORT void xpc_connection_activate(xpc_connection_t connection);
XPC_EXPORT void xpc_connection_resume(xpc_connection_t connection);
XPC_EXPORT void xpc_connection_suspend(xpc_connection_t connection);
XPC_EXPORT void xpc_connection_cancel(xpc_connection_t connection);

XPC_EXPORT void xpc_connection_send_message(xpc_connection_t connection, xpc_object_t message);
XPC_EXPORT void xpc_connection_send_message_with_reply(xpc_connection_t connection, xpc_object_t message,
    dispatch_queue_t replyq, xpc_handler_t handler);
XPC_EXPORT xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t connection, xpc_object_t message);

XPC_EXPORT void xpc_connection_set_context(xpc_connection_t connection, void *context);
XPC_EXPORT void *xpc_connection_get_context(xpc_connection_t connection);
XPC_EXPORT void xpc_connection_set_finalizer_f(xpc_connection_t connection, xpc_finalizer_t finalizer);

#ifdef __cplusplus
}
#endif

#endif /* __XPC_CONNECTION_H__ */
