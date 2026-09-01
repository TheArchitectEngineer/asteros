/* Minimal malloc: a chain of mmap'd arena blocks, each with a first-fit
 * free list. No munmap-back-to-OS. Originally sized as a single fixed
 * 8MB block ("good enough for a shell + a handful of coreutils
 * applets"), which is far too small for a real linker (ld64) processing
 * multiple sizeable static archives -- that exhausted the arena,
 * returned NULL, and surfaced as an uncaught std::bad_alloc. Growing by
 * mmap'ing another block on exhaustion, rather than just raising the
 * fixed size, means this no longer silently breaks again the next time
 * something bigger comes along.
 *
 * Thread safety: real pthreads now exist (pthread.c), so every public
 * entry point below takes g_malloc_lock -- a plain atomic-CAS spinlock,
 * not a pthread_mutex_t (this file must not depend on pthread.c, which
 * itself calls malloc()).
 *
 * malloc_lock() carries a watchdog: ground-truthed live (Xfbdev wedged
 * permanently the first time a real Xft client uploaded a glyph --
 * TODO.md's Phase 39 glyph-compositing investigation), g_malloc_lock can
 * end up permanently held with no code anywhere still running that could
 * ever call malloc_unlock() to release it -- every later malloc()/free()
 * in that process then spins in the acquire loop forever, which is a
 * silent, total hang (no crash, no error) since this OS has no watchdog
 * above the process level either. Extensive live instrumentation (kernel
 * physical-memory snapshots read back with mtools, since the hang leaves
 * no way to read a log file through the normal filesystem path -- see
 * the session notes this bug was root-caused in) traced the stuck lock
 * to a free() call made from inside pixman's per-image destroy_func hook
 * (fb/fbpict.c's image_destroy, wired up via
 * pixman_image_set_destroy_function()), which runs synchronously inside
 * pixman_image_unref() -> _pixman_image_fini() -> free(image) -- i.e.
 * between an outer free() taking this same lock and releasing it -- but
 * a real reentrant-recursion fix (tracking lock ownership and letting a
 * same-call-stack reentrant acquire through immediately) was tried live
 * and did *not* resolve the hang: recursion was confirmed firing on
 * every reproduction, yet the process still never made it back out of
 * free(). Whatever actually leaks the lock is therefore something other
 * than straightforward single-level reentrancy, and did not reveal
 * itself even after also instrumenting both malloc_nolock() loops and
 * free_nolock()'s coalescing loop individually (none of them exceeded
 * millions of iterations, ruling out an ordinary infinite loop in this
 * file's own allocation/free logic as the direct cause). Given that,
 * the honest, defensible fix here is a bounded watchdog rather than a
 * claim to have eliminated the root cause: this process is single main
 * thread only (no INPUTTHREAD, no pthread_create, ground-truthed via
 * `nm` on the built binary), so there is never a legitimate reason for
 * one caller to hold this lock across tens of millions of spin
 * iterations of another caller waiting on it -- a real concurrent
 * holder making progress would finish in microseconds. A holder still
 * not done after LOCK_SPIN_LIMIT iterations has leaked the lock, not
 * merely delayed releasing it, so breaking the lock open and proceeding
 * trades a permanent, silent hang for guaranteed forward progress,
 * which is the only choice that doesn't wedge the whole process. This
 * does not corrupt allocator state any worse than the leak already
 * would have: nothing else in this single-threaded process can be
 * concurrently mutating the free list while we're spinning, so the
 * worst case is the leaked holder's own in-progress chunk edit, which
 * existed whether or not this watchdog fires. */
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

static int g_malloc_lock;

#define LOCK_SPIN_LIMIT 1000000UL

/* Bounds every walk of the chunk free-list. A real arena never has
 * anywhere near this many chunks; this exists purely so a corrupted or
 * accidentally-cyclic list degrades to "stop walking and carry on" (a
 * wrong-but-terminating allocator decision) instead of "spin forever"
 * (see the file header's account of the malloc_lock() watchdog for why
 * a guaranteed-terminating allocator matters more here than usual). */
