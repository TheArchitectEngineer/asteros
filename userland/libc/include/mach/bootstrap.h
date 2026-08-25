/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Mach bootstrap interfaces.
 *
 * Real bootstrap_register()/bootstrap_look_up() for this project's own
 * named-service registry, hosted by launchd
 * (userland/launchd/bootstrap_server.c) -- see mach_bootstrap.c for the
 * wire protocol these two functions speak. Not Apple's actual bootstrap
 * protocol (no register2/look_up2/look_up3, no BSD process info, no
 * per-message audit token) -- this project's own design, same "nothing
 * outside this OS's own process pairs ever needs to decode it" scope as
 * libxpc's wire format.
 */
#ifndef _MACH_BOOTSTRAP_H_
#define _MACH_BOOTSTRAP_H_

#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/port.h>

#define BOOTSTRAP_MAX_NAME_LEN 128

/* Real Apple bootstrap status codes (servers/bootstrap_defs.h), kept
 * here since this project has no separate bootstrap_defs.h. This
 * project's own bootstrap_look_up()/bootstrap_register() (see the
 * header comment above) never actually return any of these -- only
 * plain kern_return_t values -- so real callers that switch on them
 * (e.g. SCDOpen.c's __SCDynamicStoreServerPort()) always fall through
 * to their own default case here; the constants exist so that real,
 * vendored source referencing them by name still compiles. */
#define BOOTSTRAP_SUCCESS          0
#define BOOTSTRAP_NOT_PRIVILEGED   1100
#define BOOTSTRAP_NAME_IN_USE      1101
#define BOOTSTRAP_UNKNOWN_SERVICE  1102
#define BOOTSTRAP_SERVICE_ACTIVE   1103
#define BOOTSTRAP_BAD_COUNT        1104
#define BOOTSTRAP_NO_MEMORY        1105
#define BOOTSTRAP_NO_CHILDREN      1106

#define BOOTSTRAP_REGISTER_MSGH_ID 9000
#define BOOTSTRAP_LOOK_UP_MSGH_ID  9001

/* Publishes `service_port` (a send right the caller keeps its own copy
 * of) under `name` in launchd's registry. Replacing an existing name's
 * entry is allowed (a respawned daemon re-registering after a crash) --
 * the previous holder's own rights are unaffected. */
kern_return_t bootstrap_register(mach_port_t bp, const char *name, mach_port_t service_port);

/* Looks up `name` in launchd's registry. On success, *service_port is a
 * fresh send right the caller owns. Returns KERN_INVALID_NAME (via
 * launchd's own reply -- this project doesn't need a real registry-
 * specific error enum) if nothing is registered under that name. */
kern_return_t bootstrap_look_up(mach_port_t bp, const char *name, mach_port_t *service_port);

#endif /* _MACH_BOOTSTRAP_H_ */
