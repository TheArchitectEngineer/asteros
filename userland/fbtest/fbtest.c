/* Proof that fbdevfs (src/xnu/bsd/miscfs/fbdevfs/) actually delivers real
 * userspace mmap() access to the GOP linear framebuffer -- open()s
 * /fbdev/fb0, mmap()s the whole thing, writes a two-color pattern directly
 * into it (visible on the real display via a QEMU monitor screendump, the
 * one truly independent proof this data reached actual video memory and
 * not just a private copy), and separately reads the bytes back both
 * through the mapping and through a real read(2) at a nonzero offset to
 * confirm the VNOP_READ path (fbdevfs_read) agrees with what VNOP_MMAP
 * wrote -- two independent code paths into the same physical pages.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int
main(void)
{
	int fd = open("/fbdev/fb0", O_RDWR);
	if (fd < 0) {
		printf("FBTEST FAIL: open(/fbdev/fb0) failed\n");
		return 1;
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		printf("FBTEST FAIL: fstat failed, errno=%d\n", errno);
		return 1;
	}
	size_t fb_size = (size_t)st.st_size;
	if (fb_size < 4) {
		printf("FBTEST FAIL: fb0 size too small (%llu)\n", (unsigned long long)fb_size);
		return 1;
	}
	printf("FBTEST: /fbdev/fb0 size = %llu bytes\n", (unsigned long long)fb_size);

	uint8_t *fb = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED) {
		printf("FBTEST FAIL: mmap failed\n");
		return 1;
	}

	/* Solid-color top/bottom split, stride-agnostic (no ioctl for real
	 * width/height/stride exists yet -- see fbdevfs.h) -- a plain byte
	 * offset split is enough to prove real, full-range writes landed in
	 * video memory, without needing exact row geometry. */
	uint32_t magenta = 0xFFFF00FFu;
	uint32_t cyan    = 0xFF00FFFFu;
	size_t npixels = fb_size / 4;
	uint32_t *px = (uint32_t *)(void *)fb;
	for (size_t i = 0; i < npixels; i++) {
		px[i] = (i < npixels / 2) ? magenta : cyan;
	}

	if (px[0] != magenta || px[npixels - 1] != cyan) {
		printf("FBTEST FAIL: mmap readback mismatch\n");
		return 1;
	}

	/* Independent verification through VNOP_READ at a nonzero offset,
	 * not just the mmap mapping used to write it. */
	off_t probe_off = (off_t)((npixels / 2) * 4);
	if (lseek(fd, probe_off, SEEK_SET) != probe_off) {
		printf("FBTEST FAIL: lseek failed\n");
		return 1;
	}
	uint32_t rd = 0;
	if (read(fd, &rd, sizeof(rd)) != (ssize_t)sizeof(rd)) {
		printf("FBTEST FAIL: read failed\n");
		return 1;
	}
	if (rd != cyan) {
		printf("FBTEST FAIL: read(2) at offset %lld got 0x%08x, expected 0x%08x\n",
		    (long long)probe_off, rd, cyan);
		return 1;
	}

	printf("FBTEST PASS\n");
	return 0;
}
