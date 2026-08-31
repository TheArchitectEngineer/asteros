/* +-------------------------------------------------------------------+ */
/* | Copyright 1996-2003, Greg Roelofs (newt@pobox.com)                | */
/* | Last revised:  1 June 2003                                        | */
/* +-------------------------------------------------------------------+ */

/* maintainer can nuke this line if not using CVS: */
/* $Id: readWritePNG.c,v 1.17 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

typedef char *String;
#include <messages.h>
#include <image.h>
#include "rwTable.h"

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include "xpaint.h"

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

#ifndef Trace
#  ifdef XPAINT_PNG_DEBUG
#    define Trace(x)   fprintf x ; fflush(stderr)
#  else
#    define Trace(x)
#  endif
#endif

extern int file_transparent;

extern void *xmalloc(size_t n);

typedef struct _jmpbuf_wrapper {
  jmp_buf jmpbuf;
} jmpbuf_wrapper;

extern void compressColormap(Image *image);
static void xpaint_png_error_handler(png_structp png_ptr, png_const_charp msg);

/* this can be shared between reading/writing code since they never overlap: */
static jmpbuf_wrapper xpaint_jmpbuf_struct;


int
TestPNG(char *file)  /* gets called a LOT on the first image:  brushes? */
{
    char header[8];
    FILE *fp = fopen(file, "rb");   /* libpng requires ANSI; so do we */

    if (!fp)
        return 0;

    fread(header, 1, 8, fp);
    fclose(fp);

    return !png_sig_cmp( (unsigned char*) header, 0, 8);
}



