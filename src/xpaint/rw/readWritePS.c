/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)		       | */
/* |								       | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.	 There is no	       | */
/* | representations about the suitability of this software for	       | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.					       | */
/* |								       | */
/* +-------------------------------------------------------------------+ */

/* $Id: readWritePS.c,v 1.14 2005/03/20 20:15:34 demailly Exp $ */

/* default ghostcript resolution
 * replace by 72 if 300 is too much to wish for ...
 */

#define GS_RESOLUTION 300.0

#define MIN(a,b)       (((a) < (b)) ? (a) : (b))

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "xpaint.h"
#include "image.h"
#include "libpnmrw.h"
#include "misc.h"
#include "print.h"

#if defined(sco) || defined(__CYGWIN__)
#include <time.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

#include <string.h>
#include <X11/Intrinsic.h>

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

enum {NO_FMT=0, PS_FMT, PDF_FMT, TEX_FMT, LTX_FMT, DVI_FMT, SVG_FMT, TXT_FMT};

typedef unsigned char byte;
typedef unsigned int  uint;

extern int file_isSpecialImage;
extern int file_transparent;
extern int file_numpages;
extern int file_force;
extern int file_bbox;
extern int file_specified_zoom;
extern int paper_sizes[15][2];

extern void * xmalloc(size_t n);
extern FILE * openTempFile(char **np);
extern void removeTempFile(void);
extern void openTempDir(char *);
extern Image * ReadPNG(char *file);
extern int TestPNG(char *file);
extern void setPageSize(int *w, int *h);
extern double GetDpi();

/*****************************************************************************
 *
 * ppmtops.c below is derived from work by (in chronological order)
 * Evgeni Chernyaev <chernaev@mx.ihep.su> (around 1993-1994)
 * Tim Adye <T.J.Adye@rl.ac.uk> (1994-1999 : color modes, LZW compression)
 * Péter Szabó <pts@fazekas.hu> (2001-2003 : work on sam2p, PNG predictors)
 *
 *****************************************************************************/

#define MAXLINE 79

static int Nbyte;
static int LineBreak;
static int formatting = 0;
static int bpp_in, bpp_out;

/*
 * This is used for compression
 * predictor0 = 0     adjust to best compression mode
 * predictor0 = 1..4  select PNG predictor 1..4
 * predictor0 = 5     no compression
 *
 */
static int predictor0;
static FILE *fo;
static Image *image0 = NULL;
static PageInfo *pinfo0 = NULL;

static void put_char(int c)
{
    fputc(c, fo);
    ++Nbyte;
    if (formatting) {
        if (Nbyte%MAXLINE==LineBreak) {
            put_char('\n');
            formatting = 2;
        } else
            formatting = 1;
    }
}

static void put_string(p)
         const char *p;
{
    while (*p) put_char(*p++);
}

static void set_linebreak()
{
    LineBreak = (Nbyte-1)%MAXLINE;
}

/***********************************************************************
 *                                                                     *
 * Name: ASCII85encode                               Date:    05.11.93 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised: 20.04.95 *
 *                                                                     *
 ***********************************************************************/
static void ASCII85encode(k, Buf)
                     int  k;
                     byte Buf[];
{
  unsigned int Value;
  char s[8];
  int           j, n;

  if (k == 0) return;
  s[5] = '\0';
  for (j=0; j<k; j+=4) {
    Value = Buf[j]<<24;
    if (++j < k) Value |= Buf[j]<<16;
    if (++j < k) Value |= Buf[j]<<8;
    if (++j < k) Value |= Buf[j];
    j -= 3;
    for (n=4; n>=0; n--) {
      s[n]   = Value % 85 + 33;
      Value /= 85;
    }
    /* Maybe not needed with ghostscript ...
    if (j == 0 && s[0] == '%')
      put_char(' ');
    */
    if (k-j < 4) s[k-j+1] = '\0';
    put_string(s);
  }
}

