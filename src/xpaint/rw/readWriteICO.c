/* ico2xpm.c -- Convert icons to pixmaps
 * Copyright (C) 1998 Philippe Martin
 * Modified by Brion Vibber
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/xpm.h>

#include "xpaint.h"
#include "image.h"
#include "libpnmrw.h"

extern void * xmalloc(size_t n);

#if defined(__alpha) || (defined(_MIPS_SZLONG) && _MIPS_SZLONG == 64)
typedef unsigned int DWord;
typedef int Long;
#else
typedef unsigned long DWord;
typedef long Long;
#endif

#define TRUE 1
#define FALSE 0

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 1024
#endif

/* The maximum number of icons we'll try to handle in any one file */
#define MAXICONS 32

struct _RGBQuad
{
  unsigned char rgbBlue;
  unsigned char rgbGreen;
  unsigned char rgbRed;
  unsigned char rgbReserved;
};
typedef struct _RGBQuad RGBQuad;

struct _ColorMap
{
  DWord nbr_colors;
  RGBQuad *color_list;
};
typedef struct _ColorMap ColorMap;

struct _DrawData
{
  unsigned char *data;
  DWord width;
  DWord height;
};
typedef struct _DrawData DrawData;

typedef void (*ColorConverter) (XpmColor *color, RGBQuad *wincolor);

/* Header of a single icon image in an icon file */
typedef struct IconDirectoryEntry {
	unsigned char  bWidth;
	unsigned char  bHeight;
	unsigned char  bColorCount;
	unsigned char  bReserved;
	unsigned short wPlanes;
	unsigned short wBitCount;
	unsigned long  dwBytesInRes;
	unsigned long  dwImageOffset;
} ICONDIRENTRY;

/* Header of an icon file */
typedef struct ICONDIR {
	 unsigned short          idReserved;
	 unsigned short          idType;
	 unsigned short          idCount;
/*    ICONDIRENTRY  idEntries[1];*/
	 ICONDIRENTRY  idEntries[MAXICONS];
} ICONHEADER;

/* Bitmap header - this is on the images themselves */
typedef struct tagBITMAPINFOHEADER{
        unsigned long      biSize;

        long       biWidth;
        long       biHeight;
        unsigned short       biPlanes;
        unsigned short       biBitCount;
        unsigned long      biCompression;
        unsigned long      biSizeImage;
        long       biXPelsPerMeter;
        long       biYPelsPerMeter;
        unsigned long      biClrUsed;
        unsigned long      biClrImportant;
} BITMAPINFOHEADER;

/* Magic number for an icon file */
/* This is the the idReserved and idType fields of the header */
static char ico_magic_number[4] = {0, 0, 1, 0};

/*
 * Conversion
 */

/* The color index of a pixel in the icon colormap */
#define PIXEL_INDEX(x,y) \
   (color_level == 4 ? \
     ((x) % 2 == 0 ? \
      (image [bytes_per_line * (height - 1 - (y)) / 2 + (x) / 2] & 0xF0) >> 4 \
	 : image [bytes_per_line * (height - 1 - (y)) / 2 + (x) / 2] & 0x0F) \
      : image [bytes_per_line * (height - 1 - (y)) + (x)])
     
/* The color index of a pixel in the (reduced) pixmap colormap */
#define PIXEL_REDUCED_INDEX(x,y) \
     (reduced_colormap_index [PIXEL_INDEX((x),(y))])
  
/* The transparency of a pixel */
#define MASK_BYTE(x,y) (bytes_per_mask_line * (height - 1 - (y)) + (x) / 8)
#define MASK_BIT(x,y) (7 - (x) % 8)
#define IS_TRANSPARENT(x,y) \
     (mask[MASK_BYTE((x),(y))] & (1 << MASK_BIT((x),(y))))
  
