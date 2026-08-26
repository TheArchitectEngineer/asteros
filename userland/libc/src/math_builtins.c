/* fabs/sqrt/floor/ceil/trunc/round/fmod: real functions (not macros --
 * see math.h's note), each a direct, exact wrapper around the clang/LLVM
 * builtin that lowers to a single hardware instruction (or a short,
 * exact sequence, for fmod) on x86_64. Not approximated:
 * __builtin_fabs/__builtin_sqrt/__builtin_floor/__builtin_ceil/
 * __builtin_trunc/__builtin_round/__builtin_fmod are exact per IEEE 754.
 * ceil/trunc/round/fmod added for pixman (X11 milestone) -- real, vendored
 * musl double-precision sources for these exist under src/musl-math-src/
 * too (same provenance as the sin/cos/pow family this project already
 * uses), but the builtin lowering is simpler and equally exact here. */
#include <math.h>

double fabs(double x) { return __builtin_fabs(x); }
double sqrt(double x) { return __builtin_sqrt(x); }
double floor(double x) { return __builtin_floor(x); }
double ceil(double x) { return __builtin_ceil(x); }
double trunc(double x) { return __builtin_trunc(x); }
double round(double x) { return __builtin_round(x); }
double fmod(double x, double y) { return __builtin_fmod(x, y); }

/* __sincos_stret: Darwin-specific combined sin+cos ABI helper. LLVM's
 * X86 backend emits a call to this (not separate sin()/cos() calls)
 * whenever it sees both computed from the same argument on a Darwin
 * triple -- see hw/kdrive's miarc.c/mifillarc.c for the X11 milestone
 * callers that first pulled this in. Two explicit leading underscores,
 * not three -- ground-truthed from the actual linker error text
 * ("___sincos_stret" undefined): Darwin's C-symbol mangling adds one
 * more implicit underscore on top of the source, so the real
 * libsystem_m export is __sincos_stret. Struct name/field names and
 * layout (sin first, cos second) ground-truthed against the real
 * Apple SDK's own math.h (struct __double2 { double __sinval,
 * __cosval; }; extern struct __double2 __sincos_stret(double);) --
 * the two-double return classifies as SSE,SSE under the x86_64 SysV
 * ABI, so a plain C struct return already lands in xmm0/xmm1, no
 * hand-written asm needed unlike ____chkstk_darwin. */
struct __double2 { double __sinval, __cosval; };
struct __double2
__sincos_stret(double x)
{
	struct __double2 r;
	r.__sinval = sin(x);
	r.__cosval = cos(x);
	return r;
}