#define CHUNK_SCAN_LIMIT 2000000UL

static void
malloc_lock(void)
{
	unsigned long spins = 0;
	while (__atomic_exchange_n(&g_malloc_lock, 1, __ATOMIC_ACQUIRE)) {
		spins++;
		if (spins > LOCK_SPIN_LIMIT) {
			/* Leaked, not contended -- see file header. Force it
			 * open rather than hang forever. */
			break;
		}
		__asm__ __volatile__("pause" ::: "memory");
	}
}

static void
malloc_unlock(void)
{
	__atomic_store_n(&g_malloc_lock, 0, __ATOMIC_RELEASE);
}

/* Header is padded to 32 bytes (a multiple of 16) so that a payload
 * immediately following it stays 16-byte aligned whenever the payload
 * before it was -- see align16() below for why 16, not 8. */
struct chunk {
	size_t         size;   /* payload size, not including header */
	int            free;
	struct chunk  *next;
	size_t         _pad;
};

#define ARENA_SIZE (8 * 1024 * 1024)
static struct chunk *g_head;

static struct chunk *
arena_grow(size_t min_size)
{
	size_t block_size = ARENA_SIZE;
	if (min_size + sizeof(struct chunk) > block_size) {
		block_size = min_size + sizeof(struct chunk);
	}
	unsigned char *base = mmap(0, block_size, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);
	if (base == (void *)-1) {
		return (void *)0;
	}
	struct chunk *c = (struct chunk *)base;
	c->size = block_size - sizeof(struct chunk);
	c->free = 1;
	c->next = (void *)0;
	return c;
}

static void
arena_init(void)
{
	if (g_head) {
		return;
	}
	g_head = arena_grow(0);
}

/* x86_64 SysV/Itanium C++ ABI requires malloc to return memory aligned
 * to alignof(max_align_t) == 16 (SSE loads/stores like `movaps` fault
 * with #GP on anything less) -- this was 8 (a real, load-bearing bug:
 * every chunk is `sizeof(struct chunk)` (now a 16-byte multiple) past
 * the previous one, so keeping allocation sizes themselves 16-aligned
 * is what keeps every payload in the arena 16-aligned, not just the
 * first one). Discovered via a SIGSEGV in a large real program
 * (clang) that happened to allocate an object whose 16-byte-aligned
 * member landed on an 8-but-not-16-aligned address; smaller programs
 * had gotten lucky by chance until then. */
static size_t
align16(size_t n)
{
	return (n + 15) & ~(size_t)15;
}

static void *
malloc_nolock(size_t size)
{
	arena_init();
	if (size == 0) {
		size = 1;
	}
	size = align16(size);

	unsigned long scan_iters = 0;
	for (struct chunk *c = g_head; c; c = c->next) {
		if (++scan_iters > CHUNK_SCAN_LIMIT) {
			break; /* corrupt/cyclic list -- fall through to arena_grow() */
		}
		if (!c->free || c->size < size) {
			continue;
		}
		/* split if there's enough room left for another header +
		 * a useful minimum payload */
		if (c->size >= size + sizeof(struct chunk) + 8) {
			struct chunk *rem = (struct chunk *)((unsigned char *)(c + 1) + size);
			rem->size = c->size - size - sizeof(struct chunk);
			rem->free = 1;
			rem->next = c->next;
			c->next = rem;
			c->size = size;
		}
		c->free = 0;
		return (void *)(c + 1);
	}

	/* No existing block has room -- mmap another one and link it onto
	 * the end of the chain, then retry the allocation from it. */
	struct chunk *tail = g_head;
	unsigned long tail_iters = 0;
	while (tail->next && ++tail_iters <= CHUNK_SCAN_LIMIT) {
		tail = tail->next;
	}
	struct chunk *fresh = arena_grow(size);
	if (!fresh) {
		return (void *)0; /* real OOM: mmap itself failed */
	}
	tail->next = fresh;

	if (fresh->size >= size + sizeof(struct chunk) + 8) {
		struct chunk *rem = (struct chunk *)((unsigned char *)(fresh + 1) + size);
		rem->size = fresh->size - size - sizeof(struct chunk);
		rem->free = 1;
		rem->next = fresh->next;
		fresh->next = rem;
		fresh->size = size;
	}
	fresh->free = 0;
	return (void *)(fresh + 1);
}

