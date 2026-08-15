/* Host-side kext linker driver for the prelinked-kext pipeline (Phase 23,
 * see TODO.md). Loads a compiled kext bundle (MH_KEXT_BUNDLE, undefined
 * externs against KPI symbols) and the running kernel's own Mach-O image,
 * and calls kxld_link_file() -- the exact same real, unmodified Apple
 * linker real `kextcache` uses -- to produce a fully-linked kext with
 * zero remaining external relocations, matching what
 * OSKext::withPrelinkedInfoDict()'s slidePrelinkedExecutable() requires
 * (it hard-fails on any external relocation still present -- prelinked
 * kexts must arrive pre-resolved, not linked live at boot; ground-truthed
 * by reading OSKext.cpp before designing this tool, not assumed).
 *
 * Usage: kxld_link_tool <kernel_macho> <kext_bundle> <target_vmaddr_hex> <out_linked_kext> <out_kmod_info_addr_file>
 *
 * target_vmaddr_hex is the intended final load address of this kext once
 * merged into the kernel's __PRELINK_TEXT segment (computed by the caller
 * -- prelink_merge.py -- ahead of time, since Mach-O segment placement
 * has to be decided before linking, not after).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mach/machine.h>

#include "kxld.h"
#include "kxld_types.h"

static void *
read_whole_file(const char *path, size_t *out_size)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		exit(1);
	}
	struct stat st;
	if (fstat(fd, &st) != 0) {
		perror("fstat");
		exit(1);
	}
	/* kxld.h: "the object data itself must be mmapped with PROT_WRITE
	 * and MAP_PRIVATE" -- kxld patches relocations in place. */
	void *p = mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	close(fd);
	*out_size = (size_t)st.st_size;
	return p;
}

static kxld_addr_t g_target_vmaddr;
static size_t g_linked_size; /* real output size, per allocate_callback's own
                               * `size` parameter -- kxld_link_file() has no
                               * separate "linked size" out-param, so this is
                               * the only place that size is ever reported;
                               * NOT necessarily equal to the input kext's
                               * file size (found by reading the callback
                               * contract in kxld.h before assuming, not
                               * after guessing wrong). */

static kxld_addr_t
allocate_callback(size_t size, KXLDAllocateFlags *flags, void *user_data)
{
	(void)user_data;
	g_linked_size = size;
	/* Not writable: we're linking offline, not into a live kernel's
	 * memory, so kxld writes the linked result into its own temporary
	 * buffer (handed back via kxld_link_file's linked_object out-param)
	 * instead of writing through this address directly. The address we
	 * report here is purely what kxld computes relocations/final symbol
	 * values against -- it must match where prelink_merge.py will
	 * actually place this kext's bytes in the kernel image. */
	*flags = kKxldAllocateDefault;
	return g_target_vmaddr;
}

static void
logging_callback(KXLDLogSubsystem sys, KXLDLogLevel level, const char *format, va_list ap, void *user_data)
{
	(void)sys;
	(void)user_data;
	if (level > kKxldLogBasic) {
		return; /* skip kKxldLogDetail/kKxldLogDebug -- too noisy for a build tool */
	}
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
}

int
main(int argc, char **argv)
{
	if (argc != 6) {
		fprintf(stderr, "usage: %s <kernel_macho> <kext_bundle> <target_vmaddr_hex> <out_linked_kext> <out_kmod_info_addr_file>\n", argv[0]);
		return 1;
	}

	const char *kernel_path = argv[1];
	const char *kext_path = argv[2];
	g_target_vmaddr = (kxld_addr_t)strtoull(argv[3], NULL, 16);
	const char *out_kext_path = argv[4];
	const char *out_kmodaddr_path = argv[5];

	size_t kernel_size = 0, kext_size = 0;
	void *kernel_bytes = read_whole_file(kernel_path, &kernel_size);
	void *kext_bytes = read_whole_file(kext_path, &kext_size);

	KXLDContext *context = NULL;
	/* cputype/cpusubtype: NOT 0/0 ("host arch") -- this tool's own
	 * process may run under Rosetta or natively on either Apple Silicon
	 * or Intel build machines, and empirically kxld's "0 means host"
	 * resolution picked up the *physical* host architecture rather than
	 * this process's own (x86_64, forced via -arch), producing "Invalid
	 * magic number" on an entirely valid x86_64 Mach-O -- caught live,
	 * not anticipated. This target is always x86_64 regardless of the
	 * build machine, so state it explicitly. */
	kern_return_t kr = kxld_create_context(&context, allocate_callback,
	    logging_callback, /*flags*/ 0, CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL, /*pagesize*/ 0);
	if (kr != 0 || !context) {
		fprintf(stderr, "kxld_create_context failed: 0x%x\n", kr);
		return 1;
	}

	/* The kernel image itself is the sole dependency -- resolves every
	 * KPI symbol (com.apple.kpi.iokit/libkern/mach/bsd all live in the
	 * one running kernel binary in this project, not separate KPI kext
	 * images -- ground-truthed: OSKext::initialize() points
	 * sKernelKext's linkedExecutable at the whole live kernel Mach-O). */
	KXLDDependency dep;
	memset(&dep, 0, sizeof(dep));
	dep.kext = (u_char *)kernel_bytes;
	dep.kext_size = (u_long)kernel_size;
	dep.kext_name = "com.apple.kernel";
	dep.interface = NULL;
	dep.interface_size = 0;
	dep.interface_name = NULL;
	dep.is_direct_dependency = TRUE;

	u_char *linked_object = NULL;
	kxld_addr_t kmod_info_kern = 0;

	kr = kxld_link_file(context, (u_char *)kext_bytes, (u_long)kext_size,
	    "com.asteros.HelloKext", /*callback_data*/ NULL,
	    &dep, 1, &linked_object, &kmod_info_kern);
	if (kr != 0) {
		fprintf(stderr, "kxld_link_file failed: 0x%x\n", kr);
		return 1;
	}

	fprintf(stderr, "kxld_link_file succeeded: kmod_info_kern=0x%llx linked_size=0x%zx\n",
	    (unsigned long long)kmod_info_kern, g_linked_size);

	FILE *outf = fopen(out_kext_path, "wb");
	if (!outf) {
		perror(out_kext_path);
		return 1;
	}
	fwrite(linked_object, 1, g_linked_size, outf);
	fclose(outf);

	FILE *addrf = fopen(out_kmodaddr_path, "w");
	if (!addrf) {
		perror(out_kmodaddr_path);
		return 1;
	}
	fprintf(addrf, "0x%llx\n", (unsigned long long)kmod_info_kern);
	fclose(addrf);

	kxld_destroy_context(context);
	return 0;
}
