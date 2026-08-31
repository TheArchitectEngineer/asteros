/* Honest stub of fontconfig's public API, not a real vendor of upstream
 * fontconfig -- see src/xft-stub/xft_stub.c for why. Pattern storage
 * (create, destroy, add, get, del, name-parse, name-unparse) is a real,
 * if simplified, in-memory key/value implementation. Font discovery
 * (FcFontList) always reports zero fonts, which is honest: this project
 * has no real font files vendored anywhere (same gap documented for
 * xterm in TODO.md Phase 35), so a real fontconfig would find nothing
 * to list here either.
 */
#ifndef _FONTCONFIG_H_
#define _FONTCONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char FcChar8;
typedef int FcBool;
#define FcTrue 1
#define FcFalse 0

typedef enum {
	FcResultMatch,
	FcResultNoMatch,
	FcResultTypeMismatch,
	FcResultNoId,
	FcResultOutOfMemory
} FcResult;

typedef struct _FcPattern FcPattern;
typedef struct _FcObjectSet FcObjectSet;

typedef struct _FcFontSet {
	int nfont;
	int sfont;
	FcPattern **fonts;
} FcFontSet;

typedef struct {
	int type;
	union {
		const FcChar8 *s;
		double d;
		int i;
		FcBool b;
	} u;
} FcValue;

#define FC_FAMILY "family"
#define FC_FOUNDRY "foundry"
#define FC_STYLE "style"
#define FC_SIZE "size"
#define FC_PIXEL_SIZE "pixelsize"
#define FC_WEIGHT "weight"
#define FC_SLANT "slant"
#define FC_WIDTH "width"

/* Real fontconfig weight/slant/width scale, ground-truthed against
 * upstream fontconfig/fontconfig.h -- not made up, since callers (e.g.
 * WPrefs.app/FontSimple.c) compare/sort by these numerically, not just
 * by name.
 */
#define FC_WEIGHT_THIN       0
#define FC_WEIGHT_EXTRALIGHT 40
#define FC_WEIGHT_ULTRALIGHT FC_WEIGHT_EXTRALIGHT
#define FC_WEIGHT_LIGHT      50
#define FC_WEIGHT_DEMILIGHT  55
#define FC_WEIGHT_SEMILIGHT  FC_WEIGHT_DEMILIGHT
#define FC_WEIGHT_BOOK       75
#define FC_WEIGHT_REGULAR    80
#define FC_WEIGHT_NORMAL     FC_WEIGHT_REGULAR
#define FC_WEIGHT_MEDIUM     100
#define FC_WEIGHT_DEMIBOLD   180
#define FC_WEIGHT_SEMIBOLD   FC_WEIGHT_DEMIBOLD
#define FC_WEIGHT_BOLD       200
#define FC_WEIGHT_EXTRABOLD  205
#define FC_WEIGHT_ULTRABOLD  FC_WEIGHT_EXTRABOLD
#define FC_WEIGHT_BLACK      210
#define FC_WEIGHT_HEAVY      FC_WEIGHT_BLACK

#define FC_SLANT_ROMAN   0
#define FC_SLANT_ITALIC  100
#define FC_SLANT_OBLIQUE 110

#define FC_WIDTH_ULTRACONDENSED 50
#define FC_WIDTH_EXTRACONDENSED 63
#define FC_WIDTH_CONDENSED      75
#define FC_WIDTH_SEMICONDENSED  87
#define FC_WIDTH_NORMAL         100
#define FC_WIDTH_SEMIEXPANDED   113
#define FC_WIDTH_EXPANDED       125
#define FC_WIDTH_EXTRAEXPANDED  150
#define FC_WIDTH_ULTRAEXPANDED  200

FcPattern *FcPatternCreate(void);
void FcPatternDestroy(FcPattern *p);
FcPattern *FcPatternDuplicate(const FcPattern *p);

FcBool FcPatternAddString(FcPattern *p, const char *object, const FcChar8 *s);
FcBool FcPatternAddDouble(FcPattern *p, const char *object, double d);
FcBool FcPatternAddInteger(FcPattern *p, const char *object, int i);
FcBool FcPatternDel(FcPattern *p, const char *object);

FcResult FcPatternGet(const FcPattern *p, const char *object, int id, FcValue *v);
FcResult FcPatternGetString(const FcPattern *p, const char *object, int id, FcChar8 **s);
FcResult FcPatternGetDouble(const FcPattern *p, const char *object, int id, double *d);
FcResult FcPatternGetInteger(const FcPattern *p, const char *object, int id, int *i);

void FcPatternPrint(const FcPattern *p);

FcPattern *FcNameParse(const FcChar8 *name);
FcChar8 *FcNameUnparse(FcPattern *pat);

FcObjectSet *FcObjectSetBuild(const char *first, ...);
void FcObjectSetDestroy(FcObjectSet *os);

void FcDefaultSubstitute(FcPattern *pattern);

FcFontSet *FcFontList(void *config, FcPattern *p, FcObjectSet *os);
void FcFontSetDestroy(FcFontSet *s);

#ifdef __cplusplus
}
#endif

#endif