#define BITS           12                       /* largest code size */
#define HSIZE          5003L                    /* hash table size */
#define SHIFT          4                        /* shift for hashing */
#define CLEARCODE      256                      /* Clear Code */
#define EOD            257                      /* End Of Data code */
#define PIXS           170000                   /* largest # of pixels */

/***********************************************************************
 *                                                                     *
 * Name: PutCode (LZW encoding)                      Date:    05.11.93 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised:          *
 *                                                                     *
 ***********************************************************************/
static void PutCode(Code, CodeSize)
                int Code, CodeSize;
{
  static int k, PartA, PartB, SizeA, SizeB;
  static const int mask[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                      0x001F, 0x003F, 0x007F, 0x00FF,
                                      0x01FF, 0x03FF, 0x07FF, 0x0FFF };
  static byte Accum[60];
  if (Code == -1) {
    k     = 0;
    PartA = 0;
    SizeA = 0;
    return;
  }

  PartB = Code;
  SizeB = CodeSize;

  while (SizeB >= 8) {
    SizeB  = SizeA + SizeB - 8;
    Accum[k++] = PartA | (PartB >> SizeB);
    if (k == 60) {
      ASCII85encode(k,Accum);
      k = 0;
    }
    PartB &= mask[SizeB];
    SizeA = 0;
    PartA = 0;
  }

  SizeA = SizeB;
  PartA = PartB << (8-SizeB);
  if (Code == EOD) {
    if (SizeA != 0) Accum[k++] = PartA;
    ASCII85encode(k, Accum);
  }
}

void AdjustGrayScale(byte *Pos, int Width)
{
    int i;
    for (i=0; i<Width; i++)
        Pos[i]=(32*Pos[3*i]+50*Pos[3*i+1]+18*Pos[3*i+2])/100;
}

void
ReadImageLine(int y, int Width, byte *Pos)
{
    int x;
    unsigned char *p, *q, *pn, *qn;
    int w = pinfo0->wsubdiv;
    int h = pinfo0->hsubdiv;
    int a, b, k, l, m, s;

    if (w == 1 && h == 1) {
      for (x=0; x<image0->width; x++) {
        p = ImagePixel(image0, x, y);
        memcpy(&Pos[3*x], p, 3);
      }
      return;
    }

    if (h == 1) {
      q = ImagePixel(image0, 0, y);
      s = 0;
      for (x=1; x<=image0->width; x++) {
        p = q;
	if (x<image0->width) q = p+bpp_in;
        for (k=0; k<w; k++)
        for (m=0; m<bpp_in; m++)
	  Pos[s++] = (p[m]*(w-k)+q[m]*k)/w;
      }
      return;
    }

    if (w == 1) {
      p = ImagePixel(image0, 0, (s=y/h));
      pn = ImagePixel(image0, 0, s+(s<image0->height-1));
      l = y-s*h;
      s = 0;
      for (x=0; x<image0->width; x++) {
        for (m=0; m<bpp_in; m++) {
	  Pos[s++] = (*p*(h-l)+*pn*l)/h;
          p++ ; pn++;
	}
      }
      return;
    }

    q = ImagePixel(image0, 0, (s=y/h));
    qn = ImagePixel(image0, 0, s+(s<image0->height-1));
    l = y-s*h;
    s = 0;
    for (x=1; x<=image0->width; x++) {
      p = q;
      pn = qn;
      if (x<image0->width) {
        q = p+bpp_in;
        qn = pn+bpp_in;
      }
      for (k=0; k<w; k++)
      for (m=0; m<bpp_in; m++) {
	a = (p[m]*(h-l)+pn[m]*l)/h;
	b = (q[m]*(h-l)+qn[m]*l)/h;
        Pos[s++] = (a*(w-k)+b*k)/w;
      }
    }
}

