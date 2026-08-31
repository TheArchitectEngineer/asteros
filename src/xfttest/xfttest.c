/* Verification client for the real FreeType2+fontconfig+Xft stack
 * built in this GTK-port phase (see TODO.md's "GTK3 port, step 1"
 * entry). Same plain-Xlib window/event-loop shape as xeyes/xclock,
 * but draws its text through the real libXft (build/gtk-deps-install)
 * instead of src/xft-stub's 8x8 bitmap stand-in -- the concrete,
 * visually-checkable proof this phase set out to deliver: real
 * antialiased FreeType glyph rendering via XRender, not blocky
 * monochrome bitmap text.
 */
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
draw(Display *dpy, Window win, XftDraw *draw, XftFont *font, XftColor *color)
{
	XClearWindow(dpy, win);
	const char *msg = "AsterOS real Xft \xe2\x80\x94 antialiased text";
	XftDrawStringUtf8(draw, color, font, 20, 60,
	    (const FcChar8 *)msg, (int)strlen(msg));

	const char *msg2 = "ABCDEFGHIJKLM abcdefghijklm 0123456789";
	XftDrawStringUtf8(draw, color, font, 20, 110,
	    (const FcChar8 *)msg2, (int)strlen(msg2));
}

/* Debug-only: make any X protocol error impossible to miss. Without
 * this, an async error (e.g. from a broken RenderCompositeGlyphs
 * request) only surfaces whenever Xlib next happens to drain the
 * socket, which could be well after the relevant draw call -- hard to
 * pin down which request actually failed. */
static int
xerror_handler(Display *d, XErrorEvent *ev)
{
	char buf[128];
	XGetErrorText(d, ev->error_code, buf, sizeof(buf));
	fprintf(stderr, "xfttest: X ERROR: %s (request_code=%d minor_code=%d serial=%lu)\n",
	    buf, ev->request_code, ev->minor_code, ev->serial);
	return 0;
}

int
main(void)
{
	XSetErrorHandler(xerror_handler);

	Display *dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		fprintf(stderr, "xfttest: cannot open display\n");
		return 1;
	}
	/* Synchronous mode: force each request's error (if any) to be
	 * reported immediately after the call that caused it, not
	 * batched/delayed. Debug-only, deliberately slow. */
	XSynchronize(dpy, True);

	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);

	Window win = XCreateSimpleWindow(dpy, root, 100, 100, 520, 160, 2,
	    BlackPixel(dpy, screen), WhitePixel(dpy, screen));

	XStoreName(dpy, win, "Xft Test");
	XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
	XMapWindow(dpy, win);

	XftFont *font = XftFontOpenName(dpy, screen, "DejaVu Sans-24");
	if (font == NULL) {
		fprintf(stderr, "xfttest: XftFontOpenName failed\n");
		return 1;
	}
	fprintf(stderr, "xfttest: font opened, ascent=%d descent=%d height=%d max_advance_width=%d\n",
	    font->ascent, font->descent, font->height, font->max_advance_width);

	/* Prove a real, specific font file was matched by fontconfig
	 * (not a null/empty fallback pattern) -- independent of whether
	 * pixels actually land on screen, this is definitive textual
	 * proof the real freetype2+fontconfig+Xft stack resolved a real
	 * on-disk font, printed to the launching shell's stdout/stderr. */
	FcChar8 *matched_file = NULL;
	FcChar8 *matched_family = NULL;
	if (FcPatternGetString(font->pattern, FC_FILE, 0, &matched_file) == FcResultMatch)
		fprintf(stderr, "xfttest: matched font file: %s\n", matched_file);
	else
		fprintf(stderr, "xfttest: FC_FILE not found in matched pattern\n");
	if (FcPatternGetString(font->pattern, FC_FAMILY, 0, &matched_family) == FcResultMatch)
		fprintf(stderr, "xfttest: matched font family: %s\n", matched_family);

	Visual *visual = DefaultVisual(dpy, screen);
	Colormap cmap = DefaultColormap(dpy, screen);
	XftDraw *xftdraw = XftDrawCreate(dpy, win, visual, cmap);

	XftColor color;
	XRenderColor rcolor = { 0x0000, 0x0000, 0x8000, 0xffff }; /* dark blue */
	XftColorAllocValue(dpy, visual, cmap, &rcolor, &color);

	/* Draw eagerly, once, right away -- don't rely solely on an
	 * Expose event's timing/delivery (e.g. under a slow emulated
	 * display or an unusual window-manager reparenting sequence) to
	 * get real content on screen for verification. */
	draw(dpy, win, xftdraw, font, &color);
	XSync(dpy, False);
	fprintf(stderr, "xfttest: initial draw() done, XSync returned (no error above means every request round-tripped clean)\n");

	for (;;) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.type == Expose && ev.xexpose.count == 0) {
			draw(dpy, win, xftdraw, font, &color);
			fprintf(stderr, "xfttest: draw() on Expose\n");
		} else if (ev.type == KeyPress) {
			break;
		}
	}

	XftColorFree(dpy, visual, cmap, &color);
	XftDrawDestroy(xftdraw);
	XftFontClose(dpy, font);
	XCloseDisplay(dpy);
	return 0;
}
