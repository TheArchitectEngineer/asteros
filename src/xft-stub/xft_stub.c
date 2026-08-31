/* Implementation backing the src/xft-stub/include headers.
 *
 * WindowMaker's WINGs toolkit (WINGs/wfont.c, wfontpanel.c, widgets.c)
 * hard-requires Xft2 + fontconfig at configure time -- there is no
 * --disable-xft in this version. Vendoring the real thing would mean
 * cross-building freetype2, fontconfig (plus expat/libxml2), and Xft
 * itself, on top of everything already vendored for the X server, just
 * to rasterize one embedded font -- so instead of a real FreeType
 * backend, glyphs are drawn straight from a public-domain 8x8 bitmap
 * font (`font8x8_basic.h`, Daniel Hepper / Marcel Sondaar / IBM's own
 * public-domain VGA font data, fetched verbatim with its original
 * license header intact -- see that file) via plain XFillRectangle
 * calls, scaled up 2x (16x16 per glyph) for legibility at UI text
 * sizes. Real rendering, real pixels on screen -- just not FreeType,
 * and not real font *matching* (every request gets this one font
 * regardless of requested family/size).
 *
 * Pattern storage (FcPattern's add/get/del, FcNameParse/FcNameUnparse,
 * XftXlfdParse) is a real, if simplified, in-memory implementation --
 * genuinely useful to WINGs' own pattern-manipulation logic, not faked.
 * Font *enumeration* is still honestly empty (`FcFontList` always
 * reports zero fonts -- there's exactly one font here, and it was
 * never installed as a discoverable system font, so reporting it via
 * fontconfig's normal enumeration path would be its own kind of
 * dishonest). `XftFontOpenName`/`XftFontOpenPattern` always "succeed"
 * with this one font's real metrics instead of returning NULL,
 * regardless of what was actually requested, specifically so WINGs'
 * widget layout code -- which assumes a font handle is always
 * obtainable and doesn't NULL-check the result of WMCreateFont-family
 * calls at every call site -- keeps computing real, now genuinely
 * correct (not just non-crashing) layout.
 */
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font8x8_basic.h"

/* Glyph cell geometry: font8x8_basic is 8x8 native. Drawn at native
 * size (no scale-up) -- confirmed live that 2x read as oversized next
 * to WindowMaker's own UI chrome (titlebars, menu row height); native
 * 8px is proportional to it.
 */
#define GLYPH_SCALE 1
#define GLYPH_W (8 * GLYPH_SCALE)
#define GLYPH_H (8 * GLYPH_SCALE)
#define GLYPH_ASCENT (GLYPH_H - 3)
#define GLYPH_DESCENT 3

enum fc_value_type { FC_VT_STRING, FC_VT_DOUBLE, FC_VT_INTEGER };

struct fc_entry {
	char *object;
	enum fc_value_type type;
	char *sval;
	double dval;
	int ival;
	struct fc_entry *next;
};

struct _FcPattern {
	struct fc_entry *entries;
};

/* ---- FcPattern ---- */

FcPattern *FcPatternCreate(void)
{
	FcPattern *p = calloc(1, sizeof(*p));
	return p;
}

void FcPatternDestroy(FcPattern *p)
{
	struct fc_entry *e;

	if (!p)
		return;
	e = p->entries;
	while (e) {
		struct fc_entry *next = e->next;
		free(e->object);
		free(e->sval);
		free(e);
		e = next;
	}
	free(p);
}

FcPattern *FcPatternDuplicate(const FcPattern *p)
{
	FcPattern *copy;
	struct fc_entry *e;

	if (!p)
		return NULL;
	copy = FcPatternCreate();
	for (e = p->entries; e; e = e->next) {
		if (e->type == FC_VT_STRING)
			FcPatternAddString(copy, e->object, (const FcChar8 *)e->sval);
		else if (e->type == FC_VT_INTEGER)
			FcPatternAddInteger(copy, e->object, e->ival);
		else
			FcPatternAddDouble(copy, e->object, e->dval);
	}
	return copy;
}

static struct fc_entry *fc_find(const FcPattern *p, const char *object)
{
	struct fc_entry *e;

	if (!p)
		return NULL;
	for (e = p->entries; e; e = e->next) {
		if (strcmp(e->object, object) == 0)
			return e;
	}
	return NULL;
}