void
FilterLine(int predictor, int start, int y, int Width, byte *ScanLine)
{
    int i, p, pu, pl, pc, sum;
    static byte *Output, *Pos, *Up, *Left, *Corner;
    static int mini, Length;

    if (start) {
       if (start == -1) {
	  Length = bpp_out*Width;
          Up = ScanLine+Length+bpp_out+1;
          Corner = Up-bpp_out;
          Pos = ScanLine+2*Length+2*bpp_out+1;
          Left = Pos-bpp_out;
          Output = ScanLine+3*Length+2*bpp_out+1;
       } else
 	  memcpy(Up, Pos, Length);
       mini = 2147483647;
       ReadImageLine(y, Width, Pos);
       /* convert color to gray in case of color input and gray output */
       if (bpp_out<bpp_in) AdjustGrayScale(Pos, Width);
    }

    switch(predictor) {
    case 1:
       for (i=0; i<Length; i++)
	  Output[i] = (Pos[i]-Left[i])&255;
       break;
    case 2:
       for (i=0; i<Length; i++)
	  Output[i] = (Pos[i]-Up[i])&255;
       break;
    case 3:
       for (i=0; i<Length; i++)
	  Output[i] = (Pos[i]-((uint)Left[i]+(uint)Up[i])/2)&255;
       break;
    case 4:
    default:
       /* Paeth predictor */
       for (i=0; i<bpp_out; i++)
	  Output[i] = (Pos[i]-Up[i])&255;
       for (i=bpp_out; i<Length; i++) {
	  p = (int)Left[i]+(int)Up[i]-(int)Corner[i];
          pl = p-(int)Left[i];
          if (pl<0) pl=-pl;
          pu = p-(int)Up[i];
          if (pu<0) pu=-pu;
          pc = p-(int)Corner[i];
          if (pc<0) pc=-pc;
          if (pl<=pu && pl<=pc) 
	     Output[i] = (Pos[i]-Left[i])&255;
          else
          if (pu<=pc) 
             Output[i] = (Pos[i]-Up[i])&255;
          else
             Output[i] = (Pos[i]-Corner[i])&255;
       }
       break;
    }
    sum = 0;
    for (i=0; i<Length; i++)
        sum += (Output[i]<128)? Output[i]:256-Output[i];
    if (sum<mini) {
        mini = sum;
        *ScanLine = predictor;
        memcpy(ScanLine+1, Output, Length);
    }
}

static void GetScanLine(int y, int Width, byte * ScanLine)
{
    int predictor;

    /* just raw data without predictor */
    if (predictor0==5) {
       ReadImageLine(y, Width, ScanLine);
       if (bpp_out<bpp_in) AdjustGrayScale(ScanLine, Width);
       return;
    }

    if (y==0) {
       /* first row; initialize pointers & use predictor=1 */
      FilterLine(1, -1, y, Width, ScanLine);
    } else
    if (predictor0==0) {
       /* find best predictor */
       for (predictor=1; predictor<=4; predictor++)
	 FilterLine(predictor, (predictor==1), y, Width, ScanLine);
    } else
       FilterLine(predictor0, 1, y, Width, ScanLine);
}

/***********************************************************************
 *                                                                     *
 * Name: EncodeData                                  Date:    05.11.93 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised: 21.06.94 *
 *                                                            04.06.94 *
 * Function: Lempel-Ziv Welch encoding of an image                     *
 *                                                                     *
 ***********************************************************************/
