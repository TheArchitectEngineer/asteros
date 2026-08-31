/*****************************************************************************
 *
 * psencode(mode, Width, Height, Ncol, ScanLine,
 *          xsize, ysize, xoff, yoff,
 *          ProgName, ProgVers, FileName)
 *
 * Function: Output image in PostScript format with ASCII85 and 
 *           Lempel-Ziv Welch encoding (patent expired in 2004)
 *
 * Input: mode       - mode = pstype+10*predictor+100*nocompress+1000*printdata,
 *             where   pstype = 1(landscape) | 2(eps) | 4(gray)
 *                     predictor = 0..5 (1..4 PNG predictors, 0=auto, 5=none)
 *                     printdata = 0,1,2 (0=none, 1=binary, 2=ascii)
                       nocompress = 1 for no compression at all
 *        Width      - image width
 *        Height     - image height
 *        ScanLine[] - array for scan line (1 or 3 bytes per pixel)
 *        xsize      - EPS width
 *        ysize      - EPS height
 *        xoff       - EPS horizontal offset
 *        yoff       - EPS vertical offset
 *
 *   EPS size and offset are in Postscript points. 
 *
 * Return: size of generated Postscript file
 *
 *****************************************************************************
 *
 * psencode.c is derived from work by (in chronological order)
 * Evgeni Chernyaev <chernaev@mx.ihep.su> (around 1993-1994)
 * Tim Adye <T.J.Adye@rl.ac.uk> (1994-1999 : color modes, LZW compression)
 * Péter Szabó <pts@fazekas.hu> (2001-2003 : work on sam2p, PNG predictors)
 *
 *****************************************************************************
 *
 * Authors: Tim Adye             <T.J.Adye@rl.ac.uk>
 *          Evgeni Chernyaev     <chernaev@mx.ihep.su>
 *          Péter Szabó          <pts@fazekas.hu>
 *          Jean-Pierre Demailly <demailly@fourier.ujf-grenoble.fr>
 *
 * Copyright (C) 1993, 1994 by Evgeni Chernyaev.
 * Copyright (C) 1994, 1996, 1998, 1999 by Tim Adye.
 * Copyright (C) 2001, 2002, 2003 by  Péter Szabó
 * Copyright (C) 2009 by Jean-Pierre Demailly
 * 
 * This software is published under the GPL (GNU Public Licence version 3),
 * which is compatible with earlier licenses used for this code.
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef __STDC__
#define ARGS(alist) alist
#define CONST const
#else
#define ARGS(alist) ()
#define CONST
#endif

#define MAXLINE 79

typedef unsigned char byte;
typedef unsigned int  uint;

static int Nbyte;
static int LineBreak;
static int formatting = 0;
static int printdata = 0;
static int nocompress = 0;
static int ifhex = 0;
static int bpp_in, bpp_out;
static int predictor0 = 4;

static FILE *fi;
static FILE *fo;

static void get_scan_line(int y, int Width, byte * ScanLine);

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
 * Name: ASCIIencode                               Date:    05.11.93 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised: 20.04.95 *
 *                                                                     *
 * Function: ASCII85 / ASCIIHex encode and output byte buffer          *
 *                                                                     *
 * Input: k   - number of bytes                                        *
 *        Buf - byte buffer                                            *
 *                                                                     *
 ***********************************************************************/
