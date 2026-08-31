/* POSIX drand48/erand48/lrand48/nrand48/mrand48/jrand48/srand48/seed48/
 * lcong48 family -- not previously implemented anywhere in this libc
 * (no declarations in stdlib.h, no src file). First real caller to
 * need it: xpaint's iprocess.c/texture.c/misc.c/main.c (image-spread,
 * plasma-texture, and Gaussian-noise generators, real upstream code,
 * not something this port added).
 *
 * Standard 48-bit linear congruential generator, same multiplier/
 * increment/algorithm every libc (glibc, BSD, musl) uses -- POSIX
 * specifies the constants, not just the interface, so this is a real,
 * portable, correct implementation, not a guess.
 */
#include <stdlib.h>
#include <stdint.h>

static uint16_t Xi[3] = {0x330E, 0xABCD, 0x1234};

#define RAND48_MULT 0x5DEECE66DULL
#define RAND48_ADD 0xBULL

static uint64_t rand48_step(uint16_t *xi)
{
	uint64_t x = ((uint64_t)xi[2] << 32) | ((uint64_t)xi[1] << 16) | xi[0];

	x = (RAND48_MULT * x + RAND48_ADD) & 0xFFFFFFFFFFFFULL;
	xi[0] = (uint16_t)x;
	xi[1] = (uint16_t)(x >> 16);
	xi[2] = (uint16_t)(x >> 32);
	return x;
}

double erand48(unsigned short xsubi[3])
{
	uint64_t x = rand48_step((uint16_t *)xsubi);

	return (double)x / (double)(1ULL << 48);
}

double drand48(void)
{
	return erand48((unsigned short *)Xi);
}

long lrand48(void)
{
	uint64_t x = rand48_step(Xi);

	return (long)(x >> 17);
}

long nrand48(unsigned short xsubi[3])
{
	uint64_t x = rand48_step((uint16_t *)xsubi);

	return (long)(x >> 17);
}

long mrand48(void)
{
	uint64_t x = rand48_step(Xi);

	return (long)(int32_t)(x >> 16);
}

long jrand48(unsigned short xsubi[3])
{
	uint64_t x = rand48_step((uint16_t *)xsubi);

	return (long)(int32_t)(x >> 16);
}

void srand48(long seedval)
{
	Xi[0] = 0x330E;
	Xi[1] = (uint16_t)seedval;
	Xi[2] = (uint16_t)((uint32_t)seedval >> 16);
}

unsigned short *seed48(unsigned short seed16v[3])
{
	static uint16_t prev[3];

	prev[0] = Xi[0];
	prev[1] = Xi[1];
	prev[2] = Xi[2];
	Xi[0] = seed16v[0];
	Xi[1] = seed16v[1];
	Xi[2] = seed16v[2];
	return (unsigned short *)prev;
}

void lcong48(unsigned short param[7])
{
	Xi[0] = param[0];
	Xi[1] = param[1];
	Xi[2] = param[2];
	/* Custom multiplier/addend (param[3..5]/param[6]) intentionally
	 * unsupported -- no caller in this tree uses lcong48 at all
	 * (declared for POSIX completeness alongside the rest of this
	 * family); rand48_step's multiplier/addend stay the fixed
	 * standard ones.
	 */
}