static void EncodeData(Width, Height, ScanLine)
                  int  Width, Height;
                  byte ScanLine[];
{
  int    k, x, y, disp, Code=0, K, Length;
  long   CodeK, Npix;
  int    FreeCode, CurCodeSize, CurMaxCode;

  long   HashTab [HSIZE];                       /* hash table */
  int    CodeTab [HSIZE];                       /* code table */

  /*   L W Z   C O M P R E S S I O N   */

  PutCode(-1, 0);
  FreeCode    = CLEARCODE + 2;
  CurCodeSize = 9;
  CurMaxCode  = 511;
  Length = bpp_out*Width;

  memset((char *) HashTab, -1, sizeof(HashTab));
  PutCode(CLEARCODE, CurCodeSize);              /* 1st - clear code */
  Npix = 0;
  predictor0 = 0;                               /* find best predictor */

  for (y=0; y<Height; y++) {
    GetScanLine(y, Width, ScanLine);
    x = 0;
    if (y == 0) Code  = ScanLine[x++];
    while(x <= Length) {
      K = ScanLine[x++];                        /* next symbol */
      Npix++;
      CodeK = ((long) K << BITS) + Code;        /* set full code */
      k = (K << SHIFT) ^ Code;                  /* xor hashing */

      if (HashTab[k] == CodeK) {                /* full code found */
        Code = CodeTab[k];
        continue;
      }
      else if (HashTab[k] < 0 )                 /* empty slot */
        goto NOMATCH;

      disp  = HSIZE - k;                        /* secondary hash */
      if (k == 0) disp = 1;

PROBE:
      if ((k -= disp) < 0)
        k  += HSIZE;

      if (HashTab[k] == CodeK) {                /* full code found */
        Code = CodeTab[k];
        continue;
      }

      if (HashTab[k] > 0)                       /* try again */
        goto PROBE;

NOMATCH:                                        /* full code not found */
      PutCode(Code, CurCodeSize);
      Code = K;
      if (FreeCode == CurMaxCode) {
        CurCodeSize++;
        CurMaxCode = CurMaxCode*2 + 1;
      }

      if (CurCodeSize <= BITS && Npix <= PIXS) {
        CodeTab[k] = FreeCode++;                /* code -> hashtable */
        HashTab[k] = CodeK;
      }else{
        if (CurCodeSize > BITS) CurCodeSize = BITS;
        PutCode(CLEARCODE, CurCodeSize);
        memset((char *) HashTab, -1, sizeof(HashTab));
        FreeCode    = CLEARCODE + 2;
        CurCodeSize = 9;
        CurMaxCode  = 511;
        Npix = 0;
      }
    }
  }
   /*   O U T P U T   T H E   R E S T  */

  PutCode(Code, CurCodeSize);
  if (FreeCode == CurMaxCode && CurCodeSize != BITS)
    CurCodeSize++;
  PutCode(EOD, CurCodeSize);
}

static void WriteData(Width, Height, ScanLine)
                  int  Width, Height;
                  byte *ScanLine;
{
  int x, y;
  char s[6];
  predictor0 = 5;
  for (y=0; y<Height; y++) {
    GetScanLine(y, Width, ScanLine);
    for (x=0; x<bpp_out*Width; x++) {
      sprintf(s, "%02x", ScanLine[x]);
      put_string(s);
    }
    if (y<Height-1) {
      if (formatting==2)
         put_char(' ');
      else {
         put_char('\n');
         set_linebreak();
      }
    }
  }
}

/*****************************************************************************
 *
 * psencode(image, pinfo, FileName, title)
 *
 * Function: Output image in PostScript format with ASCII85 and 
 *           Lempel-Ziv Welch encoding (patent expired in 2004)
 *
 *****************************************************************************/