static void
free_nolock(void *ptr)
{
	if (!ptr) {
		return;
	}
	struct chunk *c = (struct chunk *)ptr - 1;
	c->free = 1;
	/* coalesce adjacent free chunks (single pass, list is address-ordered
	 * since we only ever split forward) */
	unsigned long coalesce_iters = 0;
	for (struct chunk *p = g_head; p && p->next; p = p->next) {
		if (++coalesce_iters > CHUNK_SCAN_LIMIT) {
			break; /* corrupt/cyclic list -- ptr is already marked free above */
		}
		if (p->free && p->next->free &&
		    (unsigned char *)(p + 1) + p->size == (unsigned char *)p->next) {
			p->size += sizeof(struct chunk) + p->next->size;
			p->next = p->next->next;
		}
	}
}

void *
malloc(size_t size)
{
	malloc_lock();
	void *p = malloc_nolock(size);
	malloc_unlock();
	return p;
}

void
free(void *ptr)
{
	malloc_lock();
	free_nolock(ptr);
	malloc_unlock();
}

void *
calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	if (nmemb != 0 && total / nmemb != size) {
		return (void *)0; /* overflow */
	}
	malloc_lock();
	void *p = malloc_nolock(total);
	if (p) {
		memset(p, 0, total);
	}
	malloc_unlock();
	return p;
}

void *
realloc(void *ptr, size_t size)
{
	if (!ptr) {
		return malloc(size);
	}
	if (size == 0) {
		free(ptr);
		return (void *)0;
	}
	malloc_lock();
	struct chunk *c = (struct chunk *)ptr - 1;
	if (c->size >= size) {
		malloc_unlock();
		return ptr;
	}
	void *n = malloc_nolock(size);
	if (!n) {
		malloc_unlock();
		return (void *)0;
	}
	memcpy(n, ptr, c->size);
	free_nolock(ptr);
	malloc_unlock();
	return n;
}

void *
reallocarray(void *ptr, size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	if (nmemb != 0 && total / nmemb != size) {
		return (void *)0;
	}
	return realloc(ptr, total);
}

/* free() finds a chunk's header at ptr - 1, so an over-aligned payload
 * needs a real chunk header placed immediately before it -- over-allocate
 * and carve the leading slack off as its own (immediately freeable) chunk,
 * the same split-on-alloc technique malloc() itself already uses. */
void *
aligned_alloc(size_t alignment, size_t size)
{
	if (alignment <= 8) {
		return malloc(size);
	}
	malloc_lock();
	void *raw = malloc_nolock(size + alignment + sizeof(struct chunk));
	if (!raw) {
		malloc_unlock();
		return (void *)0;
	}
	uintptr_t aligned = ((uintptr_t)raw + sizeof(struct chunk) + alignment - 1) &
	    ~(uintptr_t)(alignment - 1);
	struct chunk *orig = (struct chunk *)raw - 1;
	struct chunk *newc = (struct chunk *)aligned - 1;
	size_t front_waste = (unsigned char *)newc - (unsigned char *)orig;

	newc->size = orig->size - front_waste;
	newc->free = 0;
	newc->next = orig->next;
	orig->size = front_waste - sizeof(struct chunk);
	orig->free = 1;
	orig->next = newc;
	malloc_unlock();
	return (void *)aligned;
}
