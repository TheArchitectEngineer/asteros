/*
 * Read a Microsoft/IBM BMP file.
 *
 * Based on bmp.c for xli by Graeme Gill,
 *
 * Based on tga.c, and guided by the Microsoft file format
 * description, and bmptoppm.c by David W. Sanderson.
 *
 */
 
#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/xpm.h>

#include "xpaint.h"
#include "image.h"
#include "libpnmrw.h"

#define C_WIN   1		/* Image class */
#define C_OS2   2

#define BI_RGB  0		/* Compression type */
#define BI_RLE8 1
#define BI_RLE4 2

#define BMP_FILEHEADER_LEN 14

#define WIN_INFOHEADER_LEN 40
#define OS2_INFOHEADER_LEN 12

extern void *xmalloc(size_t n);

/* Header structure definition. */
typedef struct {
	int class;			/* Windows or OS/2 */

	unsigned long bfSize;		/* Size of file in bytes */
	unsigned int  bfxHotSpot;	/* Not used */
	unsigned int  bfyHotSpot;	/* Not used */
	unsigned long bfOffBits;	/* Offset of image bits from start of header */

	unsigned long biSize;		/* Size of info header in bytes */
	unsigned long biWidth;		/* Image width in pixels */
	unsigned long biHeight;		/* Image height in pixels */
	unsigned int  biPlanes;		/* Planes. Must == 1 */
	unsigned int  biBitCount;	/* Bits per pixels. Must be 1, 4, 8 or 24 */
	unsigned long biCompression;	/* Compression type */
	unsigned long biSizeImage;		/* Size of image in bytes */
	unsigned long biXPelsPerMeter;	/* X pixels per meter */
	unsigned long biYPelsPerMeter;	/* Y pixels per meter */
	unsigned long biClrUsed;		/* Number of colormap entries (0 == max) */
	unsigned long biClrImportant;	/* Number of important colors */
	} bmpHeader;



#define GULONG4(bp) ((unsigned long)(bp)[0] + 256 * (unsigned long)(bp)[1] \
	+ 65536 * (unsigned long)(bp)[2] + 16777216 * (unsigned long)(bp)[3])
#define GULONG2(bp) ((unsigned long)(bp)[0] + 256 * (unsigned long)(bp)[1])
#define GUINT2(bp) ((unsigned int)(bp)[0] + 256 * (unsigned int)(bp)[1])

/* A block fill routine for BSD style systems */
void 
bfill(char *s, int n, int c)
{

        int b;
                
        /* bytes to next word */
        b = (0 - (long) s) & (sizeof(unsigned long) - 1);
        if (n < b)
                b = n;
        while (n != 0) {
                n -= b;
                while (b-- > 0)
                        *s++ = c;
                if (n == 0)
                        return;
                /* words to fill */
                b = n & ~(sizeof(unsigned long) - 1);
                if (b != 0) {
                        unsigned long f;
                        int i;
                        f = c & (((unsigned long) -1) >> ((sizeof(unsigned long) - sizeof(char)) * 8));
                        for (i = sizeof(char); i < sizeof(unsigned long); i *= 2)
                                f |= (f << (8 * i));
                        n -= b; /* remaining count */
                        while (b > 0) {
                                *((unsigned long *) s) = f;
                                s += sizeof(unsigned long);
                                b -= sizeof(unsigned long);
                        }
                }
                b = n;
	}
}