Image *
ReadICO (char *file)
{
   Image *xpaint_image = NULL;

   FILE *ico_stream;
/* Values read from icon file */
   ICONHEADER iconheader;
   BITMAPINFOHEADER bitmapinfoheader;
/*   unsigned char magic_number [sizeof(ico_magic_number)];*/
   unsigned char *magic_number;
   unsigned char color_level;
   unsigned char colormap [256 * 4];
   unsigned char image [256 * 256]; 
   unsigned char mask [256 * 32];

/* Different variables computed from the read ones */
   int whichimage = 0;
   int ncolors;
   int width = 0;
   int height = 0;
   unsigned char bytes_per_line;
   unsigned char bytes_per_pixel;
   unsigned char bytes_per_mask_line;
   unsigned char *dp, *mp = NULL;
   int image_length;
   int mask_length;
   int i, x, y;

/* Variables for the reduced colormap */
   int *reduced_colormap_index = NULL;
   int *color_used = NULL;
   int nb_color_used;

/* Does the icon have transparent pixels ? */
   int have_transparent_pixels;

  /* 
   * Read the icon file 
   */
  
  if ((ico_stream = fopen (file, "r")) == NULL)
      return NULL;

  /* Check the magic number & header */
  /*fread (magic_number, 1, sizeof(ico_magic_number), ico_stream);*/
  fread (&iconheader, 1, 6 /*sizeof(unsigned short)*3*/, ico_stream);
  magic_number = (unsigned char *)&iconheader;

  if (memcmp(magic_number, ico_magic_number, sizeof(ico_magic_number))) {
       fprintf (stderr, "ReadICO: %s: %s\n",
		   file, "Not a recognized icon file !!");
       fclose (ico_stream);
       return NULL;
  }
  
  /* Read in the rest of the icon directory entries */
  fread(&iconheader.idEntries[0],iconheader.idCount,
    /*sizeof(ICONDIRENTRY)*/ 16, ico_stream);
  
  /* Cycle through each icon image 
     and take largest width+height */
  for(whichimage = 0; whichimage < iconheader.idCount; whichimage++) {

    /* For debugging
      fprintf(stdout, "(%d) identry: %d %d %d %d %d %d %d %d\n",
        whichimage,
        (int)iconheader.idEntries[whichimage].bWidth,
        (int)iconheader.idEntries[whichimage].bHeight,
        (int)iconheader.idEntries[whichimage].bColorCount,
        (int)iconheader.idEntries[whichimage].bReserved,
        (int)iconheader.idEntries[whichimage].wPlanes,
        (int)iconheader.idEntries[whichimage].wBitCount,
        (int)iconheader.idEntries[whichimage].dwBytesInRes,
        (int)iconheader.idEntries[whichimage].dwImageOffset);
    */
    
    /* Read the icon size */
    if (iconheader.idEntries[whichimage].bWidth +
        iconheader.idEntries[whichimage].bWidth <=
        width + height) continue;
    width = iconheader.idEntries[whichimage].bWidth;
    height = iconheader.idEntries[whichimage].bHeight;

    /* Let's surf on over to the bitmap image & read the BMIH. */
    fseek (ico_stream, iconheader.idEntries[whichimage].dwImageOffset,SEEK_SET);
    fread (&bitmapinfoheader, 1, sizeof(bitmapinfoheader), ico_stream);

    /* Read the number of colors.
     * TODO: add support for monochrome, 24-bit icons
     */
    /*
        fprintf(stderr,
          "ReadICO: %s:\n   Number of colors in image "
          "(%d planes, %d bpp)\n",
          file,
          (int)bitmapinfoheader.biPlanes,
          (int)bitmapinfoheader.biBitCount);
        fprintf(stderr, 
          "width = %d, height = %d (bpl = %d)\n", width, height); 
    */

    color_level = bitmapinfoheader.biPlanes * bitmapinfoheader.biBitCount ;
    if (color_level&3 || color_level<4 || color_level>32 || 
        (color_level>8 && color_level&7)) {
        fprintf(stderr,
          "ReadICO: %s:\n   Unsupported number of colors in image! "
          "Skipping. (%d planes, %d bpp)\n",
          file,
          (int)bitmapinfoheader.biPlanes,
          (int)bitmapinfoheader.biBitCount);
        return NULL;
    }
    
    /* Determine the number of bytes defining a line of the icon. */
    if (color_level>=16) {
       bytes_per_pixel = color_level/8;
       bytes_per_line =  4 * ((bytes_per_pixel * width * 8 + 31) / 32);
       bytes_per_mask_line = 4 * ((width + 31) / 32);
    } else {
       bytes_per_line =  4 * ((width * 8 + 31) / 32);
       bytes_per_pixel = 1;
       bytes_per_mask_line = 4 * ((width + 31) / 32);
    }
    /*
    if (color_level==TRUE_COLOR) {
       bytes_per_mask_line = 0;
    } else {
       bytes_per_line =  4 * ((width + 3) / 4);
       bytes_per_mask_line = 4 * ((width + 31) / 32);
    }
    */

    /* Read the colormap */
    if (bitmapinfoheader.biClrImportant != 0)
      ncolors = bitmapinfoheader.biClrImportant;
    else if (bitmapinfoheader.biClrUsed != 0)
      ncolors = bitmapinfoheader.biClrUsed;
    else
      ncolors = 1 << (bitmapinfoheader.biPlanes*bitmapinfoheader.biBitCount);

    if (ncolors>256) ncolors = 256;
    if (color_level>=16) ncolors = 0;

    fseek (ico_stream, iconheader.idEntries[whichimage].dwImageOffset
      + bitmapinfoheader.biSize, SEEK_SET);
    fread (colormap, 1, ncolors * 4, ico_stream);

    /* Read the image */
    if (color_level == 4)
        image_length = bytes_per_line * height / 2;
    else
        image_length = bytes_per_line * height;

    fread (image, 1, image_length, ico_stream);

    /* Read the mask */
    mask_length = bytes_per_mask_line * height;
    fread (mask, 1, mask_length, ico_stream);

    /* Read error / prematured EOF  occured ? */

    if (feof (ico_stream))
      fprintf (stderr, "ReadICO: %s: Premature end of file.\n", file);
    if (ferror (ico_stream))
      fprintf (stderr, "ReadICO: %s: Read error.\n", file);
    if (feof (ico_stream) || ferror (ico_stream))
      {
        fclose (ico_stream);
        return NULL;
      }

    /* Reduce the number of colors */
    if (ncolors) {
        color_used = (int *)xmalloc(ncolors*sizeof(int));
        reduced_colormap_index = (int *)xmalloc(ncolors*sizeof(int));
    }

    for (i = 0 ; i < ncolors ; i++)
      color_used [i] = 0;

    if (ncolors)  
    for (y = 0 ; y <height ; y++)
      for (x = 0 ; x < width ; x++)
        color_used [PIXEL_INDEX(x,y)] = 1;

    nb_color_used = 0;
    for (i = 0 ; i < ncolors ; i++)
      if (color_used [i])
        reduced_colormap_index[i] = nb_color_used ++;

    /* Check for at least one transparent pixel */
    have_transparent_pixels = 0;
    for (y=0; y<height && have_transparent_pixels == 0 ; y++)
      for (x=0; x<width; x++)
        if (IS_TRANSPARENT(x,y)) {
	    have_transparent_pixels = 1;
	    break;
	}
  
    /*
     * Write the image
     */

    if (xpaint_image) ImageDelete(xpaint_image);
    xpaint_image = ImageNew(width, height);
    dp = xpaint_image->data;

    if (have_transparent_pixels) {
	ImageMakeMask(xpaint_image);
	mp = xpaint_image->alpha;
    }
    /* Write the image */
    for (y=0; y<height; y++)
      {
        for (x=0; x<width; x++)
	  {
	    if (IS_TRANSPARENT(x,y)) {
	        *dp++ = 0;
	        *dp++ = 0;
	        *dp++ = 0;
		*mp = True;
	    } else {
	      if (color_level>=16) {
                /* Fix it : this should not work for color_level == 16 ... */
                i = bytes_per_line * (height-1-y) + bytes_per_pixel * x + 2;
                *dp++ = image[i--];
                *dp++ = image[i--];
                *dp++ = image[i];
	      } else {
	        i = 4 * PIXEL_INDEX(x,y);
	        *dp++ = colormap[i--];
	        *dp++ = colormap[i--];
	        *dp++ = colormap[i];
	      }
	    }
	    mp++;
	  }
      }
  }
  fclose (ico_stream);
  free(color_used);
  return xpaint_image;
}

