/* Minimal sscanf: %d %i %u %x %o %c %s %f %g %e %% plus literal
 * text/whitespace matching and numeric field widths. Good enough for the
 * handful of straightforward numeric/string parses busybox's libbb code
 * does (procps.c /proc field parsing, etc.) plus xkbcomp's own number
 * scanner (X11 milestone, xkbscan.c's yyGetNumber -- real floats, not
 * skip-only, ground-truthed live: geometry files with fractional
 * measurements like "1.5" were failing to parse) -- not a complete C
 * stdio scanf. */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

static int
do_vsscanf(const char *s, const char *fmt, va_list ap)
{
	int assigned = 0;
	const char *p = fmt;
	while (*p) {
		if (isspace((unsigned char)*p)) {
			while (isspace((unsigned char)*s)) {
				s++;
			}
			p++;
			continue;
		}
		if (*p != '%') {
			if (*s != *p) {
				return assigned;
			}
			s++;
			p++;
			continue;
		}
		p++;
		int width = 0;
		while (*p >= '0' && *p <= '9') {
			width = width * 10 + (*p - '0');
			p++;
		}
		int suppress = 0;
		if (*p == '*') {
			suppress = 1;
			p++;
		}
		int islong = 0;
		while (*p == 'l' || *p == 'h') {
			if (*p == 'l') {
				islong = 1;
			}
			p++;
		}
		switch (*p) {
		case 'd':
		case 'i':
		case 'u': {
			/* %i auto-detects its base from the input (a "0x"/"0X"
			 * prefix means hex, a bare leading "0" means octal, same
			 * as strtol(..., 0)) -- %d/%u are always base 10. Getting
			 * this wrong silently mis-parses any "0x..."-formatted
			 * value as just its leading "0" digit in base 10 (i.e.
			 * 0), which is exactly the bug that left WindowMaker's
			 * WindowTitleMaxHeight preference at 0 instead of
			 * INT_MAX whenever the property-list value happened to
			 * be serialized in hex ("0x7fffffff") -- ground-truthed
			 * live via a zero-height titlebar. */
			int conv_base = (*p == 'i') ? 0 : 10;
			while (isspace((unsigned char)*s)) {
				s++;
			}
			const char *src = s;
			char tmp[32];
			if (width) {
				int n = 0;
				while (src[n] && n < width && n < (int)sizeof(tmp) - 1) {
					n++;
				}
				for (int i = 0; i < n; i++) {
					tmp[i] = src[i];
				}
				tmp[n] = 0;
				src = tmp;
			}
			char *end;
			long v = strtol(src, &end, conv_base);
			if (end == src) {
				return assigned;
			}
			if (!suppress) {
				*va_arg(ap, int *) = (int)v;
				assigned++;
			}
			s += (end - src);
			break;
		}
		case 'x': {
			while (isspace((unsigned char)*s)) {
				s++;
			}
			char *end;
			long v = strtol(s, &end, 16);
			if (end == s) {
				return assigned;
			}
			if (!suppress) {
				*va_arg(ap, int *) = (int)v;
				assigned++;
			}
			s = end;
			break;
		}
		case 'f':
		case 'g':
		case 'e': {
			while (isspace((unsigned char)*s)) {
				s++;
			}
			char *end;
			double v = strtod(s, &end);
			if (end == s) {
				return assigned;
			}
			if (!suppress) {
				if (islong) {
					*va_arg(ap, double *) = v;
				} else {
					*va_arg(ap, float *) = (float)v;
				}
				assigned++;
			}
			s = end;
			break;
		}
		case 'c': {
			if (!*s) {
				return assigned;
			}
			if (!suppress) {
				*va_arg(ap, char *) = *s;
				assigned++;
			}
			s++;
			break;
		}
		case 's': {
			while (isspace((unsigned char)*s)) {
				s++;
			}
			const char *start = s;
			int n = 0;
			while (*s && !isspace((unsigned char)*s) && (!width || n < width)) {
				s++;
				n++;
			}
			if (s == start) {
				return assigned;
			}
			if (!suppress) {
				char *out = va_arg(ap, char *);
				for (int i = 0; i < n; i++) {
					out[i] = start[i];
				}
				out[n] = 0;
				assigned++;
			}
			break;
		}
		case '%':
			if (*s != '%') {
				return assigned;
			}
			s++;
			break;
		default:
			return assigned;
		}
		p++;
	}
	return assigned;
}

int
vsscanf(const char *str, const char *fmt, va_list ap)
{
	return do_vsscanf(str, fmt, ap);
}

int
sscanf(const char *str, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = do_vsscanf(str, fmt, ap);
	va_end(ap);
	return r;
}

int
fscanf(FILE *stream, const char *fmt, ...)
{
	char buf[512];
	if (!fgets(buf, sizeof(buf), stream)) {
		return -1;
	}
	va_list ap;
	va_start(ap, fmt);
	int r = do_vsscanf(buf, fmt, ap);
	va_end(ap);
	return r;
}
