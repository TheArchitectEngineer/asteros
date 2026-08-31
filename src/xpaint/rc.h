/* $Id: rc.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

typedef struct {
    int freed;
    int ncolors;
    char **colors;
    Boolean *colorFlags;
    Pixel *colorPixels;
    int nimages;
    Image **images;
    int nbrushes;
    Image **brushes;
} RCInfo;

RCInfo *ReadDefaultRC(void);
RCInfo *ReadRC(char *);
void FreeRC(RCInfo *);