void
write_byte (FILE *fd, unsigned char c)
{
  fputc(c, fd);
}

void
write_word (FILE *fd, short word)
{
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
   fwrite (&word, sizeof (short), 1, fd);
#else
  int i = 0;

  while (i < sizeof (short)) {
    fputc (word & 0xff, fd);
    word >>= 8;
    i++;
  }
#endif
}

void
write_dword(FILE *fd, DWord dword)
{
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
  fwrite (&dword, sizeof (DWord), 1, fd);
#else
  int i = 0;

  while (i < sizeof (DWord)) {
    fputc (dword & 0xff, fd);
    dword >>= 8;
    i++;
  }
#endif
}

void
write_long(FILE *fd, Long dword)
{
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
  fwrite (&dword, sizeof (Long), 1, fd);
#else
  int i = 0;

  while (i < sizeof (Long)) {
    fputc (dword & 0xff, fd);
    dword >>= 8;
    i++;
  }
#endif
}

static RGBQuad s_color;

RGBQuad
convert_color (XpmColor *xpm_color)
{
  unsigned int r, g, b;
  if (xpm_color->c_color && xpm_color->c_color[0] == '#') {
     sscanf(xpm_color->c_color+1, "%4x%4x%4x", &r, &g, &b);
     s_color.rgbRed = r>>8;
     s_color.rgbGreen = g>>8;
     s_color.rgbBlue = b>>8;
     s_color.rgbReserved = 0;   
  } else {
     s_color.rgbRed = 0;
     s_color.rgbGreen = 0;
     s_color.rgbBlue = 0;
     s_color.rgbReserved = 0;
  }
  return s_color;
}