/* Read the header of the file, and */
/* Return TRUE if this looks like a bmp file */
static Boolean read_bmpHeader(FILE * fd, bmpHeader * hp)
{
	unsigned char buf[WIN_INFOHEADER_LEN];	/* largest we'll need */

	if (fread(buf, 1, BMP_FILEHEADER_LEN, fd) != BMP_FILEHEADER_LEN)
		return FALSE;

	if (buf[0] != 'B' || buf[1] != 'M')
		return FALSE;	/* bad magic number */

	hp->bfSize = GULONG4(&buf[2]);
	hp->bfxHotSpot = GUINT2(&buf[6]);
	hp->bfyHotSpot = GUINT2(&buf[8]);
	hp->bfOffBits = GULONG4(&buf[10]);

	/* Read enough of the file info to figure the type out */
	if (fread(buf, 1, 4, fd) != 4)
		return FALSE;

	hp->biSize = GULONG4(&buf[0]);
	if (hp->biSize == WIN_INFOHEADER_LEN)
		hp->class = C_WIN;
	else if (hp->biSize == OS2_INFOHEADER_LEN)
		hp->class = C_OS2;
	else
		return FALSE;

	if (hp->class == C_WIN) {
		/* read in the rest of the info header */
		if (fread(buf + 4, 1, WIN_INFOHEADER_LEN - 4, fd)
				!= WIN_INFOHEADER_LEN - 4)
			return FALSE;

		hp->biWidth = GULONG4(&buf[4]);
		hp->biHeight = GULONG4(&buf[8]);
		hp->biPlanes = GUINT2(&buf[12]);;
		hp->biBitCount = GUINT2(&buf[14]);;
		hp->biCompression = GULONG4(&buf[16]);;
		hp->biSizeImage = GULONG4(&buf[20]);;
		hp->biXPelsPerMeter = GULONG4(&buf[24]);;
		hp->biYPelsPerMeter = GULONG4(&buf[28]);;
		hp->biClrUsed = GULONG4(&buf[32]);
		hp->biClrImportant = GULONG4(&buf[36]);
	} else {		/* C_OS2 */
		/* read in the rest of the info header */
		if (fread(buf + 4, 1, OS2_INFOHEADER_LEN - 4, fd)
				!= OS2_INFOHEADER_LEN - 4)
			return FALSE;

		hp->biWidth = GULONG2(&buf[4]);
		hp->biHeight = GULONG2(&buf[6]);
		hp->biPlanes = GUINT2(&buf[8]);;
		hp->biBitCount = GUINT2(&buf[10]);;
		hp->biCompression = BI_RGB;
		hp->biSizeImage = 0;
		hp->biXPelsPerMeter = 0;
		hp->biYPelsPerMeter = 0;
		hp->biClrUsed = 0;
		hp->biClrImportant = 0;
	}

	/* Check for file corruption */

	if (hp->biBitCount != 1
	    && hp->biBitCount != 4
	    && hp->biBitCount != 8
	    && hp->biBitCount != 24) {
	    fprintf(stderr, "bmpLoad: Illegal image BitCount %d\n", 
                    hp->biBitCount);
		return FALSE;
	}
	if ((hp->biCompression != BI_RGB
	     && hp->biCompression != BI_RLE8
	     && hp->biCompression != BI_RLE4)
	    || (hp->biCompression == BI_RLE8 && hp->biBitCount != 8)
	    || (hp->biCompression == BI_RLE4 && hp->biBitCount != 4)) {
		fprintf(stderr,
			"bmpLoad: Illegal image compression type %ld\n",
                        hp->biCompression);
		return FALSE;
	}
	if (hp->biPlanes != 1) {
	    fprintf(stderr, "bmpLoad: Illegal image Planes value %d\n", hp->biPlanes);
		return FALSE;
	}
	/* Fix up a few things */
	if (hp->biBitCount < 24) {
		if (hp->biClrUsed == 0
		    || hp->biClrUsed > (1 << hp->biBitCount))
			hp->biClrUsed = (1 << hp->biBitCount);
	} else
		hp->biClrUsed = 0;

	return TRUE;
}

int
TestBMP(char *fullname)
{
	FILE *fd;
	bmpHeader hdr;

	if (!(fd = fopen(fullname, "r"))) {
		return FALSE;
	}
	if (!read_bmpHeader(fd, &hdr)) {
		fclose(fd);
		return FALSE;	/* Nope, not a BMP file */
	}
	fclose(fd);
	return TRUE;
}