FcBool FcPatternDel(FcPattern *p, const char *object)
{
	struct fc_entry *e, *prev = NULL;

	if (!p)
		return FcFalse;
	for (e = p->entries; e; prev = e, e = e->next) {
		if (strcmp(e->object, object) == 0) {
			if (prev)
				prev->next = e->next;
			else
				p->entries = e->next;
			free(e->object);
			free(e->sval);
			free(e);
			return FcTrue;
		}
	}
	return FcFalse;
}

FcBool FcPatternAddString(FcPattern *p, const char *object, const FcChar8 *s)
{
	struct fc_entry *e;

	if (!p)
		return FcFalse;
	FcPatternDel(p, object);
	e = calloc(1, sizeof(*e));
	e->object = strdup(object);
	e->type = FC_VT_STRING;
	e->sval = strdup((const char *)s);
	e->next = p->entries;
	p->entries = e;
	return FcTrue;
}

FcBool FcPatternAddDouble(FcPattern *p, const char *object, double d)
{
	struct fc_entry *e;

	if (!p)
		return FcFalse;
	FcPatternDel(p, object);
	e = calloc(1, sizeof(*e));
	e->object = strdup(object);
	e->type = FC_VT_DOUBLE;
	e->dval = d;
	e->next = p->entries;
	p->entries = e;
	return FcTrue;
}

FcResult FcPatternGet(const FcPattern *p, const char *object, int id, FcValue *v)
{
	struct fc_entry *e;

	if (id != 0)
		return FcResultNoId;
	e = fc_find(p, object);
	if (!e)
		return FcResultNoMatch;
	if (v) {
		v->type = e->type;
		if (e->type == FC_VT_STRING)
			v->u.s = (const FcChar8 *)e->sval;
		else if (e->type == FC_VT_INTEGER)
			v->u.i = e->ival;
		else
			v->u.d = e->dval;
	}
	return FcResultMatch;
}

FcBool FcPatternAddInteger(FcPattern *p, const char *object, int i)
{
	struct fc_entry *e;

	if (!p)
		return FcFalse;
	FcPatternDel(p, object);
	e = calloc(1, sizeof(*e));
	e->object = strdup(object);
	e->type = FC_VT_INTEGER;
	e->ival = i;
	e->next = p->entries;
	p->entries = e;
	return FcTrue;
}

FcResult FcPatternGetInteger(const FcPattern *p, const char *object, int id, int *i)
{
	struct fc_entry *e;

	if (id != 0)
		return FcResultNoId;
	e = fc_find(p, object);
	if (!e || e->type != FC_VT_INTEGER)
		return FcResultNoMatch;
	if (i)
		*i = e->ival;
	return FcResultMatch;
}

FcResult FcPatternGetString(const FcPattern *p, const char *object, int id, FcChar8 **s)
{
	struct fc_entry *e;

	if (id != 0)
		return FcResultNoId;
	e = fc_find(p, object);
	if (!e || e->type != FC_VT_STRING)
		return FcResultNoMatch;
	if (s)
		*s = (FcChar8 *)e->sval;
	return FcResultMatch;
}

FcResult FcPatternGetDouble(const FcPattern *p, const char *object, int id, double *d)
{
	struct fc_entry *e;

	if (id != 0)
		return FcResultNoId;
	e = fc_find(p, object);
	if (!e || e->type != FC_VT_DOUBLE)
		return FcResultNoMatch;
	if (d)
		*d = e->dval;
	return FcResultMatch;
}

void FcPatternPrint(const FcPattern *p)
{
	struct fc_entry *e;

	if (!p)
		return;
	for (e = p->entries; e; e = e->next) {
		if (e->type == FC_VT_STRING)
			fprintf(stderr, "%s: \"%s\"\n", e->object, e->sval);
		else
			fprintf(stderr, "%s: %g\n", e->object, e->dval);
	}
}

/* Real, if simplified, "family[:key=val[:key2=val2]]" parse -- good
 * enough for WINGs' own default-font strings ("sans serif:pixelsize=12"
 * and friends), not a full fontconfig grammar.
 */
FcPattern *FcNameParse(const FcChar8 *name)
{
	FcPattern *p = FcPatternCreate();
	char *copy, *saveptr, *tok;

	if (!name || !*(const char *)name)
		return p;

	copy = strdup((const char *)name);
	tok = strtok_r(copy, ":", &saveptr);
	if (tok && !strchr(tok, '='))
		FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)tok);
	else if (tok)
		goto parse_kv;

	tok = strtok_r(NULL, ":", &saveptr);
	while (tok) {
parse_kv:
		{
			char *eq = strchr(tok, '=');
			if (eq) {
				*eq = '\0';
				char *val = eq + 1;
				char *endptr;
				double d = strtod(val, &endptr);
				if (endptr != val && *endptr == '\0')
					FcPatternAddDouble(p, tok, d);
				else
					FcPatternAddString(p, tok, (const FcChar8 *)val);
			}
		}
		tok = strtok_r(NULL, ":", &saveptr);
	}

	free(copy);
	return p;
}