static int
white_present (ColorMap *color_map)
{
  RGBQuad *color_quad;
  int ccount, found;

  ccount = 0;
  found = 0;

  while (!found && ccount < color_map->nbr_colors)
    {
      color_quad = color_map->color_list + ccount;
      if ((color_quad->rgbRed == 255)
	  && (color_quad->rgbGreen == 255)
	  && (color_quad->rgbBlue == 255))
	found = !found;
      else
	ccount++;
    }

  return found;
}

void
add_white (ColorMap *color_map, unsigned int clr_count)
{
  RGBQuad *white;

  white = color_map->color_list + clr_count;
  white->rgbRed = 255;
  white->rgbGreen = 255;
  white->rgbBlue = 255;
  white->rgbReserved = 0;
}

static ColorMap *
convert_color_map (XpmImage *icon, DWord ncolors)
{
  unsigned int clr_count;
  ColorMap *color_map;

  if (ncolors <= 256)
    {
      color_map = xmalloc (sizeof (ColorMap));
      color_map->nbr_colors = ncolors;
      color_map->color_list = xmalloc (sizeof (RGBQuad) * ncolors);
      memset (color_map->color_list, 0, sizeof (RGBQuad) * ncolors);

      for (clr_count = 0; clr_count < icon->ncolors; clr_count++)
	*(color_map->color_list + clr_count) =
	  convert_color (icon->colorTable + clr_count);

      if (!white_present (color_map))
	{
	  if (clr_count < ncolors)
	    {
	      add_white (color_map, clr_count);
	      clr_count++;
	    }
	  else
	    fprintf (stderr,
		     "Could not add a white color to the colormap, icon may"
		     " not be displayed properly.\n");
	}
    }
  else
    {
      color_map = NULL;
    }

  return color_map;
}

static void
put_24bit_color (DrawData *draw_data,
		 short x, short y,
		 RGBQuad *color)
{
  unsigned char *ptr;

  ptr = draw_data->data
    + ((draw_data->height - y - 1)
       * draw_data->width
       + x) * 3;

  *ptr = color->rgbBlue;
  *(ptr + 1) = color->rgbGreen;
  *(ptr + 2) = color->rgbRed;
}

void
put_8bit_color (DrawData *draw_data,
		short x, short y,
		unsigned char val)
{
  unsigned char *ptr;

  ptr = draw_data->data
    + (draw_data->height - y - 1)
    * draw_data->width
    + x;

  *ptr = val;
}

static unsigned char *
make_xor_mask (XpmImage *icon, DWord ncolors)
{
  DWord mask_size, x, y;
  DWord *uint_ptr;
  short bypp;
  RGBQuad color; /* for bpp == 24 */
  DrawData *draw_data;
  unsigned char *mask;

  bypp = (ncolors <= 256)? 1 : 3;

  mask_size = (DWord) icon->width * (DWord) icon->height * bypp;

  draw_data = xmalloc (sizeof (DrawData));
  draw_data->data = xmalloc (mask_size);
  draw_data->width = (DWord) icon->width;
  draw_data->height = (DWord) icon->height;

  uint_ptr = (DWord *) icon->data;

  for (y = 0; y < icon->height; y++)
    for (x = 0; x < icon->width; x++)
      {
	if (bypp == 1)
	  put_8bit_color (draw_data, x, y, *uint_ptr);
	else
	  {
	    color = convert_color (icon->colorTable + *uint_ptr);
	    put_24bit_color (draw_data, x, y, &color);
	  }

	uint_ptr++;
      }

  mask = draw_data->data;
  free (draw_data);

  return mask;
}