long psencode(image, pinfo, title)
     Image *image;
     PageInfo * pinfo;
     char *title;
{
  static byte * ScanLine;
  int        Width, Height, N;
  time_t     clock;
  char       buffer[2048];

  /*   I N I T I A L I S A T I O N   */

  Width = image->width * pinfo->wsubdiv;
  Height = image->height * pinfo->hsubdiv;
  image0 = image;
  pinfo0 = pinfo;

  bpp_in = 3;
  bpp_out = (pinfo->gray)?1:3;
  N = 1+2*bpp_out+(4*bpp_out+(bpp_in>bpp_out))*Width;

  ScanLine = (byte *)xmalloc(N);
  memset(ScanLine, 0, N);
  Nbyte  = 0;

  /*   O U T P U T   H E A D E R   */

  sprintf(buffer,
    "%%!PS-Adobe-3.0%s\n"
    "%%%%Title: %s\n"
    "%%%%Creator: xpaint-v%s\n"
    "%%%%CreationDate: %s"
    "%%%%BoundingBox: 0 0 %d %d\n"
    "%%%%DocumentData: Clean7Bit\n"
    "%%%%LanguageLevel: 2\n"
    "%%%%Pages: 1\n"
    "%%%%EndComments\n"
    "%%%%Page: 1 1\n%s",
    pinfo->eps? "" : " EPSF-3.0",
    title, XPAINT_VERSION,
    (time(&clock),ctime(&clock)),
    pinfo->wbbox, pinfo->hbbox,  pinfo->eps? "gsave\n" : "");
  put_string(buffer);
  
  if (pinfo->rx || pinfo->ry) {
    sprintf(buffer, "%d %d translate\n", pinfo->rx, pinfo->ry);
    put_string(buffer);
  }
  if (pinfo->orient) {
    sprintf(buffer, "%d %d translate  90 rotate\n",
	    (int)(0.01 * image->height * pinfo->hpercent), 0);
    put_string(buffer);
  }

  if (pinfo->compress) {
    sprintf(buffer,
      "%g %g scale\n" "save 9 dict begin {\n"
      "/T currentfile/ASCII85Decode filter def/%s setcolorspace /F T\n"
      "<< /BitsPerComponent 8/Columns %d/Colors %d/Predictor 10 >>\n"
      "/LZWDecode filter def << /ImageType 1"
      "/Width %d/Height %d/BitsPerComponent 8\n"
      "/ImageMatrix[1 0 0 -1 0 %d]/Decode %s/DataSource F >> image\n"
      "F closefile T closefile} exec\n",
      0.01 * pinfo->wpercent/pinfo->wsubdiv, 
      0.01 * pinfo->hpercent/pinfo->hsubdiv,
      pinfo->gray? "DeviceGray" : "DeviceRGB", Width, pinfo->gray? 1 : 3,
      Width, Height, Height,
      pinfo->gray?"[0 1]" : "[0 1 0 1 0 1]");
    put_string(buffer);
  } else {
    sprintf(buffer, "%g %g scale\n/line %d string def\n"
                    "%d %d 8\n"
                    "[ %d 0 0 -%d 0 %d ]\n"
                    "{currentfile line readhexstring pop}\n" "%s",
       0.01*pinfo->wpercent*image->width, 0.01*pinfo->wpercent*image->height,
       Width, Width, Height, Width, Height, Height,
       (pinfo->gray)? "image\n" : "false 3 colorimage\n");
    put_string(buffer);
  }

  formatting = 1;
  set_linebreak();

  if (pinfo->compress) {
    EncodeData(Width, Height, ScanLine);
    put_string("~>");
  } else
    WriteData(Width, Height, ScanLine);

  /* add newline unless just added (so as to avoid blank lines!) */
  if (formatting != 2) put_char('\n');
  formatting = 0;

  if (pinfo->compress)
     put_string("end restore\n");
  if (pinfo->eps) 
     put_string("grestore showpage\n");
  else
     put_string("showpage\n");
  put_string("%%Trailer\n%%EOF\n");
  fflush(fo);
  fclose(fo);
  free(ScanLine);  
  return (Nbyte);
}

int WriteResizedPS(char *file, Image * image, void *data)
{
    PageInfo *pinfo = (PageInfo *)data;
    char *cp, title[256];

    if (!image) return 1;

    if ((fo = fopen(file, "w")) == NULL)
	return 1;

    if (pinfo->title && *pinfo->title) {
        if ((cp = strrchr(pinfo->title, '/')) == NULL)
            cp = pinfo->title;
        else
            cp++;
        strcpy(title, cp);
        if ((cp = strrchr(title, '.')) != NULL)
            *cp = '\0';
    } else
        strcpy(title, "_untitled");

    strcat(title, (pinfo->pdf)? ".pdf" : ".ps");

    psencode(image, pinfo, title);
    return 0;
}

