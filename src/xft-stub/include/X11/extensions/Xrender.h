/* Honest stub of libXrender's public header -- real Xrender.h defines
 * a large Picture/PictFormat rendering API this project doesn't
 * vendor (no real Xrender extension support). The only piece any app
 * ported so far needs from it is the XRenderColor struct (real Xft's
 * own Xft.h includes this header for exactly that reason, so XPaint's
 * fontOp.c -- the one caller so far -- includes it directly too,
 * matching real usage). Nothing else from real Xrender.h is declared;
 * add it here, for real, if a future port needs more of it.
 */
#ifndef _XRENDER_H_
#define _XRENDER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned short red, green, blue, alpha;
} XRenderColor;

#ifdef __cplusplus
}
#endif

#endif