static unsigned char *
make_and_mask (XpmImage *icon, ColorMap *color_map, DWord ncolors)
{
  DWord width, line_size, mask_size, x;
  DWord *uint_ptr;
  RGBQuad color; /* for bpp >= 16 */
  unsigned char *mask, *line;

  width = icon->width;
  line_size = ((width + 31) / 32) * 4;
  mask_size = icon->height * line_size;

  mask = xmalloc(mask_size * sizeof(unsigned char));
  bzero(mask, mask_size * sizeof(unsigned char));
  uint_ptr = (DWord *) icon->data;

  for (line = mask + mask_size - line_size; line >= mask; line -= line_size)
    for (x = 0; x < width; x++)
      {
	if (ncolors <= 256)
	  color = *(color_map->color_list + *uint_ptr);
	else
	  color = convert_color (icon->colorTable + *uint_ptr);

	if (color.rgbReserved)
	  line[x / 8] |= 128 >> (x % 8);

	uint_ptr++;
      }

  return mask;
}

int
WriteICO(char *file, Image * image)
{
    FILE *fd;
    unsigned char *xor_mask, *and_mask;
    int count, bypp;
    DWord dwBytesInRes, ncolors, xor_size, and_size, total_size;
    XpmAttributes attr;
    XpmImage icon;
    ColorMap *color_map;
    RGBQuad *rgb_quad;

    if (image->width > 256 || image->height > 256)
        return TRUE;
    if ((fd = fopen(file, "w")) == NULL)
	return TRUE;

    attr.valuemask = XpmColormap;
    attr.colormap = (Colormap) image->sourceColormap;

    if (XpmCreateXpmImageFromPixmap(Global.display, 
	image->sourcePixmap, image->sourceMask, &icon, &attr) != XpmSuccess)
        return TRUE;

    if (icon.ncolors <= 256) 
        ncolors = 256;
    else
        ncolors = 65536;
    bypp = (ncolors <= 256)? 1 : 3;

    color_map = convert_color_map (&icon, ncolors);

    xor_mask = make_xor_mask(&icon, ncolors);
    and_mask = make_and_mask(&icon, color_map, ncolors);
    xor_size = image->width * image->height * bypp;
    and_size = ((image->width+31)/32) * 4 * image->height;
    total_size = xor_size + and_size;

    /* write header */
    write_word(fd, 0);
    write_word(fd, 1);
    write_word(fd, 1);

    /* write directory entry */
    write_byte (fd, image->width);
    write_byte (fd, image->height);
    write_byte (fd, 0); /* bColorCount */
    write_byte (fd, 0); /* bReserved */
    write_word (fd, 0); /* wPlanes */
    write_word (fd, 0); /* wBitCount */
    if (ncolors == 256)
        dwBytesInRes = 40 + ncolors * 4 + total_size;
    else
        dwBytesInRes = 40 + total_size;
    write_dword (fd, dwBytesInRes);
    write_dword (fd, 22); /* dwImageOffset */

    /* write bitmap info header */
    write_dword (fd, 40);
    write_long (fd, image->width);
    write_long (fd, image->height * 2);
    write_word (fd, 1);
    write_word (fd, 8 * bypp);
    write_dword (fd, 0);
    write_dword (fd, total_size);
    write_long (fd, 0);
    write_long (fd, 0);
    write_dword (fd, (ncolors==256)? 256 : 0);
    write_dword (fd, 0);

    /* write colormap */
    rgb_quad = color_map->color_list;
    for (count = 0; count < color_map->nbr_colors; count++) {
        write_byte (fd, rgb_quad->rgbBlue);
        write_byte (fd, rgb_quad->rgbGreen);
        write_byte (fd, rgb_quad->rgbRed);
        write_byte (fd, 0);
        rgb_quad++;
    }

    fwrite (xor_mask, 1, xor_size, fd);
    fwrite (and_mask, 1, and_size, fd);
    
    XpmFreeXpmImage(&icon);
    free(color_map->color_list);
    free(color_map);
    free(and_mask);
    free(xor_mask);
    fclose(fd);
    return FALSE;
}

int 
TestICO(char *file)
{
   FILE *ico_stream;
/* Values read from icon file */
   ICONHEADER iconheader;
   unsigned char *magic_number;

   if ((ico_stream = fopen (file, "r")) == NULL)
      return FALSE;

   fread (&iconheader, 1, 6/*sizeof(unsigned short)*3*/, ico_stream);
   magic_number = (unsigned char *)&iconheader;

   if (memcmp(magic_number, ico_magic_number, sizeof(ico_magic_number))) {
       fclose (ico_stream);
       return FALSE;
   }
   return TRUE;
}