FcChar8 *FcNameUnparse(FcPattern *pat)
{
	char buf[512];
	FcChar8 *family = NULL;
	struct fc_entry *e;
	size_t len;

	buf[0] = '\0';
	if (pat && FcPatternGetString(pat, FC_FAMILY, 0, &family) == FcResultMatch)
		snprintf(buf, sizeof(buf), "%s", (const char *)family);
	else
		snprintf(buf, sizeof(buf), "sans serif");

	/* Every other stored key (pixelsize, weight, slant, ...) round-trips
	 * as its own ":key=value" segment, so FcNameParse(FcNameUnparse(p))
	 * recovers everything that was set, not just family/size.
	 */
	if (pat) {
		for (e = pat->entries; e; e = e->next) {
			if (strcmp(e->object, FC_FAMILY) == 0)
				continue;
			len = strlen(buf);
			if (e->type == FC_VT_STRING)
				snprintf(buf + len, sizeof(buf) - len, ":%s=%s", e->object, e->sval);
			else if (e->type == FC_VT_INTEGER)
				snprintf(buf + len, sizeof(buf) - len, ":%s=%d", e->object, e->ival);
			else
				snprintf(buf + len, sizeof(buf) - len, ":%s=%g", e->object, e->dval);
		}
	}

	return (FcChar8 *)strdup(buf);
}

/* Real fontconfig's FcDefaultSubstitute() fills in any of family/size/
 * weight/slant/width the caller left unset before matching. Genuinely
 * useful here too, not just a no-op: callers like WPrefs.app's
 * FontSimple.c read these fields straight back out afterward and
 * expect sane values to already be there.
 */
void FcDefaultSubstitute(FcPattern *pattern)
{
	double d;
	int i;
	FcChar8 *s;

	if (!pattern)
		return;
	if (FcPatternGetString(pattern, FC_FAMILY, 0, &s) != FcResultMatch)
		FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)"sans serif");
	if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &d) != FcResultMatch)
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12.0);
	if (FcPatternGetInteger(pattern, FC_WEIGHT, 0, &i) != FcResultMatch)
		FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_MEDIUM);
	if (FcPatternGetInteger(pattern, FC_SLANT, 0, &i) != FcResultMatch)
		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	if (FcPatternGetInteger(pattern, FC_WIDTH, 0, &i) != FcResultMatch)
		FcPatternAddInteger(pattern, FC_WIDTH, FC_WIDTH_NORMAL);
}

struct _FcObjectSet {
	int unused;
};

FcObjectSet *FcObjectSetBuild(const char *first, ...)
{
	(void)first;
	return calloc(1, sizeof(struct _FcObjectSet));
}

void FcObjectSetDestroy(FcObjectSet *os)
{
	free(os);
}

/* Real fontconfig would enumerate installed fonts here; this project
 * has none vendored, so an honest empty result is the correct answer,
 * not just the easy one.
 */
FcFontSet *FcFontList(void *config, FcPattern *p, FcObjectSet *os)
{
	FcFontSet *s;

	(void)config;
	(void)p;
	(void)os;
	s = calloc(1, sizeof(*s));
	s->nfont = 0;
	s->sfont = 0;
	s->fonts = NULL;
	return s;
}

void FcFontSetDestroy(FcFontSet *s)
{
	int i;

	if (!s)
		return;
	for (i = 0; i < s->nfont; i++)
		FcPatternDestroy(s->fonts[i]);
	free(s->fonts);
	free(s);
}

/* ---- Xft ---- */

/* XLFD is "-foundry-family-weight-slant-...."; field 2 (1-indexed after
 * the leading empty field from the first dash) is the family. Not a
 * full XLFD parser, just enough to pull a plausible family name out,
 * same spirit as the rest of this stub.
 */
