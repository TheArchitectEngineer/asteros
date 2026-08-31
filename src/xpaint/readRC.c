/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)                  | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.  There is no           | */
/* | representations about the suitability of this software for        | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.                                              | */
/* |                                                                   | */
/* +-------------------------------------------------------------------+ */

/* $Id: readRC.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>
#include <pwd.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/Xos.h>
#include <sys/stat.h>
#include "xpaint.h"
#include "image.h"
#include "rc.h"
#include "misc.h"


#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

/*
#ifdef __STDC__
extern char *mktemp(char *);
#else
extern char *mktemp();
#endif */ /* __STDC__ */


#define RC_FILENAME	".XPaintrc"

static String defaultRC[] =
{
#include "DefaultRC.txt.h"
};

/*
**   RC File Syntax
**
**     solid    [ name | x-rgb-def | r g b ]
**
**     solid!   [ name | x-rgb-def | r g b ]
**     
**     pattern  [ BeginData\n ... \nEndData | filename ]
**
**     pattern! [ BeginData\n ... \nEndData | filename ]
**
**     brush    [ BeginData\n ... \nEndData | filename ]
**
**     brush!    [ BeginData\n ... \nEndData | filename ]
**
**     \n# -- comment
**
** Note:  solid!  or  pattern!  or  brush!  mean : 
**        take color/pattern/brush only if screen depth is > 8 bits
 */

#define EQ(a,b)		(strcasecmp(a,b) == 0)

static int tempIndex = -1;
static char *tempName[20];
static RCInfo *baseInfo = NULL;

void
openTempDir(char *buf)
{
    char *tmp, *home;

    if ((tmp = getenv("TMPDIR")) != NULL) {
        sprintf(buf, "mkdir -p \"%s\"", tmp);
        system(buf);
        strncpy(buf, tmp, 200);
    } else
    if ((home = getenv("HOME")) != NULL) {
        sprintf(buf, "%s/.xpaint", home);
        mkdir(buf, 0777);
        sprintf(buf, "%s/.xpaint/tmp", home);
        mkdir(buf, 0777);
    } else
        strncpy(buf, P_tmpdir, 200);
}

FILE *
openTempFile(char **np)
{
    int fd;
    char buf[256];

    openTempDir(buf);
    buf[200] = '\0';
    strcat(buf, "/XPaint-XXXXXX");
    fd = mkstemp(buf);
    if (fd == -1) return NULL;

    tempName[++tempIndex] = XtNewString(buf);
    if (np != NULL)
        *np = tempName[tempIndex];
    return fdopen(fd, "w");
}

void 
removeTempFile(void)
{
    if (tempIndex < 0)
	return;
    if (tempName[tempIndex] != NULL) {
	unlink(tempName[tempIndex]);
	XtFree((XtPointer) tempName[tempIndex]);
    }
    tempName[tempIndex--] = NULL;
}

static void 
addImage(RCInfo * info, Image * image, Boolean isBrush)
{
    if (isBrush) {
	info->brushes = (Image **)
	    XtRealloc((XtPointer) info->brushes,
		      (info->nbrushes + 1) * sizeof(Image *));
	info->brushes[info->nbrushes++] = image;
    } else {
	info->images = (Image **)
	    XtRealloc((XtPointer) info->images,
		      (info->nimages + 1) * sizeof(Image *));
	info->images[info->nimages++] = image;
    }
}

static void 
addSolid(RCInfo * info, char *color)
{
    info->colors = (char **) XtRealloc((XtPointer) info->colors,
				       (info->ncolors + 1) * sizeof(char *));
    info->colors[info->ncolors++] = XtNewString(color);
}

static RCInfo *
makeInfo(void)
{
    RCInfo *info;

    info = XtNew(RCInfo);
    info->freed = False;
    info->nimages = 0;
    info->nbrushes = 0;
    info->colorFlags = NULL;
    info->colorPixels = NULL;
    info->images = XtNew(Image *);
    info->brushes = XtNew(Image *);
    info->ncolors = 0;
    info->colors = XtNew(char *);

    baseInfo = info;

    return info;
}

void 
FreeRC(RCInfo * info)
{
    int i;

    if (info->colors != NULL) {
	for (i = 0; i < info->ncolors; i++)
	    XtFree((XtPointer) info->colors[i]);
	XtFree((XtPointer) info->colors);
    }
    if (info->colorFlags != NULL)
	XtFree((XtPointer) info->colorFlags);
    if (info->colorPixels != NULL)
	XtFree((XtPointer) info->colorPixels);

    for (i = 0; i < info->nimages; i++)
	ImageDelete(info->images[i]);
    if (info->images != NULL)
	XtFree((XtPointer) info->images);

    for (i = 0; i < info->nbrushes; i++)
	ImageDelete(info->brushes[i]);
    if (info->brushes != NULL)
	XtFree((XtPointer) info->brushes);

    XtFree((XtPointer) info);

    if (info == baseInfo)
	baseInfo = NULL;
}

/*
 * Expand leading tilde in path name.
 */
static char *
expand(char *path)
{
    static char out[512];
    char name[80];
    char *pp = path, *cp;
    struct passwd *pw;

    if (*path != '~')
	return path;
    path++;
    cp = name;
    while (*pp != '/' && *pp != '\0')
	*cp++ = *pp++;
    *cp = '\0';
    if (name[0] == '\0') {
	pw = getpwuid(getuid());
    } else {
	pw = getpwnam(name);
    }
    if (pw == NULL)
	return path;
    strcpy(out, pw->pw_dir);
    strcat(out, "/");
    strcat(out, pp);

    return out;
}

