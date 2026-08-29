/* Copyright (c) 2026 Vihaan Nathan
 *
 * We link syscalls.o directly (not through an archive), so ld64 pulls in
 * the whole translation unit -- including execv()/execvp(), which we
 * never call but which reference `environ`/getenv() (normally provided
 * by start.c, which we deliberately don't link: it pulls in __libc_start
 * and its own undefined reference to main()). Cheaper to satisfy the two
 * stray references directly than to link start.c for symbols dyld itself
 * never uses.
 */
char **environ;

char *
getenv(const char *name)
{
	(void)name;
	return 0;
}

/* syscalls.o's fork()/vfork() (also never called by dyld -- it never
 * forks) reference these two real-pthread/mach-init entry points for
 * post-fork bookkeeping. Pulling in pthread.c/mach_msg.c for symbols on
 * a dead path would drag in the whole threading + mach message
 * subsystem dyld has no other use for, so stub them instead. */
void
__mach_init_task_self(void)
{
}

static int dyld_errno;

int *
__errno_location(void)
{
	return &dyld_errno;
}