Image *
ReadPNG(char *file)
{
    FILE          *fp;
    png_structp   png_ptr;
    png_infop     info_ptr;
    png_uint_32   width, height;
    png_colorp    palette;
    png_bytep     trans = NULL;
    png_color_16p palette16;
    int           num_trans;
    int           bit_depth, color_type, interlace_type, num_palette, rowbytes;
    int           i, hasAlpha;
    int           level /* , npasses */ ;
    Image         *image = NULL;


    Trace((stderr, "\nGRR ReadPNG:  reading file %s\n", file));
    if ((fp = fopen(file, "rb")) == (FILE *)NULL) {
        RWSetMsg("Error opening input file");
        return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
      &xpaint_jmpbuf_struct, xpaint_png_error_handler, NULL);
    if (!png_ptr) {
        RWSetMsg("Error allocating PNG png_ptr memory");
        fclose(fp);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        RWSetMsg("Error allocating PNG info_ptr memory");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(xpaint_jmpbuf_struct.jmpbuf)) {
        RWSetMsg(msgText[FATAL_LIBPNG_ERROR_LONGJMP_CALLED_WHILE_READING]);
        fprintf(stderr, "%s\n",
          msgText[FATAL_LIBPNG_ERROR_LONGJMP_CALLED_WHILE_READING]);
        fflush(stderr);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);  /* read all PNG info up to image data */

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
      &interlace_type, NULL, NULL);
    Trace((stderr, "GRR ReadPNG:  width = %d, height = %d\n", width, height));

    hasAlpha = FALSE;

    switch (color_type) {

        case PNG_COLOR_TYPE_PALETTE:
            Trace((stderr, "GRR ReadPNG:  PNG_COLOR_TYPE_PALETTE\n"));
	    if (!png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette)) {
                RWSetMsg(msgText[ERROR_PLTE_CHUNK_NOT_FOUND_IN_PALETTE_IMAGE]);
                fprintf(stderr, "%s\n",
                  msgText[ERROR_PLTE_CHUNK_NOT_FOUND_IN_PALETTE_IMAGE]);
                fflush(stderr);
                png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                fclose(fp);
                return NULL;
            }
            image = ImageNewCmap(width, height, num_palette);
            png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &palette16);
            if (num_trans && trans) {
                file_transparent = 1;
                ImageMakeMask(image);   /* alpha is 1 byte deep */
	    }
            for (i = 0;  i < num_palette;  ++i)
                ImageSetCmap(image, i, palette[i].red, palette[i].green,
                palette[i].blue);

            /* GRR:  still need to get image data into `image' */
            break;

        case PNG_COLOR_TYPE_RGB:
            Trace((stderr, "GRR ReadPNG:  PNG_COLOR_TYPE_RGB\n"));
            if (bit_depth == 16) {
                RWSetMsg(msgText[STRIPPING_48_BIT_RGB_IMAGE_TO_24_BITS]);
                fprintf(stderr, "%s\n",
                  msgText[STRIPPING_48_BIT_RGB_IMAGE_TO_24_BITS]);
                fflush(stderr);
                png_set_strip_16(png_ptr);
            }
            image = ImageNew(width, height);
            break;

        case PNG_COLOR_TYPE_GRAY:   /* treat grayscale as special colormap */
            Trace((stderr, "GRR ReadPNG:  PNG_COLOR_TYPE_GRAY\n"));
            if (bit_depth == 16) {
                RWSetMsg(msgText[STRIPPING_16_BIT_GRAYSCALE_IMAGE_TO_8_BITS]);
                fprintf(stderr, "%s\n",
                  msgText[STRIPPING_16_BIT_GRAYSCALE_IMAGE_TO_8_BITS]);
                fflush(stderr);
                png_set_strip_16(png_ptr);
                bit_depth = 8;
            }
            image = ImageNewCmap(width, height, 1 << bit_depth);
            switch (bit_depth) {
                case 1:
                    image->isBW = TRUE;
                    ImageSetCmap(image, 0,   0,   0,   0);
                    ImageSetCmap(image, 1, 255, 255, 255);
                    break;
                case 2:
                    image->isGrey = TRUE;
                    ImageSetCmap(image, 0,   0,   0,   0);
                    ImageSetCmap(image, 1,  85,  85,  85);  /* 255/3 */
                    ImageSetCmap(image, 2, 170, 170, 170);
                    ImageSetCmap(image, 3, 255, 255, 255);
                    break;
                case 4:
                    image->isGrey = TRUE;
                    for (i = 0;  i < 16;  ++i) {
                        level = i * 17;  /* 255/15 */
                        ImageSetCmap(image, i, level, level, level);
                    }
                    break;
                case 8:
                    image->isGrey = TRUE;
                    for (i = 0;  i < 256;  ++i)
                        ImageSetCmap(image, i, i, i, i);
                    break;
            }
            break;

        case PNG_COLOR_TYPE_RGB_ALPHA:
            Trace((stderr, "GRR ReadPNG:  PNG_COLOR_TYPE_RGB_ALPHA\n"));
            if (bit_depth == 16) {
                RWSetMsg(msgText[STRIPPING_64_BIT_RGBA_IMAGE_TO_32_BITS]);
                fprintf(stderr, "%s\n",
                  msgText[STRIPPING_64_BIT_RGBA_IMAGE_TO_32_BITS]);
                fflush(stderr);
                png_set_strip_16(png_ptr);
            }
            /* need to split alpha and RGB/gray the hard way, sigh */
            hasAlpha = TRUE;
            file_transparent = 1;
            image = ImageNew(width, height);
            ImageMakeMask(image);   /* alpha is 1 byte deep */
            break;

        case PNG_COLOR_TYPE_GRAY_ALPHA:
            Trace((stderr, "GRR ReadPNG:  PNG_COLOR_TYPE_GRAY_ALPHA\n"));
            if (bit_depth == 16) {
                RWSetMsg(msgText[STRIPPING_32_BIT_GRAY_ALPHA_IMAGE_TO_16_BITS]);
                fprintf(stderr, "%s\n",
                  msgText[STRIPPING_32_BIT_GRAY_ALPHA_IMAGE_TO_16_BITS]);
                fflush(stderr);
                png_set_strip_16(png_ptr);
            }
            hasAlpha = TRUE;
            file_transparent = 1;
            image = ImageNewGrey(width, height);
            ImageMakeMask(image);   /* alpha is 1 byte deep */
            break;

        default:
            fprintf(stderr, "ReadPNG:  %s (%d)\n", 
                msgText[UNKNOWN_PNG_IMAGE_TYPE], color_type);
            fflush(stderr);
            RWSetMsg(msgText[UNKNOWN_PNG_IMAGE_TYPE]);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return NULL;
    }

    if (bit_depth < 8)
        png_set_packing(png_ptr);

    if (interlace_type)
        /* npasses = */ png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    if (hasAlpha) {
        png_bytep *row_pointers, png_data;

        row_pointers = (png_bytep *)xmalloc(height * sizeof(png_bytep));
        if (!row_pointers) {
            RWSetMsg(msgText[READPNG_CANT_ALLOCATE_MEMORY_FOR_ROW_POINTERS_1]);
            fprintf(stderr, "%s\n",
              msgText[READPNG_CANT_ALLOCATE_MEMORY_FOR_ROW_POINTERS_1]);
            fflush(stderr);
            ImageDelete(image);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return NULL;
        }

        /* very inefficient (memory-wise) to allocate entire image again, but
         * no easy way around it:  libpng returns the image and alpha channel
         * interspersed, and interlaced alpha images just make matters worse
         */
        png_data = (png_bytep)xmalloc(height*rowbytes);
        if (!png_data) {
            RWSetMsg(
              msgText[UNABLE_TO_ALLOCATE_TEMPORARY_STORAGE_FOR_ALPHA_IMAGE]);
            fprintf(stderr, "%s", msgText[READPNG_UNABLE_TO_MALLOC_PNG_DATA]);
            fflush(stderr);
            free(row_pointers);
            ImageDelete(image);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return NULL;
        }

        /* only bit depths of 8 and 16 support alpha channels */
        for (i = 0;  i < height;  ++i)
            row_pointers[i] = (png_bytep)png_data + i*rowbytes;

        png_read_image(png_ptr, row_pointers);

        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
            png_bytep png=png_data, rgb=image->data, alpha=image->alpha;
            for (i = 0;  i < height*width;  ++i) {
                *rgb++   = *png++;
                *rgb++   = *png++;
                *rgb++   = *png++;
                *alpha++ = *png++;
            }
        } else {  /* PNG_COLOR_TYPE_GRAY_ALPHA */
            png_bytep png=png_data, gray=image->data, alpha=image->alpha;

            for (i = 0;  i < height*width;  ++i) {
                *gray++  = *png++;
                *alpha++ = *png++;
            }
        }
        free(png_data);   /* whew */
        free(row_pointers);

    } else {  /* no alpha channel, except perhaps from palette */
        png_bytep *row_pointers;

        row_pointers = (png_bytep *)xmalloc(height * sizeof(png_bytep));
        if (!row_pointers) {
            RWSetMsg(msgText[READPNG_CANT_ALLOCATE_MEMORY_FOR_ROW_POINTERS_2]);
            fprintf(stderr, "%s\n",
              msgText[READPNG_CANT_ALLOCATE_MEMORY_FOR_ROW_POINTERS_2]);
            fflush(stderr);
            ImageDelete(image);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return NULL;
        }

        for (i = 0;  i < height;  ++i)
            row_pointers[i] = (png_bytep)image->data + i*rowbytes;

        png_read_image(png_ptr, row_pointers);

        if (image->alpha) {
	    if (trans) {
                for (i = 0;  i < width*height;  ++i)
	            image->alpha[i] = trans[image->data[i]];
            } else {
	        free(image->alpha);
                image->alpha = NULL;
	    }
	}
        free(row_pointers);

    } /* end if (hasAlpha) */

    /* GRR:  ideally should read all other info (text chunks, etc.) and save
     *       whatever ones make sense for the possible output file...but we
     *       don't, both because we're too darned lazy to bother and because
     *       XPaint is too simple-minded to be able to use it
     */

    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return image;

} /* end function ReadPNG() */



