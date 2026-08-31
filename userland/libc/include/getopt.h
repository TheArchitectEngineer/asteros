/* Real Darwin's <getopt.h>: just the GNU-style long-option extension
 * (struct option, getopt_long/getopt_long_only) layered on top of
 * <unistd.h>'s plain getopt() -- optarg/optind/opterr/optopt are the
 * same globals, declared again here since real callers often include
 * only this header. Needed by WindowMaker's util/wdread.c and
 * util/wdwrite.c.
 */
#ifndef _GETOPT_H_
#define _GETOPT_H_

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define no_argument       0
#define required_argument 1
#define optional_argument 2

struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

int getopt_long(int argc, char *const argv[], const char *optstring,
		 const struct option *longopts, int *longindex);
int getopt_long_only(int argc, char *const argv[], const char *optstring,
		      const struct option *longopts, int *longindex);

#ifdef __cplusplus
}
#endif

#endif /* _GETOPT_H_ */
