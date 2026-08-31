/* Standard POSIX getopt() -- classic single-pass permuting-free
 * implementation (options must precede operands, no GNU permutation).
 * Good enough for busybox applets, which mostly use their own
 * bb_getopt_ulflags anyway; this covers the stragglers that call the
 * plain libc getopt(). getopt_long()/getopt_long_only() (added for
 * WindowMaker's util/wdread.c, util/wdwrite.c) are a straightforward
 * layer on top: exact-name matching only, no GNU unambiguous-prefix
 * matching -- every real caller in this codebase spells out full
 * option names, so there's nothing to disambiguate. */
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>

char *optarg;
int optind = 1;
int opterr = 1;
int optopt;

static int sp = 1;

int
getopt(int argc, char *const argv[], const char *optstring)
{
	/* glibc convention busybox relies on unconditionally (see its
	 * GETOPT_RESET() macro, include/libbb.h): optind==0 on entry means
	 * "full reset, start scanning from argv[1] again" -- without this,
	 * optind==0 makes us scan argv[0] (the program/applet name itself)
	 * as if it were the first operand, find no leading '-', and return
	 * -1 immediately without ever advancing optind past argv[0]. Every
	 * caller that then does `argv += optind` (e.g. ls_main) is left
	 * with argv[0] still pointing at the applet name, misinterpreting
	 * it as a positional argument (e.g. `ls` tries to stat a file
	 * named "ls"). */
	if (optind == 0) {
		optind = 1;
		sp = 1;
	}
	if (sp == 1) {
		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == 0) {
			return -1;
		}
		if (strcmp(argv[optind], "--") == 0) {
			optind++;
			return -1;
		}
	}
	optopt = argv[optind][sp];
	const char *cp = strchr(optstring, optopt);
	if (optopt == ':' || !cp) {
		if (opterr) {
			fprintf(stderr, "%s: illegal option -- %c\n", argv[0], optopt);
		}
		if (argv[optind][++sp] == 0) {
			optind++;
			sp = 1;
		}
		return '?';
	}
	if (cp[1] == ':') {
		if (argv[optind][sp + 1] != 0) {
			optarg = &argv[optind][sp + 1];
			optind++;
		} else if (++optind < argc) {
			optarg = argv[optind];
			optind++;
		} else {
			if (opterr) {
				fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optopt);
			}
			sp = 1;
			return '?';
		}
		sp = 1;
	} else {
		if (argv[optind][++sp] == 0) {
			sp = 1;
			optind++;
		}
		optarg = (void *)0;
	}
	return optopt;
}

static int
getopt_long_common(int argc, char *const argv[], const char *optstring,
		    const struct option *longopts, int *longindex, int long_only)
{
	const char *arg;
	int is_long;

	if (optind == 0) {
		optind = 1;
	}

	if (optind >= argc)
		return -1;

	arg = argv[optind];
	if (arg[0] != '-' || arg[1] == 0)
		return getopt(argc, argv, optstring);
	if (strcmp(arg, "--") == 0) {
		optind++;
		return -1;
	}

	is_long = (arg[0] == '-' && arg[1] == '-') || (long_only && arg[1] != '-');
	if (!is_long)
		return getopt(argc, argv, optstring);

	{
		const char *name = (arg[0] == '-' && arg[1] == '-') ? arg + 2 : arg + 1;
		const char *eq = strchr(name, '=');
		size_t namelen = eq ? (size_t)(eq - name) : strlen(name);
		const struct option *opt;

		for (opt = longopts; opt->name; opt++) {
			if (strlen(opt->name) != namelen || strncmp(opt->name, name, namelen) != 0)
				continue;

			optind++;

			if (opt->has_arg == required_argument || opt->has_arg == optional_argument) {
				if (eq) {
					optarg = (char *)(eq + 1);
				} else if (opt->has_arg == required_argument) {
					if (optind < argc) {
						optarg = argv[optind];
						optind++;
					} else {
						if (opterr)
							fprintf(stderr, "%s: option '--%s' requires an argument\n",
								argv[0], opt->name);
						return '?';
					}
				} else {
					optarg = (void *)0;
				}
			} else {
				optarg = (void *)0;
			}

			if (longindex)
				*longindex = (int)(opt - longopts);

			if (opt->flag) {
				*opt->flag = opt->val;
				return 0;
			}
			return opt->val;
		}

		if (opterr)
			fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], arg);
		optind++;
		return '?';
	}
}

int
getopt_long(int argc, char *const argv[], const char *optstring,
	    const struct option *longopts, int *longindex)
{
	return getopt_long_common(argc, argv, optstring, longopts, longindex, 0);
}

int
getopt_long_only(int argc, char *const argv[], const char *optstring,
		  const struct option *longopts, int *longindex)
{
	return getopt_long_common(argc, argv, optstring, longopts, longindex, 1);
}
