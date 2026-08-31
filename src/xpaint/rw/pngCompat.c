/* PNG stand-in for this port: upstream xpaint always builds its PNG
 * reader/writer (rw/readWritePNG.c) unconditionally against libpng,
 * even without HAVE_PNG defined -- WritePNGn is the hardcoded default
 * writer in rwTable.c's writeMagic()/RWtableGetWriterFromSuffix() (used
 * whenever a save has no recognized extension), and print.c's
 * screenshot save, rw/readWriteLXP.c's layer save, and
 * share/c_scripts/batch/batch.c call it directly. libpng/zlib's PNG
 * layer isn't vendored in this project (only libjpeg was, for
 * WindowMaker's JPEG wallpaper support -- see src/libjpeg), so rather
 * than vendor a second image codec just for this fallback path, these
 * three entry points forward to the already-real, already-linked XPM
 * reader/writer (rw/readWriteXPM.c) instead of faking PNG bytes under
 * a .png name. TestPNG stays honestly "never matches" (this can't
 * actually decode PNG-format bytes), so readMagic()'s format-sniffing
 * loop in rwTable.c never routes a real PNG file through here -- it
 * only affects the "no other format applies" fallback and the direct
 * screenshot/layer-save call sites, which get a real, valid XPM file
 * instead of a truncated/fake PNG one.
 */
#include "../image.h"

extern int WriteXPM(char *file, Image *image);
extern Image *ReadXPM(char *file);

Image *
ReadPNG(char *file)
{
	return ReadXPM(file);
}

int
WritePNGn(char *file, Image *image)
{
	return WriteXPM(file, image);
}

int
WritePNGi(char *file, Image *image)
{
	return WriteXPM(file, image);
}

int
TestPNG(char *file)
{
	return 0;
}