/* Test Postscript and other related vector formats */
int
TestPS_(char *file)
{
    char header[512];
    FILE *fp = fopen(file, "rb");   /* libpng requires ANSI; so do we */

    if (!fp)
        return 0;

    fread(header, 1, 8, fp);
    fclose(fp);

    if (!strncasecmp(header, "%!PS", 4)) return PS_FMT;
    if (!strncasecmp(header, "%PDF", 4)) return PDF_FMT;
    if (!strncasecmp(header, "%TEX", 4)) return TEX_FMT;
    if (!strncasecmp(header, "%LTX", 4)) return LTX_FMT;
    if (!strncasecmp(header, "\367\002", 2)) return DVI_FMT;
    if (strstr(file, ".ltx")) return LTX_FMT;
    if (strstr(file, ".tex")) return TEX_FMT;
    if (strstr(file, ".svg") && !strncasecmp(header, "<?xml", 5)) {
       fp = fopen(file, "rb");
       fread(header, 1, 512, fp);
       fclose(fp);
       if (strstr(header, "<svg")) return SVG_FMT;
    }
    if (strstr(file, ".txt") || !strncasecmp(header, "\\*text", 6) ||
        Global.astext)
        return TXT_FMT;
    return 0;
}

/* read PostScript and other related vector formats */
Image *
ReadPS_(char *file)
{
    char *tmp;
    char scan[80];
    char rad[512];
    static char * last_rad = NULL;
    static int  last_type = 0;
    char buffer[2048];
    char *sc, *dc, *ptr;
    FILE *fp;
    int wth, hth, type_doc;
    int i, j;
    double x1=0.0, y1=0.0, x2=1000.0, y2=1414.0, res;
    Image *image = NULL;
    Pixmap pix;
    Colormap cmap;
    void *info = NULL;
    char *dir;

    fp = fopen(file, "rb");
    if (!fp) return NULL;
    fclose(fp);

    fp = openTempFile(&tmp);

    if (!fp) return NULL;
    fclose(fp);

    type_doc = TestPS_(file);
    if (type_doc==NO_FMT) 
        return NULL; /* should not happen anyway ... */
    
    file_isSpecialImage = 1;
    file_transparent = 1;

    openTempDir(rad);
    dir = strdup(rad);
    strcat(rad, "/");
    strcpy(buffer, file);
    if ((sc=strrchr(buffer, '/'))) {
        *sc = '\0';
        ++sc;
    } else
        sc = buffer;
    if ((dc=strrchr(sc, '.')))
        *dc = '\0';
    strcat(rad, sc);
   
    if (type_doc>=PDF_FMT && type_doc<=DVI_FMT) { /* TeX et al documents */
       if (type_doc==PDF_FMT)
           sprintf(buffer, 
               "pdf2ps %s %s.ps ", file, rad);
       else
       if (type_doc==TEX_FMT) {
           sprintf(buffer, 
               "cd %s ; tex \"\\\\nonstopmode\\\\input\" %s ; dvips %s.dvi -o %s.ps ",
	       dir, file, rad, rad);
           free(dir);
       } else
       if (type_doc==LTX_FMT) {
           sprintf(buffer, 
               "cd %s ; latex \"\\\\nonstopmode\\\\input\" %s ; dvips %s.dvi -o %s.ps ",
	       dir, file, rad, rad);
           free(dir);
       } else
       if (type_doc==DVI_FMT)
           sprintf(buffer, 
               "dvips %s -o %s.ps ", file, rad);
       strcat(rad, ".ps");
       if (file_force || type_doc != last_type || 
           !last_rad || strcmp(last_rad, rad)) {
	   printf("Running:\n%s\n", buffer);
           system(buffer);
           file_force = 0;
           last_rad = (char *)realloc(last_rad, strlen(rad)+2);
           strcpy(last_rad, rad);
           last_type = type_doc;
       }
    } else
       strcpy(rad, file);

    if (type_doc==TXT_FMT) {
       fp = fopen(rad, "rb");
       fgets(buffer, 2040, fp);
       wth = 1536;
       hth = 2172;
       if (!strncmp(buffer, "\\*text:", 7)) {
	   sscanf(buffer+7, "%d %d*", &wth, &hth);
           if (wth<=0) wth = 1536;
           if (hth<=0) hth = 2172;
           ptr = strstr(buffer, "\\*zoom:");
           if (ptr)
	       file_specified_zoom=atoi(ptr+7);
       }

       /* initialize pixmap and data structures */
       sprintf(buffer, "%d %d", wth, hth);
       info = NULL;
       WriteText(Global.toplevel, &info, buffer);

       rewind(fp);
       while (!feof(fp)) {
	   if (fgets(buffer, 2040, fp))
               WriteText(Global.toplevel, &info, buffer);
       }
       pix = GetLocalInfoDrawable(info);
       XtVaGetValues(Global.toplevel, XtNcolormap, &cmap, NULL);
       image = PixmapToImage(Global.toplevel, pix, cmap);

       /* free structures */
       WriteText(Global.toplevel, &info, NULL);
       fclose(fp);
       return image;
    }

    wth = Global.default_width;
    hth = Global.default_height;

    if (type_doc==SVG_FMT) {
       if ((res=GetDpi()) != 0)       
	  hth = (int)(hth * res/72.0 + 0.5);
       sprintf(buffer, 
          "inkscape %s --export-height=%d --export-png=%s",
	  file, hth, tmp);
       printf("Running:\n%s\n", buffer);
       system(buffer);
       if (TestPNG(tmp)) image = ReadPNG(tmp);
       removeTempFile();
       return image;
    }

    fp = fopen(rad, "rb");
    i = 0;
    j = 0;
    file_numpages = -1;
    if (fp)
    while (!feof(fp)) {
       fgets(buffer, 2040, fp);
       ++j;
       if (!strncasecmp(buffer, "%%BoundingBox:", 14)) {
	   sscanf(buffer, "%s %lf %lf %lf %lf", scan, &x1, &y1, &x2, &y2);
           i = 1;
       }
       if (!strncasecmp(buffer, "%%HiResBoundingBox:", 19)) {
           sscanf(buffer, "%s %lf %lf %lf %lf", scan, &x1, &y1, &x2, &y2);
           i = 2;
       }
       if (!strncmp(buffer, "%%Pages:",8) && !strstr(buffer,"(atend)")) {
	   file_numpages = atoi(buffer+8);
       }
       if (file_numpages>=0 && i==2) break;
       if (file_numpages>=0 && i==1 && j>=20) break;
    }
    if (fp) fclose(fp);

    if (!i || file_bbox==0) {
       x1 = y1 = 0;
       setPageSize(&i, &j);
       x2 = (double)i;
       y2 = (double)j;
    }

    if (i && file_bbox==1) {
       int mx = 1191, my = 1684;
       x1 = y1 = 0;
       if (x2<y2) {
          for (j = 0; j<15; j++) {
	      if (x2<paper_sizes[j][0]+1.5 && y2<paper_sizes[j][1]+1.5 &&
		  paper_sizes[j][0]<=mx && paper_sizes[j][1]<=my) {
		 mx = paper_sizes[j][0];
		 my = paper_sizes[j][1];
	      } 
	  }
          x2 = (double) mx;
          y2 = (double) my;
       }
       if (x2>=y2) {
          for (j = 0; j<15; j++) {
	      if (y2<paper_sizes[j][0]+1.5 && x2<paper_sizes[j][1]+1.5 &&
		  paper_sizes[j][0]<=mx && paper_sizes[j][1]<=my) {
		 mx = paper_sizes[j][0];
		 my = paper_sizes[j][1];
	      } 
	  }
          y2 = (double) mx;
          x2 = (double) my;
       }
    }
    if (i && file_bbox==2)
       x1 = y1 = 0;

    if ((res=GetDpi()) == 0)
          res = GS_RESOLUTION * MIN(wth/x2, hth/y2);

    wth = (int) (0.5+res*x2/72.0);
    hth = (int) (0.5+res*y2/72.0);

    /*
    if (type_doc==PDF_FMT)
         sprintf(buffer, 
                "gs -sDEVICE=pngalpha -r%f -g%dx%d -dNOPAUSE -dQUIET "
                "-dFirstPage=%d -dLastPage=%d "
                "-sOutputFile=%s %s -c quit", 
		res, wth, hth, Global.numpage, Global.numpage,
                tmp, rad);
    */
    sprintf(buffer, 
                "psselect -p%d %s | "
                "gs -sDEVICE=pngalpha -r%f -g%dx%d -dNOPAUSE -dQUIET "
                "-sOutputFile=%s - -c quit", 
                Global.numpage, rad,
		res, wth, hth, tmp);
    printf("Running:\n%s\n", buffer);
    system(buffer);

    if (TestPNG(tmp)) image = ReadPNG(tmp);
    removeTempFile();

    if (!image) return NULL;

    if (image->alpha) {
        i = image->width*image->height;
        j = 1;
        while (i>0)
	    if (image->alpha[--i] != 0xff) { j = 0; break; }
        if (j) {
            free(image->alpha);
            image->alpha = NULL;
	}
    }
    if (!image->alpha)
        file_transparent = 0;

    i = (int) (res*x1/72.0);
    j = (int) (res*y1/72.0);

    /* crop left and bottom bordures in case of BBox= */
    if (file_bbox==3 && (i>=0 && j>=0) && (i+j>0)) {
        int dw, dh, wth1, hth1;
        unsigned char *data;
        dw = i;
        dh = j;
        wth1 = wth - i;
        hth1 = hth - j;
        data = (unsigned char*) malloc(wth1*hth1*image->scale);
        if (!data) return image;
        for (j=0; j<hth1; j++)
	    memcpy(data+j*wth1*image->scale, 
                   image->data+(dw+j*wth)*image->scale, wth1*image->scale);
        free(image->data);
        image->data = data;
        if (image->alpha) {
            data = (unsigned char*) malloc(wth1*hth1);
            if (!data) {
	        free(image->alpha);
                image->alpha = NULL;
	    }
            for (j=0; j<hth1; j++)
	        memcpy(data+j*wth1, image->alpha+dw+j*wth, wth1);
            free(image->alpha);
            image->alpha = data;
	}
        image->width = wth1;
        image->height = hth1;
    }

    return image;
}

int WritePS(char *file, Image * image)
{
    static PageInfo pinfo;

    pinfo.wbbox = image->width;
    pinfo.hbbox = image->height;
    pinfo.width = image->width;
    pinfo.height = image->height;
    pinfo.rx = 0;
    pinfo.ry = 0;
    pinfo.wsubdiv = 1;
    pinfo.hsubdiv = 1;
    pinfo.orient = 0;
    pinfo.eps = 1;
    pinfo.wpercent = 100.0;
    pinfo.hpercent = 100.0;
    pinfo.title = file;
    pinfo.compress = 1;

    return WriteResizedPS(file, image, &pinfo);
}

int WritePDF(char *file, Image * image)
{
    char *tmp;
    char buffer[512];
    FILE *fp;

    fp = openTempFile(&tmp);
    if (!fp) return 1;
    fclose(fp);

    if (WritePS(tmp, image)) return 1;
    fp = fopen(tmp, "rb");
    if (!fp) return 1;
    fclose(fp);
    
    sprintf(buffer, "gs -sDEVICE=pdfwrite -q -dNOPAUSE -sOutputFile=%s -- %s", 
            file, tmp);
    system(buffer);
    removeTempFile();
    return 0;
}