static Boolean
readRC(RCInfo ** info, char *file)
{
    FILE *fd = fopen(file, "r");
    char buf[512];
    int lineno = 0;
    int argc;
    char *argv[128 + 2];

    if (fd == NULL)
	return False;

    while (fgets(buf, sizeof(buf), fd) != NULL) {
	lineno++;
	if (buf[0] == '#' || buf[0] == '!')
	    continue;
	StrToArgv(buf, &argc, argv);
	if (argc == 0)
	    continue;
	if (EQ(argv[0], "reset")) {
	    FreeRC(*info);
	    *info = makeInfo();
	} else if (EQ(argv[0], "defaultsize")) {
	} else if (EQ(argv[0], "solid") || 
	           (EQ(argv[0], "solid!") && Global.vis.depth>8)) {
	    addSolid(*info, argv[1]);
	} else if (EQ(argv[0], "pattern") || EQ(argv[0], "brush") ||
	           ((EQ(argv[0], "pattern!") || EQ(argv[0], "brush!")) && 
                   Global.vis.depth>8)) {
	    char *nm;
	    Image *image;
	    int isBrush = EQ(argv[0], "brush") || EQ(argv[0], "brush!");

	    if (EQ(argv[1], "BeginData")) {
		FILE *ofd;

		ofd = openTempFile(&nm);
		while (fgets(buf, sizeof(buf), fd) != NULL) {
		    if (strncmp(buf, "EndData", 7) == 0)
			break;
		    if (ofd != NULL)
			fputs(buf, ofd);
		}
		if (ofd != NULL) {
		    fclose(ofd);
		} else {
		    removeTempFile();
		    continue;
		}
	    } else {
		nm = expand(argv[1]);
	    }

	    if ((image = ReadMagic(nm)) != NULL)
		addImage(*info, image, isBrush);

	    removeTempFile();
	}
    }

    return True;
}

/*
**  Simple RC reading strategy:
**    load default
**    append users ~/.XPaintrc
**    append users ./.XPaintrc
**
 */
RCInfo *
ReadDefaultRC()
{
    static Boolean inited = False;
    static Boolean have[2] =
    {False, False};
    static time_t lastMtime;
    static RCInfo *info = NULL;
    static char homeRC[256];
    FILE *fd;
    int i;
    char *tn;
    struct passwd *pw = getpwuid(getuid());
    struct stat statbufA, statbufB;
    char *rcf;
    Boolean defaultDone = False;

    if (!inited) {
	inited = True;
	if (pw != NULL && pw->pw_dir != NULL) {
	    strcpy(homeRC, pw->pw_dir);
	    strcat(homeRC, "/");
	    strcat(homeRC, RC_FILENAME);
	} else {
	    homeRC[0] = '\0';
	}
    }
    if ((rcf = GetDefaultRC()) != NULL) {
	if (stat(rcf, &statbufA) < 0)	/* missing file? */
	    goto readit;

	if (info == NULL || statbufA.st_mtime > lastMtime) {
	    info = makeInfo();
	    readRC(&info, rcf);
	    lastMtime = statbufA.st_mtime;
	    defaultDone = True;
	}
    } else {
	if (info != NULL) {
	    Boolean hA, hB;

	    hA = (stat(homeRC, &statbufA) >= 0);
	    hB = (stat(RC_FILENAME, &statbufB) >= 0);

	    if (hA != have[0] || hB != have[1])
		goto readit;

	    if (hA && statbufA.st_mtime > lastMtime)
		goto readit;
	    if (hB && statbufB.st_mtime > lastMtime)
		goto readit;

	    /*
	    **  No change
	     */
	    return info;
	}
      readit:
	if (info != NULL)
	    FreeRC(info);
	info = makeInfo();

	/*
	**  Set time information
	 */
	have[0] = (stat(homeRC, &statbufA) >= 0);
	have[1] = (stat(RC_FILENAME, &statbufB) >= 0);

	if (have[0] && have[1]) {
	    if (statbufA.st_mtime > statbufB.st_mtime)
		lastMtime = statbufA.st_mtime;
	    else
		lastMtime = statbufB.st_mtime;
	} else if (have[0]) {
	    lastMtime = statbufA.st_mtime;
	} else if (have[1]) {
	    lastMtime = statbufB.st_mtime;
	}
	/*
	**  Load the default RC
	 */
	if (!defaultDone && ((fd = openTempFile(&tn)) != NULL)) {
	    for (i = 0; i < XtNumber(defaultRC); i++) {
		fputs(defaultRC[i], fd);
		putc('\n', fd);
	    }
	    fclose(fd);

	    readRC(&info, tn);
	    removeTempFile();
	}
	/*
	**  Load ~/.XPaintrc
	 */
	if (homeRC[0] != '\0')
	    readRC(&info, homeRC);

	/*
	**  Load ".XPaintrc"
	 */
	readRC(&info, RC_FILENAME);
    }

    if (info->ncolors == 0 && info->nimages == 0) {
	addSolid(info, "black");
	addSolid(info, "white");
	addSolid(info, "red");
	addSolid(info, "green");
	addSolid(info, "blue");
	addSolid(info, "cyan");
	addSolid(info, "magenta");
	addSolid(info, "yellow");
    }
    return info;
}

RCInfo *
ReadRC(char *file)
{
    RCInfo *info = makeInfo();

    if (!readRC(&info, file)) {
	/*
	**  Error occured
	 */
	FreeRC(info);
	return NULL;
    }
    return info;
}