/* GRR 20010720:  Note that image gets discarded immediately after we return
 *   [unless somebody, presumably us, boosts the refCount to save it?  --see
 *   stdSaveCommonCallback() in fileName.c], so what we do to it here matters
 *   only to us. */

static int
WritePNG(char *file, Image *image, int interlace_type)
{
    int           i, j;
    int           bit_depth=0, color_type=0, num_palette, rowbytes;
    png_colorp    palette;
    png_structp   png_ptr;
    png_infop     info_ptr;
    png_bytep     *row_pointers;
    FILE          *fp;


    Trace((stderr, "\nGRR WritePNG:  %s\n", file));
    Trace((stderr, "GRR WritePNG:  %d x %d, scale = %d\n",
      image->width, image->height, image->scale));

    if (!(fp = fopen(file, "wb")))
        return 1;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
      &xpaint_jmpbuf_struct, xpaint_png_error_handler, NULL);
    if (!png_ptr) {
        fclose(fp);
        return 1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return 1;
    }

    if (setjmp(xpaint_jmpbuf_struct.jmpbuf)) {
        RWSetMsg(msgText[FATAL_LIBPNG_ERROR_LONGJMP_CALLED_WHILE_WRITING]);
        fprintf(stderr, "%s\n",
          msgText[FATAL_LIBPNG_ERROR_LONGJMP_CALLED_WHILE_WRITING]);
        fflush(stderr);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 1;
    }

    png_init_io(png_ptr, fp);

    color_type = -1;  /* quiet compiler warning (all cases actually covered) */
    bit_depth = -1;

    if (image->isBW) {
        if (image->alpha) {
            color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
            bit_depth = 8;   /* promote to full grayscale */
        } else {
            color_type = PNG_COLOR_TYPE_GRAY;
            bit_depth = 1;
        }
        Trace((stderr, "GRR WritePNG:  B/W, bit_depth = %d\n", bit_depth));

    } else if (image->isGrey) {
        color_type = image->alpha? PNG_COLOR_TYPE_GRAY_ALPHA :
                                      PNG_COLOR_TYPE_GRAY;
        if (image->cmapPacked)
            bit_depth = 8;
        else {
            Trace((stderr, "GRR WritePNG:  isGrey, cmapSize = %d and scale = "
              "%d (before compressing),\n", image->cmapSize, image->scale));
            compressColormap(image);   /* GRR:  destructive?? (seems OK...) */
            Trace((stderr, "               %s = %d and scale = %d (after "
              "compressing)\n", image->isGrey? "isGrey, cmapSize" :
              "NOT isGrey, cmap", image->cmapSize, image->scale));
            if (image->cmapSize > 16)
                bit_depth = 8;
            else if (image->cmapSize > 4)
                bit_depth = 4;
            else if (image->cmapSize > 2)
                bit_depth = 2;
            else
                bit_depth = 1;
            Trace((stderr, "GRR WritePNG:  [was] isGrey; picked bit_depth = "
              "%d\n", bit_depth));
        }

    } else if (image->scale == 3) {
        Image *cmapImage = NULL;

        /* try compressing image to palette mode, but don't force if too big */
        if (!image->alpha)   /* can't store alpha mask with palette image */
            cmapImage = ImageCompress(image, 256, 1);

        if (cmapImage) {
            image = cmapImage;  /* original was deleted in ImageCompress() */
        } else {
            color_type = image->alpha? PNG_COLOR_TYPE_RGB_ALPHA :
                                          PNG_COLOR_TYPE_RGB;
            bit_depth = 8;
            Trace((stderr, "GRR WritePNG:  RGB, bit_depth = 8\n"));
        }
    }

    /* either we're on an 8-bit or smaller display, or image->scale was 3 and
     * ImageCompress() worked
     */
    if (image->scale == 1) {
        int must_remap = FALSE;
        unsigned char *p, *q;

        Trace((stderr, "GRR WritePNG:  palette..."));
        if (!image->cmapPacked)
            compressColormap(image);   /* apparently not destructive... */

        /* first guess = gray - will be changed just afterwards if non gray */
        if (image->alpha)
	    color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
        else
            color_type = PNG_COLOR_TYPE_GRAY;

        p = image->cmapData;   /* 1D array of RGB triplets */
        for (i = image->cmapSize;  i > 0;  --i, p += 3) {
              /*   red != green ||   red != blue */
            if (p[0] != p[1]  ||  p[0] != p[2]) {
                color_type = PNG_COLOR_TYPE_PALETTE;  /* not gray, alas */
                break;
            }
            /* red = green = blue, but if != index, will have to remap */
            if (p[0] != i)
                must_remap = TRUE;
        }

        if (image->cmapSize > 16) {
            bit_depth = 8;
            /* if palette has more than 16 entries, all of which are gray, it's
             * more efficient to omit palette entirely and write a grayscale
             * PNG */

            if (color_type == PNG_COLOR_TYPE_GRAY ||
                color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                Trace((stderr, "no, grayscale..."));
                if (must_remap) {
                    /* reset all pixels to [red] palette value indexed by
                     * current pixel value (now know red == green == blue) */
                    Trace((stderr, "remapping..."));
                    p = image->cmapData;  /* 1D array of RGB (ggg) triplets */
                    q = image->data;      /* 1D array of indices into palette */
                    for (i = image->width * image->height;  i > 0;  --i, ++q) {
                        *q = p[3*(*q)];
                    }
                }
            }
        } else if (image->cmapSize > 4)
            bit_depth = 4;
        else if (image->cmapSize > 2)
            bit_depth = 2;
        else
            bit_depth = 1;

        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            palette = (png_colorp)image->cmapData;   /* GRR:  seems to work */
            num_palette = image->cmapSize;
#if 0
            for (i = 0;  i < num_palette;  ++i) {
                palette[i].red = image->cmapData ...
                palette[i].green = 
                palette[i].blue = 
            }
#endif
            png_set_PLTE(png_ptr, info_ptr, palette, num_palette);
            Trace((stderr, "bit_depth = %d, num_palette = %d\n", bit_depth,
              num_palette));
        } else /* (color_type == PNG_COLOR_TYPE_GRAY) */ {
            Trace((stderr, "bit_depth = %d\n", bit_depth));
            /* recall that image gets discarded immediately after return, so
             * this won't hurt anybody else */
            image->isGrey = TRUE;
        }
    }

    Trace((stderr, "GRR WritePNG:  isGrey = %s, scale = %d, width = %d, height"
      " = %d\n", image->isGrey? "TRUE":"FALSE", image->scale, image->width,
      image->height));

    png_set_IHDR(png_ptr, info_ptr, image->width, image->height, bit_depth,
      color_type, interlace_type, PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT);