static void ASCIIencode(k, Buf)
                     int  k;
                     byte Buf[];
{
  unsigned int Value;
  int j, n;
  char s[8];

  if (k == 0) return;
  if (ifhex) {
    for (j=0; j<k; j++) {
      sprintf(s, "%02x", Buf[j]);
      put_string(s);
    }
    return;
  }

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
 * Name: PutCode                                     Date:    05.11.93 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised:          *
 *                                                                     *
 * Function: Put out code (LZW encoding)                               *
 *                                                                     *
 * Input: Code - code                                                  *
 *        CodeSize - codesize                                          *
 *                                                                     *
 ***********************************************************************/
static void PutCode(Code, CodeSize)
                int Code, CodeSize;
{
  static int k, PartA, PartB, SizeA, SizeB;
  static CONST int mask[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
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
      ASCIIencode(k,Accum);
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
    ASCIIencode(k, Accum);
  }
}

/***********************************************************************
 *                                                                     *
 * Name: EncodeData                                  Date:    05.11.93 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised: 21.06.94 *
 *                                                            04.06.94 *
 * Function: Lempel-Ziv Welch encoding of an image                     *
 *                                                                     *
 * Input: Width      - image width                                     *
 *        Height     - image height                                    *
 *        ScanLine[] - array for scan line (byte per pixel)            *
 *                                                                     *
 ***********************************************************************/
static void EncodeData(Width, Height, ScanLine)
                  int  Width, Height;
                  byte *ScanLine;
{
  int    k, x, y, disp, Code, K, Length;
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

  for (y=0; y<Height; y++) {
    get_scan_line(y, Width, ScanLine);
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
    get_scan_line(y, Width, ScanLine);
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

/***********************************************************************
 *                                                                     *
 * Name: psencode                                                      *
 *                                                                     *
 * Function: Output image in PostScript format with ASCII85/ASCIIHex   *
 *           and Lempel-Ziv Welch encoding.                            *
 *                                                                     *
 * See top of file for a description of this routine.                  *
 *                                                                     *
 ***********************************************************************/
long psencode(mode, Width, Height, ScanLine,
	      xsize, ysize, xoff, yoff,
	      ProgName, ProgVers, FileName)
         int  mode, Width, Height;
         int  xsize, ysize, xoff, yoff;
         CONST ScanLine[];
         CONST char *ProgName, *ProgVers, *FileName;
{
  int        ifeps, ifgray, ifrotate;
  int        EPSsizeX, EPSsizeY;
  time_t     clock;
  char       buffer[2048];
  char       rotate[80];

  /*   C H E C K   P A R A M E T E R S   */

  ifrotate   = (mode%10)&1;
  ifeps      = (mode%10)&2;
  ifgray     = (mode%10)&4;
  predictor0 = (mode/10)%10;
  nocompress = (mode/100)%10;
  if (nocompress == 1) {
    nocompress = 0;
    ifhex = 1;
  }
  printdata   = (mode/1000)%10;

  if (Width <= 0 || Height <= 0) {
    fprintf(stderr,
      "\n%s: incorrect image size: %d x %d\n", ProgName, Width, Height);
    return 0;
  }

  /*   I N I T I A L I S A T I O N   */

  if (!ifeps || xsize==0 || ysize==0) {
    EPSsizeX = Width;
    EPSsizeY = Height;
  } else {
    EPSsizeX = xsize;
    EPSsizeY = ysize;
  }
  if (ifrotate) {
     sprintf(rotate, "%d 0 translate 90 rotate ", Height);
     Nbyte = EPSsizeX;
     EPSsizeX = EPSsizeY;
     EPSsizeY = Nbyte;
  } else
     *rotate = '\0';

  Nbyte  = 0;

  /*   O U T P U T   H E A D E R   */

  sprintf(buffer,
    "%%!PS-Adobe-3.0 %s\n"
    "%%%%Title: %s\n"
    "%%%%Creator: %s-v%s\n"
    "%%%%CreationDate: %s"
    "%%%%BoundingBox: 0 0 %d %d\n"
    "%%%%DocumentData: Clean7Bit\n"
    "%%%%LanguageLevel: 2\n"
    "%%%%Pages: 1\n"
    "%%%%EndComments\n"
    "%%%%Page: 1 1\n"
    "%s %s",
    ifeps? "" : "EPSF-3.0",
    FileName, ProgName, ProgVers,
    (time(&clock),ctime(&clock)),
    EPSsizeX, EPSsizeY, 
    ifeps? "gsave " : "", rotate);
  put_string (buffer);

  if (nocompress)
    sprintf(buffer,
    "%d %d scale\n/line %d string def\n"
    "%d %d 8\n"
    "[ %d 0 0 -%d 0 %d ]\n"
    "{currentfile line readhexstring pop}\n" "%s",
    Width, Height, Width, Width, Height, Width, Height, Height,
    (ifgray)? "image\n" : "false 3 colorimage\n");
  else
    sprintf(buffer,
    "save 9 dict begin {\n"
    "/T currentfile %s filter def/%s setcolorspace /F T\n"
    "<< /BitsPerComponent 8/Columns %d/Colors %d/Predictor 10 >>\n"
    "/LZWDecode filter def << /ImageType 1"
    "/Width %d/Height %d/BitsPerComponent 8\n"
    "/ImageMatrix[1 0 0 -1 0 %d]/Decode %s/DataSource F >> image\n"
    "F closefile T closefile} exec\n",
    ifhex? "/ASCIIHexDecode" : "/ASCII85Decode",
    ifgray? "DeviceGray" : "DeviceRGB", Width, ifgray? 1 : 3,
    Width, Height, Height,
    ifgray?"[0 1]" : "[0 1 0 1 0 1]");
  put_string (buffer);

  formatting = 1;
  set_linebreak();

  if (!nocompress) {
      EncodeData(Width, Height, ScanLine);
      if (ifhex)
          put_string(">");
      else
          put_string("~>");
  } else {
      predictor0 = 5;
      WriteData(Width, Height, ScanLine);
  }

  /* add newline unless just added (so as to avoid blank lines!) */
  if (formatting != 2) put_char('\n');
  formatting = 0;

  if (!nocompress)
     put_string ("end restore\n");
  if (ifeps) 
     put_string ("showpage grestore\n");
  else
     put_string ("showpage\n");
  put_string ("%%Trailer\n%%EOF\n");
  return (Nbyte);
  fprintf(stderr, "Written %d bytes to output file %s\n", Nbyte, FileName);
}

void AdjustGrayScale(byte *Pos, int Width)
{
    int i;
    for (i=0; i<Width; i++)
       Pos[i]=(50*Pos[3*i]+32*Pos[3*i+1]+18*Pos[3*i+2])/100;
}

void
FilterLine(int predictor, int start, int Width, byte *ScanLine)
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
       fread(Pos, 1, bpp_in*Width, fi);
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
    case 5:
       /* JPD predictor */
       for (i=0; i<Length; i++) {
	  p = (int)Left[i]+(int)Up[i]-(int)Corner[i];
          if (p<0) p = 0;
          if (p>255) p = 255;
          Output[i] = (Pos[i]-p)&255;
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

static void get_scan_line(int y, int Width, byte * ScanLine)
{
    int i, predictor;
    byte *Pos;

    if (feof(fi)) fprintf(stderr, "Missing data in input file!\n");

    /* just raw data without predictor */
    if (predictor0>=5) {
       Pos = ScanLine+1;
       fread(ScanLine, 1, bpp_in*Width, fi);
       if (bpp_out<bpp_in) AdjustGrayScale(ScanLine, Width);
       return;
    }

    if (y==0) {
       /* first row; initialize pointers & use predictor=1 */
       FilterLine(1, -1, Width, ScanLine);
    } else
    if (predictor0<=0) {
       /* find best predictor (unused 5 is best !) */
       for (predictor=1; predictor<=4; predictor++)
	  FilterLine(predictor, (predictor==1), Width, ScanLine);
    } else
      FilterLine(predictor0, 1, Width, ScanLine);

    if (printdata) {
        if (printdata==1) {
           for (i=0; i<=bpp_out*Width; i++)
              printf("%c", ScanLine[i]);
	   return;
        }
        printf("row %d, predictor %d:\n", y, (int)*ScanLine);
        if (bpp_out==1) 
        for (i=1; i<=Width; i++) {
           printf("%02x", ScanLine[i]);
           if (i%36==0) 
              printf("\n");
           else
              printf(" ");
	} else
        for (i=1; i<=bpp_out*Width; i++) {
           printf("%02x", ScanLine[i]);
           if (i%30==0) 
              printf("\n");
           else
           if (i%3==0)
              printf(" ");
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    static byte * ScanLine;
    char buf[128];
    int mode, N, Width, Height;

    if (!argv[1]) {
       fprintf(stderr, "Usage: ppmtops <input.ppm> <output.ps> [<mode>]\n");
       fprintf(stderr, "where mode = pstype+10*predictor+100*nocompress+1000*printdata\n");
       fprintf(stderr, "      pstype = 1(landscape) | 2(eps) | 4(gray)\n");
       fprintf(stderr, "      predictor = 0..5 (1..4 PNG predictors, 0=auto, 5=none)\n");
       fprintf(stderr, "      printdata = 0,1,2 (0=none, 1=binary, 2=ascii)\n");
       fprintf(stderr, "Using - as input (resp. output) means stdin (resp. stdout)\n");

       return -1;
    }
    if (!argv[2]) {
       fprintf(stderr, "No output file specified !\n");
       return -1;
    }

    if (!strcmp(argv[1], "-"))
       fi = stdin;
    else
       fi = fopen(argv[1], "rb");
    if (!fi) { 
       fprintf(stderr, "Could not open file %s for reading !\n", argv[1]);
       return -1;
    }
    if (!strcmp(argv[2], "-"))
       fo = stdout;
    else
       fo = fopen(argv[2], "w");
    if (!fo) {
       fprintf(stderr, "Could not open file %s for writing !\n", argv[2]);
       return -1;
    }
    if (argv[3]) 
      mode = atoi(argv[3]);
    else
      mode = 0;
    fgets(buf, 120, fi);
    if (!strncmp(buf, "P6", 2)) {
       fprintf(stderr, 
          "Input %s seems to be PPM image of type P6 (color)\n", argv[1]);
       bpp_in = 3;
    } else
    if (!strncmp(buf, "P5", 2)) {
       fprintf(stderr, 
          "Input %s seems to be PPM image of type P5 (gray)\n", argv[1]);
       bpp_in = 1;
       mode = ((mode%10)|4)+(mode/10)*10;
    } else {
    error:
       fprintf(stderr, "PPM image %s could not be recognized.\n", argv[1]);
       fprintf(stderr, "Only types P5/P6 are recognized. Aborting...\n");
       goto terminate;
    }
    if (!fgets(buf, 120, fi)) goto error;
    sscanf(buf, "%d %d", &Width, &Height);
    if (!fgets(buf, 120, fi)) goto error;

    bpp_out = ((mode%10)&4)?1:3;
    N = 1+2*bpp_out+(4*bpp_out+(bpp_in>bpp_out))*Width;

    ScanLine = (byte *)malloc(N);
    memset(ScanLine, 0, N);
    psencode(mode, Width, Height,
             ScanLine, 596, 842, 0, 0,
	     "ppmtops", "3", argv[2]);

 terminate:
    fclose(fi);
    fclose(fo);
    free(ScanLine);
    fprintf(stderr, "Written %ld bytes to %s\n", Nbyte, argv[2]);

    return 0;
}