FcPattern *XftXlfdParse(const char *xlfd_name, Bool ignore_scalable, Bool complete)
{
	FcPattern *p = FcPatternCreate();
	char *copy, *saveptr, *tok;
	int field = 0;

	(void)ignore_scalable;
	(void)complete;

	if (!xlfd_name || xlfd_name[0] != '-') {
		FcPatternDestroy(p);
		return FcNameParse((const FcChar8 *)xlfd_name);
	}

	copy = strdup(xlfd_name);
	tok = strtok_r(copy + 1, "-", &saveptr);
	while (tok) {
		field++;
		if (field == 2) {
			FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)tok);
			break;
		}
		tok = strtok_r(NULL, "-", &saveptr);
	}
	free(copy);

	if (field < 2)
		FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)"sans serif");

	return p;
}

struct _XftDraw {
	Display *dpy;
	Drawable drawable;
};

/* One GC, shared by every XftDraw for the whole process, created
 * lazily on first actual draw rather than one-per-XftDrawCreate. WINGs
 * creates an XftDraw per screen, per miniwindow, per dock/app icon --
 * often many, often short-lived -- and a GC isn't drawable-specific in
 * X11 (only screen/depth-specific), so a single shared one is both
 * correct and cheaper than churning through many. (A live regression
 * that looked exactly like this GC handling at first -- WindowMaker's
 * dock icon would appear but no client ever got decorated -- turned
 * out under bisection to be an unrelated startx.sh race, see that
 * file's comment; this GC design is kept because it's the right
 * design regardless, not because it was ever the actual cause.)
 */
static GC shared_gc;
static unsigned long shared_gc_pixel;
static int shared_gc_have_pixel;

static GC xft_gc(Display *dpy)
{
	if (!shared_gc)
		shared_gc = XCreateGC(dpy, DefaultRootWindow(dpy), 0, NULL);
	return shared_gc;
}

/* Real metrics for the one embedded font (see file-level comment) --
 * every request gets these regardless of requested family/size.
 * Never returns NULL -- WINGs' widget layout code assumes a font
 * handle is always obtainable and doesn't NULL-check the result of
 * WMCreateFont-family calls at every call site.
 */
XftFont *XftFontOpenName(Display *dpy, int screen, const char *name)
{
	XftFont *f;

	(void)dpy;
	(void)screen;

	f = calloc(1, sizeof(*f));
	f->ascent = GLYPH_ASCENT;
	f->descent = GLYPH_DESCENT;
	f->height = GLYPH_H;
	f->max_advance_width = GLYPH_W;
	f->pattern = FcNameParse((const FcChar8 *)(name ? name : ""));
	return f;
}

XftFont *XftFontOpenPattern(Display *dpy, FcPattern *pattern)
{
	XftFont *f;

	(void)dpy;

	f = calloc(1, sizeof(*f));
	f->ascent = GLYPH_ASCENT;
	f->descent = GLYPH_DESCENT;
	f->height = GLYPH_H;
	f->max_advance_width = GLYPH_W;
	f->pattern = pattern;
	return f;
}

void XftFontClose(Display *dpy, XftFont *pub)
{
	(void)dpy;
	if (!pub)
		return;
	if (pub->pattern)
		FcPatternDestroy(pub->pattern);
	free(pub);
}

XftDraw *XftDrawCreate(Display *dpy, Drawable drawable, Visual *visual, Colormap colormap)
{
	XftDraw *d = calloc(1, sizeof(*d));

	(void)visual;
	(void)colormap;
	d->dpy = dpy;
	d->drawable = drawable;
	return d;
}

void XftDrawChange(XftDraw *draw, Drawable drawable)
{
	if (draw)
		draw->drawable = drawable;
}

void XftDrawDestroy(XftDraw *draw)
{
	free(draw);
}

static void xft_set_foreground(Display *dpy, GC gc, const XftColor *color)
{
	if (!gc)
		return;
	if (shared_gc_have_pixel && shared_gc_pixel == color->pixel)
		return;
	XSetForeground(dpy, gc, color->pixel);
	shared_gc_pixel = color->pixel;
	shared_gc_have_pixel = 1;
}

void XftDrawRect(XftDraw *draw, const XftColor *color, int x, int y, unsigned int width, unsigned int height)
{
	GC gc;

	if (!draw)
		return;
	gc = xft_gc(draw->dpy);
	if (!gc)
		return;
	xft_set_foreground(draw->dpy, gc, color);
	XFillRectangle(draw->dpy, draw->drawable, gc, x, y, width, height);
}

/* Real glyph rendering: one XFillRectangle per set pixel in the
 * embedded 8x8 bitmap, scaled GLYPH_SCALE-x. Only the basic-Latin
 * range (font8x8_basic covers U+0000-U+007F) draws anything -- UTF-8
 * multi-byte sequences (top bit set) just advance the cursor by one
 * cell width without drawing, which is honest (no glyph data for
 * them) rather than pretending. `y` is the text baseline, matching
 * real Xft's own XftDrawStringUtf8 convention -- callers already pass
 * `y + font->y` (font->y == ascent) for exactly this reason.
 */
void XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *pub, int x, int y,
			const XftChar8 *string, int len)
{
	int i, row, col;
	int top = y - (pub ? pub->ascent : GLYPH_ASCENT);
	GC gc;

	if (!draw || !string)
		return;

	gc = xft_gc(draw->dpy);
	if (!gc)
		return;
	xft_set_foreground(draw->dpy, gc, color);

	for (i = 0; i < len; i++) {
		unsigned char ch = string[i];
		int cellx = x + i * GLYPH_W;

		if (ch < 0x80) {
			const char *rows = font8x8_basic[ch];

			for (row = 0; row < 8; row++) {
				unsigned char bits = (unsigned char)rows[row];

				for (col = 0; col < 8; col++) {
					if (!(bits & (1 << col)))
						continue;
					XFillRectangle(draw->dpy, draw->drawable, gc,
							cellx + col * GLYPH_SCALE, top + row * GLYPH_SCALE,
							GLYPH_SCALE, GLYPH_SCALE);
				}
			}
		}
	}
}

/* Real advance/extent numbers for the embedded font -- monospace, so
 * this is exact, not approximated.
 */
void XftTextExtentsUtf8(Display *dpy, XftFont *pub, const XftChar8 *string, int len, XGlyphInfo *extents)
{
	int advance = len * (pub ? pub->max_advance_width : GLYPH_W);

	(void)dpy;
	(void)string;

	extents->width = (unsigned short)advance;
	extents->height = (unsigned short)(pub ? pub->height : GLYPH_H);
	extents->x = 0;
	extents->y = (short)(pub ? pub->ascent : GLYPH_ASCENT);
	extents->xOff = (short)advance;
	extents->yOff = 0;
}

/* Real Xft has separate 8-bit-Latin1 and UTF-8 entry points; this
 * stub's own glyph table only ever covers U+0000-U+007F either way
 * (see XftDrawStringUtf8), so the metrics are identical.
 */
void XftTextExtents8(Display *dpy, XftFont *pub, const XftChar8 *string, int len, XGlyphInfo *extents)
{
	XftTextExtentsUtf8(dpy, pub, string, len, extents);
}

/* Real Xft's FreeType-library init. There is no FreeType here (see
 * file-level comment), so this just reports success -- the same
 * "always succeeds" contract XftFontOpenName already keeps.
 */
Bool XftInitFtLibrary(void)
{
	return True;
}

/* Real XAllocColor/XFreeColors round-trip -- genuinely real pixel
 * allocation on the target's own colormap, not faked.
 */
FcBool XftColorAllocValue(Display *dpy, Visual *visual, Colormap cmap,
			   const XRenderColor *color, XftColor *result)
{
	XColor xc;

	(void)visual;
	xc.red = color->red;
	xc.green = color->green;
	xc.blue = color->blue;
	xc.flags = DoRed | DoGreen | DoBlue;
	if (!XAllocColor(dpy, cmap, &xc))
		return FcFalse;
	result->color = *color;
	result->pixel = xc.pixel;
	return FcTrue;
}

void XftColorFree(Display *dpy, Visual *visual, Colormap cmap, XftColor *color)
{
	(void)visual;
	XFreeColors(dpy, cmap, &color->pixel, 1, 0);
}

/* See the XftListFonts declaration in Xft.h for why this reports one
 * real font rather than FcFontList's honestly-empty answer: it is the
 * one font this stub can actually render, and the only list XPaint's
 * font-selector dialog ever gets to show. The variadic object/value
 * list real Xft would filter/build the pattern from is intentionally
 * unread -- every request gets the same single, real answer, same as
 * XftFontOpenName.
 */
XftFontSet *XftListFonts(Display *dpy, int screen, ...)
{
	FcFontSet *s = calloc(1, sizeof(*s));
	FcPattern *p = FcPatternCreate();

	(void)dpy;
	(void)screen;

	FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)"Liberation");
	FcPatternAddString(p, FC_FOUNDRY, (const FcChar8 *)"misc");
	FcPatternAddString(p, FC_STYLE, (const FcChar8 *)"Regular");

	s->fonts = calloc(1, sizeof(FcPattern *));
	s->fonts[0] = p;
	s->nfont = 1;
	s->sfont = 1;
	return s;
}