#if 0	/* if we don't know what the display-system gamma is, don't guess */
    if (xpaint_foo->gamma > 0.0)
        png_set_gAMA(png_ptr, info_ptr, xpaint_foo->gamma);
#endif

    /* store the modification time, at least */
    {
        png_time  modtime;

        png_convert_from_time_t(&modtime, time(NULL));
        png_set_tIME(png_ptr, info_ptr, &modtime);
    }

    /* only one text comment:  Software */
    {
        png_text textdata;
        char software_text[40];

        sprintf(software_text, "XPaint %s", XPAINT_VERSION);
        textdata.compression = PNG_TEXT_COMPRESSION_NONE;
        textdata.key = "Software";
        textdata.text = software_text;
        png_set_text(png_ptr, info_ptr, &textdata, 1);
    }

    /* write all chunks up to (but not including) first IDAT */
    Trace((stderr, "GRR WritePNG:  calling png_write_info()\n"));
    png_write_info(png_ptr, info_ptr);
    png_write_flush(png_ptr);

    /* any transformations must be set *after* png_write_info() is called */
    Trace((stderr, "GRR WritePNG:  calling png_set_packing()\n"));
    png_set_packing(png_ptr);   /* squish multiple pixels into each byte */
/*  png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);  */

    /* shouldn't occur !! */
    if (image->alpha && color_type == PNG_COLOR_TYPE_PALETTE) {
        /* alpha channel version */
        RWSetMsg(msgText[WRITEPNG_CANT_USE_ALPHA_MASK_WITH_COLORMAPPED_IMAGE]);
        fprintf(stderr, "%s", msgText[WRITEPNG_CANT_USE_ALPHA_MASK_WITH_COLORMAPPED_IMAGE]);
        fflush(stderr);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 1;

    }

    rowbytes = (image->scale+((image->alpha)?1:0)) * image->width;
    row_pointers = (png_bytep *)xmalloc(image->height * sizeof(png_bytep));
    if (!row_pointers) {
        RWSetMsg(msgText[WRITEPNG_CANT_ALLOCATE_MEMORY_FOR_ROW_POINTERS]);
        fprintf(stderr, "%s\n",
                msgText[WRITEPNG_CANT_ALLOCATE_MEMORY_FOR_ROW_POINTERS]);
        fflush(stderr);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 1;
    }

    if (image->alpha) {
        unsigned char *png, *data, *alpha;
        png = (unsigned char *)
	          XtMalloc(rowbytes*image->height);
        data = image->data;
        alpha = image->alpha;
        for (j = 0;  j < image->height;  ++j) {
	    row_pointers[j] = (png_bytep)png;
            for (i = 0;  i < image->width;  ++i) {
	        memcpy(png, data, image->scale);
                png += image->scale;
                data += image->scale;
		*png++ = *alpha++;
	    }
	}
    } else {
        for (j = 0;  j < image->height;  ++j)
            row_pointers[j] = (png_bytep)image->data + j*rowbytes;
    }

