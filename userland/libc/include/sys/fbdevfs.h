/* Userland-side copy of /fbdev/geometry's wire format -- kept in sync
 * by hand with src/xnu/bsd/miscfs/fbdevfs/fbdevfs.h, same pattern every
 * other kernel/userland ABI boundary in this tree uses (this libc has
 * no mechanism to #include a kernel header directly). This project's
 * own design, not a port of Linux's fb_var_screeninfo/fb_fix_screeninfo.
 * Read via a plain read(2) on /fbdev/geometry, not an ioctl on fb0 --
 * an ioctl was tried first, but real Darwin's vn_ioctl() only lets
 * VCHR/VBLK/VFIFO vnodes reach a filesystem's own VNOP_IOCTL, and fb0
 * is deliberately a VREG (see the kernel header's comment). */
#ifndef _SYS_FBDEVFS_H_
#define _SYS_FBDEVFS_H_

#include <stdint.h>

struct fbdevfs_screeninfo {
	uint32_t width;
	uint32_t height;
	uint32_t stride; /* bytes per row */
	uint32_t depth;  /* bits per pixel */
};

#endif /* _SYS_FBDEVFS_H_ */
