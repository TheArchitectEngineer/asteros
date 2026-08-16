/* Copyright (c) 2026 Vihaan Nathan
 *
 * Server side of launchd_control.h's protocol -- see control_server.c.
 */
#ifndef LAUNCHD_CONTROL_SERVER_H
#define LAUNCHD_CONTROL_SERVER_H

/* Allocates a receive right, registers it under LCTL_SERVICE_NAME against
 * launchd's own bootstrap registry (bootstrap_server_start() must already
 * have run), and starts the control server's thread. Safe to call any
 * time after bootstrap_server_start() -- unlike that call, this doesn't
 * need to happen before the first spawn_daemon(): launchctl invocations
 * only ever happen after boot, not via fork inheritance. */
void control_server_start(void);

#endif /* LAUNCHD_CONTROL_SERVER_H */
