/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchd's real bootstrap-namespace registry -- see bootstrap_server.c
 * for the mechanism.
 */
#ifndef LAUNCHD_BOOTSTRAP_SERVER_H
#define LAUNCHD_BOOTSTRAP_SERVER_H

/* Installs launchd's own TASK_BOOTSTRAP_PORT (every daemon forked
 * afterward inherits a send right to it automatically) and starts the
 * registry's server thread. Must be called before the first
 * spawn_daemon() so no daemon can start with the wrong bootstrap port. */
void bootstrap_server_start(void);

#endif /* LAUNCHD_BOOTSTRAP_SERVER_H */