#ifdef XPAINT_PNG_DEBUG___disabled_for_now
        {
            int wrapcount = (image->scale == 1)? 20 : 24;
            int rownum = (image->height >> 1);
            png_bytep row = row_pointers[rownum];

            Trace((stderr, "GRR WritePNG:  data for row %d:", rownum));
            for (i = 0;  i < rowbytes;  ++i, ++row) {
                if (i % wrapcount == 0)
                    Trace((stderr, "\n  "));
                Trace((stderr, "%02d ", *row));
            }
            Trace((stderr, "\nGRR WritePNG:  done with row %d.\n", rownum));
        }
#endif
    png_write_image(png_ptr, row_pointers);
    if (image->alpha) free(row_pointers[0]);
    free(row_pointers);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return 0;

} /* end function WritePNG() */



int
WritePNGn(char *file, Image *image)
{
    return WritePNG(file, image, 0);
}



int
WritePNGi(char *file, Image *image)
{
    return WritePNG(file, image, 1);
}



static void
xpaint_png_error_handler (png_structp png_ptr, png_const_charp msg)
{
    jmpbuf_wrapper  *jmpbuf_ptr;

    /* this function, aside from the extra step of retrieving the "error
     * pointer" (below) and the fact that it exists within the application
     * rather than within libpng, is essentially identical to libpng's
     * default error handler.  The second point is critical:  since both
     * setjmp() and longjmp() are called from the same code, they are
     * guaranteed to have compatible notions of how big a jmp_buf is,
     * regardless of whether _BSD_SOURCE or anything else has (or has not)
     * been defined. */

    fprintf(stderr, msgText[XPAINT_FATAL_LIBPNG_ERROR], msg);
    fflush(stderr);

    jmpbuf_ptr = png_get_error_ptr(png_ptr);
    if (jmpbuf_ptr == NULL) {         /* we are completely hosed now */
        fprintf(stderr, "%s", msgText[XPAINT_EXTREMELY_FATAL_LIBPNG_ERROR]);
        fflush(stderr);
        exit(99);
    }

    longjmp(jmpbuf_ptr->jmpbuf, 1);
}
