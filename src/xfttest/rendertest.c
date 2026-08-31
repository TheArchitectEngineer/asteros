/* Minimal isolation test for the Phase 39 glyph-compositing hang:
 * calls only XRenderCreateGlyphSet (no XRenderAddGlyphs) to determine
 * whether the hang is in glyph-SET creation itself or specifically in
 * adding a glyph to it. See TODO.md's Phase 39 entry.
 */
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>

int
main(void)
{
	Display *dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		fprintf(stderr, "rendertest: cannot open display\n");
		return 1;
	}
	XSynchronize(dpy, True);

	int screen = DefaultScreen(dpy);
	fprintf(stderr, "rendertest: display open, screen=%d\n", screen);

	XRenderPictFormat *fmt = XRenderFindStandardFormat(dpy, PictStandardA8);
	fprintf(stderr, "rendertest: XRenderFindStandardFormat(A8) = %p\n", (void *)fmt);
	if (!fmt) {
		fprintf(stderr, "rendertest: no A8 format, aborting\n");
		return 1;
	}

	fprintf(stderr, "rendertest: calling XRenderCreateGlyphSet...\n");
	GlyphSet gs = XRenderCreateGlyphSet(dpy, fmt);
	fprintf(stderr, "rendertest: XRenderCreateGlyphSet returned gs=0x%lx\n", (unsigned long)gs);

	XSync(dpy, False);
	fprintf(stderr, "rendertest: XSync after CreateGlyphSet returned cleanly\n");

	XCloseDisplay(dpy);
	fprintf(stderr, "rendertest: done\n");
	return 0;
}
