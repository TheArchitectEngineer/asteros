/* Real BSD socket syscalls -- AF_UNIX transport for the X11 milestone's
 * Xorg server (/tmp/.X11-unix/X0). Ground-truthed syscall numbers from
 * src/xnu/bsd/kern/syscalls.master (same discipline as syscall_raw.h's
 * other numbers): accept=30, getpeername=31, getsockname=32, select=93,
 * socket=97, connect=98, bind=104, setsockopt=105, listen=106,
 * getsockopt=118, shutdown=134, socketpair=135, poll=230 (already
 * implemented in syscalls.c). Several of these (bind/connect/listen/
 * accept/getpeername/getsockname/socketpair) are marked NO_SYSCALL_STUB
 * in syscalls.master -- that only means Apple's own libsyscall stub
 * generator skips them (they need hand-written asm in real Darwin,
 * typically for pthread-cancellation wrapping), not that the raw
 * syscall ABI itself is unusual; a plain raw_syscallN works like any
 * other syscall here since this project has no cancellation points.
 *
 * No real network (inet) stack exists in this kernel config -- see
 * net_stub.c for the DNS/hostname stubs that stay stubbed. This file is
 * only about the syscalls themselves, which work for any socket domain
 * the kernel supports; AF_UNIX is the one this project actually
 * exercises. */
#include "syscall_raw.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>

int
socket(int domain, int type, int protocol)
{
	return (int)sys_result(raw_syscall3(97 /* SYS_socket */, domain, type, protocol));
}

int
bind(int s, const struct sockaddr * addr, socklen_t len)
{
	return (int)sys_result(raw_syscall3(104 /* SYS_bind */, s, (long)addr, len));
}

int
connect(int s, const struct sockaddr * addr, socklen_t len)
{
	return (int)sys_result(raw_syscall3(98 /* SYS_connect */, s, (long)addr, len));
}

int
listen(int s, int backlog)
{
	return (int)sys_result(raw_syscall2(106 /* SYS_listen */, s, backlog));
}

int
accept(int s, struct sockaddr * addr, socklen_t * len)
{
	return (int)sys_result(raw_syscall3(30 /* SYS_accept */, s, (long)addr, (long)len));
}

int
getsockname(int s, struct sockaddr * addr, socklen_t * len)
{
	return (int)sys_result(raw_syscall3(32 /* SYS_getsockname */, s, (long)addr, (long)len));
}

int
getpeername(int s, struct sockaddr * addr, socklen_t * len)
{
	return (int)sys_result(raw_syscall3(31 /* SYS_getpeername */, s, (long)addr, (long)len));
}

int
setsockopt(int s, int level, int optname, const void * optval, socklen_t optlen)
{
	return (int)sys_result(raw_syscall5(105 /* SYS_setsockopt */, s, level, optname, (long)optval, optlen));
}

int
getsockopt(int s, int level, int optname, void * optval, socklen_t * optlen)
{
	return (int)sys_result(raw_syscall5(118 /* SYS_getsockopt */, s, level, optname, (long)optval, (long)optlen));
}

int
shutdown(int s, int how)
{
	return (int)sys_result(raw_syscall2(134 /* SYS_shutdown */, s, how));
}

int
socketpair(int domain, int type, int protocol, int * sv)
{
	return (int)sys_result(raw_syscall4(135 /* SYS_socketpair */, domain, type, protocol, (long)sv));
}

/* send/recv/friends: no dedicated send(2)/recv(2) syscalls in this
 * syscalls.master (they're historically implemented in real Darwin as
 * thin wrappers around sendto/recvfrom or as Libc-side conveniences over
 * write/read); ground-truthed here as the read/write-equivalent path,
 * which is what the kernel's socket vnode-less fd path actually accepts.
 * sendto/recvfrom need real syscall numbers -- both present in
 * syscalls.master. */
ssize_t
sendto(int s, const void * buf, size_t len, int flags, const struct sockaddr * to, socklen_t tolen)
{
	return sys_result(raw_syscall6(133 /* SYS_sendto */, s, (long)buf, (long)len, flags, (long)to, tolen));
}

ssize_t
send(int s, const void * buf, size_t len, int flags)
{
	return sendto(s, buf, len, flags, (const struct sockaddr *)0, 0);
}

ssize_t
recvfrom(int s, void * buf, size_t len, int flags, struct sockaddr * from, socklen_t * fromlen)
{
	return sys_result(raw_syscall6(29 /* SYS_recvfrom */, s, (long)buf, (long)len, flags, (long)from, (long)fromlen));
}

ssize_t
recv(int s, void * buf, size_t len, int flags)
{
	return recvfrom(s, buf, len, flags, (struct sockaddr *)0, (socklen_t *)0);
}

/* select(2): fd_set is a fixed-size (FD_SETSIZE-bit) bitmask array
 * (sys/_types/_fd_set.h) -- the kernel just wants pointers to caller-
 * allocated arrays (or NULL), same as any other in/out buffer arg. */
int
select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout)
{
	return (int)sys_result(raw_syscall5(93 /* SYS_select */, nfds, (long)readfds, (long)writefds, (long)exceptfds, (long)timeout));
}
