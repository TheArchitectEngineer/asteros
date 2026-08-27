/* Copyright (c) 2026 Vihaan Nathan
 *
 * Minimal xsetroot-equivalent: paints the root window a solid color.
 * Not vendored from anywhere -- this project never pulled in the real
 * xsetroot (or its app-defaults), and a solid-fill root is all the
 * X11 milestone's wallpaper request needs.
 *
 * Computes the pixel value directly from the default visual's
 * red/green/blue masks instead of going through XAllocColor/a
 * colormap round trip: Xfbdev's framebuffer is TrueColor/DirectColor
 * (a real, direct-mapped GOP surface, not a palette), so there is no
 * shared colormap entry to allocate in the first place -- any pixel
 * value that fits the visual's masks is already valid. Colormap
 * allocation hung indefinitely here live (confirmed: this program
 * never got past its very first fprintf when XAllocColor was still in
 * the path, with the parent shell blocked the whole time), so this
 * sidesteps that round trip rather than chasing why.
 */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long
scale_to_mask(unsigned long component_8bit, unsigned long mask)
{
	int shift = 0;
	unsigned long m = mask;
	while (m && !(m & 1)) {
		shift++;
		m >>= 1;
	}
	int bits = 0;
	while (m & 1) {
		bits++;
		m >>= 1;
	}
	unsigned long scaled = (component_8bit * ((1UL << bits) - 1)) / 255;
	return (scaled << shift) & mask;
}

int
main(int argc, char **argv)
{
	unsigned long r8 = 0xc0, g8 = 0x00, b8 = 0x00;
	if (argc > 3) {
		r8 = strtoul(argv[1], NULL, 0);
		g8 = strtoul(argv[2], NULL, 0);
		b8 = strtoul(argv[3], NULL, 0);
	}

	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "xsetbg: can't open display\n");
		return 1;
	}

	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	Visual *visual = DefaultVisual(dpy, screen);

	unsigned long pixel = scale_to_mask(r8, visual->red_mask) |
	    scale_to_mask(g8, visual->green_mask) |
	    scale_to_mask(b8, visual->blue_mask);

	XSetWindowBackground(dpy, root, pixel);
	XClearWindow(dpy, root);
	XFlush(dpy);
	XCloseDisplay(dpy);
	return 0;
}