Image 
*ReadBMP(char *fullname)
{
	FILE *fd;
	bmpHeader hdr;
	Image *image;
	Boolean data_bounds = FALSE;
	int skip;

	if (!(fd = fopen(fullname, "r"))) {
		perror("bmpIdent");
		return (0);
	}
	if (!read_bmpHeader(fd, &hdr)) {
		fclose(fd);
		return NULL;	/* Nope, not a BMP file */
	}
#if 0
        /* Print a brief description of the image */
        printf( "%s is a %lux%lu %d bit deep, %s%s BMP image\n", fullname,
		hdr.biWidth, hdr.biHeight, hdr.biBitCount,
		hdr.biCompression == BI_RGB ? "" : "Run length compressed, ",
		hdr.class == C_WIN ? "Windows" : "OS/2");       
#endif

	/* Create the appropriate image and colormap */
	if (hdr.biBitCount < 24) {
		/* must be 1, 4 or 8 bit mapped type */
		int i, j, n = 3;
		unsigned char buf[4];
		/* maximum number of colors */
		int used = (1 << hdr.biBitCount);

		if (hdr.biBitCount == 1)	/* bitmap */
			image = ImageNewBW(hdr.biWidth, hdr.biHeight);
		else
     		if (hdr.biBitCount <= 8)	/* colormap image */
			image = ImageNewCmap(hdr.biWidth, hdr.biHeight, used);
	        else
			image = ImageNew(hdr.biWidth, hdr.biHeight);

		if (hdr.class == C_WIN)
			n++;
	        j = 0;
		for (i = 0; i < hdr.biClrUsed; i++) {
			if (fread(buf, 1, n, fd) != n) {
				fprintf(stderr, "bmpLoad: Short read within Colormap\n");
				ImageDelete(image);
				fclose(fd);
				return NULL;
			}
                        image->cmapData[j++] = buf[2];
                        image->cmapData[j++] = buf[1];
                        image->cmapData[j++] = buf[0];
		}

		/* init rest of colormap (if any) */
		for (; i < used; i++) {
			image->cmapData[j++] = 0;
  			image->cmapData[j++] = 0;
			image->cmapData[j++] = 0;
		}
	} else {		/* else must be a true color image */
                image = ImageNew(hdr.biWidth, hdr.biHeight);
	}

	/* Skip to offset specified in file header for image data */
	if (hdr.class == C_WIN)
		skip = hdr.bfOffBits - (BMP_FILEHEADER_LEN
			+ WIN_INFOHEADER_LEN + 4 * hdr.biClrUsed);
	else
		skip = hdr.bfOffBits - (BMP_FILEHEADER_LEN
			+ OS2_INFOHEADER_LEN + 3 * hdr.biClrUsed);

	while (skip > 0) {
		if (fgetc(fd) == EOF)
			goto data_short;
		skip--;
	}

	/* Read the pixel data */
	if (hdr.biBitCount == 1) {
		unsigned char *data, *buf, pad[4];
		int i, illen, padlen, y;

		/* round bits up to byte */
		illen = (image->width + 7) / 8;
		/* extra bytes to word boundary */	
		padlen = (((image->width + 31) / 32) * 4) - illen;
		/* start at bottom */
		data = image->data + (image->height - 1) * image->width;
		buf = (unsigned char *)xmalloc(illen+1);
		if (!buf) goto data_short;
		for (y = image->height; y > 0; y--) {
			/* BMP files are left bit == ms bit,
			 * so read straight in.
			 */
			if (fread(buf, 1, illen, fd) != illen
			    || fread(pad, 1, padlen, fd) != padlen)
				goto data_short;
			/* convert 8-pixel per byte to 1-pixel per byte */
			for (i=0; i<image->width; i++)
			    data[i] = (buf[i>>3]>>(7-(i&7)))&1;
                        data -= image->width;
		}
		free(buf);
	} else if (hdr.biBitCount == 4) {
		unsigned char *data;
		int illen, x, y;

		illen = image->width;
		/* start at bottom */
		data = image->data + (image->height - 1) * illen;

		if (hdr.biCompression == BI_RLE4) {
			int d, e;
			bzero((char *) image->data,
				image->width * image->height);
			for (x = y = 0;;) {
				int i, f;
				if ((d = fgetc(fd)) == EOF)
					goto data_short;
				if (d != 0) {	/* run of pixels */
					x += d;
					if (x > image->width ||
							y > image->height) {
						/* don't run off buffer */
						data_bounds = TRUE;
						/* ignore this run */
						x -= d;	
						if ((e = fgetc(fd)) == EOF)
							goto data_short;
						continue;
					}
					if ((e = fgetc(fd)) == EOF)
						goto data_short;
					f = e & 0xf;
					e >>= 4;
					for (i = d / 2; i > 0; i--) {
						*(data++) = e;
						*(data++) = f;
					}
					if (d & 1)
						*(data++) = e;
					continue;
				}
				/* else code */
				if ((d = fgetc(fd)) == EOF)
					goto data_short;
				if (d == 0) {	/* end of line */
					data -= (x + illen);
					x = 0;
					y++;
					continue;
				}
				/* else */
				if (d == 1)	/* end of bitmap */
					break;
				/* else */
				if (d == 2) {	/* delta */
					if ((d = fgetc(fd)) == EOF ||
							(e = fgetc(fd)) == EOF)
						goto data_short;
					x += d;
					data += d;
					y += e;
					data -= (e * illen);
					continue;
				}
				/* else run of literals */
				x += d;
				if (x > image->width || y > image->height) {
					int btr;
					/* don't run off buffer */
					data_bounds = TRUE;
					x -= d;		/* ignore this run */
					btr = d / 2 + (d & 1)
						+ (((d + 1) & 2) >> 1);
					for (; btr > 0; btr--) {
						if ((e = fgetc(fd)) == EOF)
							goto data_short;
					}
					continue;
				}
				for (i = d / 2; i > 0; i--) {
					if ((e = fgetc(fd)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
					*(data++) = e & 0xf;
				}
				if (d & 1) {
					if ((e = fgetc(fd)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
				}
				if ((d + 1) & 2)	/* read pad byte */
					if (fgetc(fd) == EOF)
						goto data_short;
			}
		} else {	/* No 4 bit rle compression */
			int d, s, p;
			int i, e;
			d = image->width / 2;	/* double pixel count */
			s = image->width & 1;	/* single pixel */
			p = (4 - (d + s)) & 0x3;	/* byte pad */
			for (y = image->height; y > 0; y--, data -= (2 * illen)) {
				for (i = d; i > 0; i--) {
					if ((e = fgetc(fd)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
					*(data++) = e & 0xf;
				}
				if (s) {
					if ((e = fgetc(fd)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
				}
				for (i = p; i > 0; i--)
					if (fgetc(fd) == EOF)
						goto data_short;
			}
		}
	} else if (hdr.biBitCount == 8) {
		unsigned char *data;
		int illen, x, y;

		illen = image->width;
		/* start at bottom */
		data = image->data + (image->height - 1) * illen;

		if (hdr.biCompression == BI_RLE8) {
			int d, e;

			bzero((char *) image->data,
			      image->width * image->height);
			for (x = y = 0;;) {
				if ((d = fgetc(fd)) == EOF)
					goto data_short;
				if (d != 0) {	/* run of pixels */
					x += d;
					if (x > image->width ||
							y > image->height) {
						/* don't run off buffer */
						data_bounds = TRUE;
						/* ignore this run */
						x -= d;
						if ((e = fgetc(fd)) == EOF)
							goto data_short;
						continue;
					}
					if ((e = fgetc(fd)) == EOF)
						goto data_short;
					bfill( (char*) data, d, e);
					data += d;
					continue;
				}
				/* else code */
				if ((d = fgetc(fd)) == EOF)
					goto data_short;
				if (d == 0) {	/* end of line */
					data -= (x + illen);
					x = 0;
					y++;
					continue;
				}
				/* else */
				if (d == 1)	/* end of bitmap */
					break;
				/* else */
				if (d == 2) {	/* delta */
					if ((d = fgetc(fd)) == EOF ||
							(e = fgetc(fd)) == EOF)
						goto data_short;
					x += d;
					data += d;
					y += e;
					data -= (e * illen);
					continue;
				}
				/* else run of literals */
				x += d;
				if (x > image->width || y > image->height) {
					int btr;
					/* don't run off buffer */
					data_bounds = TRUE;
					/* ignore this run */
					x -= d;	
					btr = d + (d & 1);
					for (; btr > 0; btr--) {
						if ((e = fgetc(fd)) == EOF)
							goto data_short;
					}
					continue;
				}
				if (fread(data, 1, d, fd) != d)
					goto data_short;
				data += d;
				if (d & 1)	/* read pad byte */
					if (fgetc(fd) == EOF)
						goto data_short;
			}
		} else {	/* No 8 bit rle compression */
			unsigned char pad[4];
			int padlen;

		        /* extra bytes to word boundary */
			padlen = ((image->width + 3) & ~3) - illen;
			for (y = image->height; y > 0; y--, data -= illen) {
				if (fread(data, 1, illen, fd) != illen
				    || fread(pad, 1, padlen, fd) != padlen)
					goto data_short;
			}
		}
	} else if (hdr.biBitCount == 16) {
		unsigned char *data, pad[4];
		int illen, padlen, y;
		illen = 3 * image->width;
	        /* extra bytes to word boundary */
	        padlen = ((illen + 3) & ~3) - illen;
		/* start at bottom */	
		data = image->data + (image->height - 1) * illen;	
		for (y = image->height; y > 0; y--, data -= illen) {
			int i;

			/* BMP files are RGB, so read straight in. */
			if (fread(data, 1, illen*2/3, fd) != illen
                            || fread(pad, 1, 0, fd) != 0)
				goto data_short;
			/* Oh, no they're not */
			for (i = 3 * image->width - 1; i > 0; i -= 3) {
				int t = data[i];
				data[i] = data[i - 2];
				data[i - 2] = t;
			}
		}
	} else {		/* hdr.biBitCount == 24 */
		unsigned char *data, pad[4];
		int illen, padlen, y;
		illen = 3 * image->width;
	        /* extra bytes to word boundary */
	        padlen = ((illen + 3) & ~3) - illen;
		/* start at bottom */	
		data = image->data + (image->height - 1) * illen;	
		for (y = image->height; y > 0; y--, data -= illen) {
			int i;

			/* BMP files are RGB, so read straight in. */
			if (fread(data, 1, illen, fd) != illen
                            || fread(pad, 1, padlen, fd) != padlen)
				goto data_short;
			/* Oh, no they're not */
			for (i = 3 * image->width - 1; i > 0; i -= 3) {
				int t = data[i];
				data[i] = data[i - 2];
				data[i - 2] = t;
			}
		}
	}
	if (data_bounds)
		fprintf(stderr, "bmpLoad: Data outside image area\n");

	fclose(fd);
	return image;

      data_short:
	fprintf(stderr, "bmpLoad: Short read within Data\n");
	fclose(fd);
	return image;
}

static 
void putshort(fp, i)
     FILE *fp;
     int i;
{
   int c, c1;

   c = ((unsigned int ) i) & 0xff;  
   c1 = (((unsigned int) i)>>8) & 0xff;
   putc(c, fp);   
   putc(c1,fp);
}

static 
void putint(fp, i)
     FILE *fp;
     int i;
{
   int c, c1, c2, c3;
   c  = ((unsigned int ) i)      & 0xff;
   c1 = (((unsigned int) i)>>8)  & 0xff;
   c2 = (((unsigned int) i)>>16) & 0xff;
   c3 = (((unsigned int) i)>>24) & 0xff;

   putc(c, fp);   
   putc(c1,fp);  
   putc(c2,fp);  
   putc(c3,fp);
}

int
WriteBMP(char *file, Image * image)
{
    FILE *fp;
    int x, y, i, j, colsize, bpl, w, h, nbits;
    unsigned char *ip;
    Image *cmapImage = NULL;
    unsigned char buf[3];

    if (!(fp = fopen(file, "wb")))
        return 1;
   
    /* Write BMP header */
    putc('B', fp);  putc('M', fp);           /* BMP file magic number */

    w = image->width;
    h = image->height;
    if (image->isBW) {
        colsize = 2;
        nbits = 1;
        bpl = ((w+31)/32)*4;
    } else {
        /* try compressing image to palette mode, but don't force if too big */
        if (!image->alpha)   /* can't store alpha mask with palette image */
            cmapImage = ImageCompress(image, 256, 1);
        if (cmapImage) {
            image = cmapImage;  /* original was deleted in ImageCompress() */
        }
	if (image->scale==1) {
            colsize = image->cmapSize;
            nbits = 8;
            bpl = (w+3) & ~3 ;
	} else {
            colsize = 0;
            nbits = 24;
            bpl = (3*w+3) & ~3;
	}
    }

    /* compute filesize and write it */
    i = 14 +                /* size of bitmap file header */
      40 +                  /* size of bitmap info header */
      (colsize * 4) +       /* size of colormap */
      bpl * h;              /* size of image data */
   
    putint(fp, i);
    putshort(fp, 0);        /* reserved1 */
    putshort(fp, 0);        /* reserved2 */
    putint(fp, 14 + 40 + (colsize * 4));  /* offset from BOfile to BObitmap */        
    putint(fp, 40);         /* biSize: size of bitmap info header */
    putint(fp, w);          /* biWidth */
    putint(fp, h);          /* biHeight */
    putshort(fp, 1);        /* biPlanes:  must be '1' */
    putshort(fp, nbits);    /* biBitCount: 1,4,8, or 24 */
    putint(fp, BI_RGB);     /* biCompression:  BI_RGB, BI_RLE8 or BI_RLE4 */
    putint(fp, bpl*h);      /* biSizeImage:  size of raw image data */
    putint(fp, 75 * 39);    /* biXPelsPerMeter: (75dpi * 39" per meter) */
    putint(fp, 75 * 39);    /* biYPelsPerMeter: (75dpi * 39" per meter) */
    putint(fp, colsize);    /* biClrUsed: # of colors used in cmap */
    putint(fp, colsize);    /* biClrImportant: same as above */

    /* Write colormap */
    for (i=0; i<colsize; i++) {
        j = 3*i;
        putc(image->cmapData[j+2],fp);
        putc(image->cmapData[j+1],fp);
        putc(image->cmapData[j],fp);
        putc('\0',fp);
    }

    if (image->isBW) {
        for (y = h-1; y>=0; y--) {
	   *buf = 0;
           for (x = 0; x < w; x++) {
	       if (image->data[x+y*w]) *buf |= (128>>(x&7));
	       if ((x&7)==7) {
                   putc(*buf, fp);
                   *buf = 0;
	       }
	   }
	   j = bpl - w/8;
	   for (i=0; i<j; i++) {
               putc(*buf, fp);
               *buf = 0;
	   }
	}
    } else
    if (image->scale==1) {
        for (y = h-1; y>=0; y--) {
	    for (x = 0; x < w; x++)
	        putc(image->data[x+y*w], fp);
            j = bpl-w;
	    for (i=0; i<j; i++) putc('\0', fp);
	}
    } else {
        for (y = h-1; y>=0; y--) {
	    for (x = 0; x < w; x++) {
	        ip = &image->data[3*(x+y*w)];
                buf[0] = ip[2]; buf[1] = ip[1]; buf[2] = ip[0];
	        fwrite(buf, 1, 3, fp);
	    }
            j = bpl-3*w ;
	    for (i=0; i<j; i++) putc('\0', fp);
	}
    }

    fclose(fp);
    return 0;
}
