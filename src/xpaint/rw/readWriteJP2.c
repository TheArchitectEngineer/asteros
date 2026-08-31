#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include <openjpeg.h>

#ifdef U_JP2

/* *** PREFACE ***
 * OpenJPEG version 1.1.1 is wasteful in the extreme, with memory overhead of
 * several times the unpacked image size. So it can fail to handle even such
 * resolutions that fit into available memory with lots of room to spare. */

static int load_jpeg2000(char *file_name, ls_settings *settings)
{
	opj_dparameters_t par;
	opj_dinfo_t *dinfo;
	opj_cio_t *cio = NULL;
	opj_image_t *image = NULL;
	opj_image_comp_t *comp;
	opj_event_mgr_t useless_events; /* ! Silently made mandatory in v1.2 */
	unsigned char xtb[256], *dest, *buf = NULL;
	FILE *fp;
	int i, j, k, l, w, h, w0, nc, pr, step, delta, shift;
	int *src, cmask = CMASK_IMAGE, codec = CODEC_JP2, res;


	if ((fp = fopen(file_name, "rb")) == NULL) return (-1);

	/* Read in the entire file */
	fseek(fp, 0, SEEK_END);
	l = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buf = malloc(l);
	res = FILE_MEM_ERROR;
	if (!buf) goto ffail;
	res = FILE_LIB_ERROR;
	i = fread(buf, 1, l, fp);
	if (i < l) goto ffail;
	fclose(fp);
	if ((buf[0] == 0xFF) && (buf[1] == 0x4F)) codec = CODEC_J2K;

	/* Decompress it */
	dinfo = opj_create_decompress(codec);
	if (!dinfo) goto lfail;
	memset(&useless_events, 0, sizeof(useless_events));
	useless_events.error_handler = useless_events.warning_handler =
		useless_events.info_handler = stupid_callback;
	opj_set_event_mgr((opj_common_ptr)dinfo, &useless_events, stderr);
	opj_set_default_decoder_parameters(&par);
	opj_setup_decoder(dinfo, &par);
	cio = opj_cio_open((opj_common_ptr)dinfo, buf, l);
	if (!cio) goto lfail;
	if ((pr = !settings->silent)) ls_init("JPEG2000", 0);
	image = opj_decode(dinfo, cio);
	opj_cio_close(cio);
	opj_destroy_decompress(dinfo);
	free(buf);
	if (!image) goto ifail;
	
	/* Analyze what we got */
// !!! OpenJPEG 1.1.1 does *NOT* properly set image->color_space !!!
	if (image->numcomps < 3) /* Guess this is paletted */
	{
		settings->bpp = 1;
		settings->colors = 256;
		mem_scale_pal(settings->pal, 0, 0,0,0, 255, 255,255,255);
	}
	else settings->bpp = 3;
	if ((nc = settings->bpp) < image->numcomps) nc++ , cmask = CMASK_RGBA;
	comp = image->comps;
	settings->width = w = (comp->w + (1 << comp->factor) - 1) >> comp->factor;
	settings->height = h = (comp->h + (1 << comp->factor) - 1) >> comp->factor;
	for (i = 1; i < nc; i++) /* Check if all components are the same size */
	{
		comp++;
		if ((w != (comp->w + (1 << comp->factor) - 1) >> comp->factor) ||
			(h != (comp->h + (1 << comp->factor) - 1) >> comp->factor))
			goto ifail;
	}
	if ((res = allocate_image(settings, cmask))) goto ifail;

	/* Unpack data */
	for (i = 0 , comp = image->comps; i < nc; i++ , comp++)
	{
		if (i < settings->bpp) /* Image */
		{
			dest = settings->img[CHN_IMAGE] + i;
			step = settings->bpp;
		}
		else /* Alpha */
		{
			dest = settings->img[CHN_ALPHA];
			if (!dest) break; /* No alpha allocated */
			step = 1;
		}
		w0 = comp->w;
		delta = comp->sgnd ? 1 << (comp->prec - 1) : 0;
		shift = comp->prec > 8 ? comp->prec - 8 : 0;
		set_xlate(xtb, comp->prec - shift);
		for (j = 0; j < h; j++)
		{
			src = comp->data + j * w0;
			for (k = 0; k < w; k++)
			{
				*dest = xtb[(src[k] + delta) >> shift];
				dest += step;
			}
		}
	}
	res = 1;
ifail:	if (pr) progress_end();
	opj_image_destroy(image);
	return (res);
lfail:	opj_destroy_decompress(dinfo);
	free(buf);
	return (res);
ffail:	free(buf);
	fclose(fp);
	return (res);
}

static int save_jpeg2000(char *file_name, ls_settings *settings)
{
	opj_cparameters_t par;
	opj_cinfo_t *cinfo;
	opj_image_cmptparm_t channels[4];
	opj_cio_t *cio = NULL;
	opj_image_t *image;
	opj_event_mgr_t useless_events; // !!! Silently made mandatory in v1.2
	unsigned char *src;
	FILE *fp;
	int i, j, k, nc, step;
	int *dest, w = settings->width, h = settings->height, res = -1;


	if (settings->bpp == 1) return WRONG_FORMAT;

	if ((fp = fopen(file_name, "wb")) == NULL) return -1;

	/* Create intermediate structure */
	nc = settings->img[CHN_ALPHA] ? 4 : 3;
	memset(channels, 0, sizeof(channels));
	for (i = 0; i < nc; i++)
	{
		channels[i].prec = channels[i].bpp = 8;
		channels[i].dx = channels[i].dy = 1;
		channels[i].w = settings->width;
		channels[i].h = settings->height;
	}
	image = opj_image_create(nc, channels, CLRSPC_SRGB);
	if (!image) goto ffail;
	image->x0 = image->y0 = 0;
	image->x1 = w; image->y1 = h;

	/* Fill it */
	if (!settings->silent) ls_init("JPEG2000", 1);
	k = w * h;
	for (i = 0; i < nc; i++)
	{
		if (i < 3)
		{
			src = settings->img[CHN_IMAGE] + i;
			step = 3;
		}
		else
		{
			src = settings->img[CHN_ALPHA];
			step = 1;
		}
		dest = image->comps[i].data;
		for (j = 0; j < k; j++ , src += step) dest[j] = *src;
	}

	/* Compress it */
	cinfo = opj_create_compress(settings->ftype == FT_JP2 ? CODEC_JP2 : CODEC_J2K);
	if (!cinfo) goto fail;
	memset(&useless_events, 0, sizeof(useless_events));
	useless_events.error_handler = useless_events.warning_handler =
		useless_events.info_handler = stupid_callback;
	opj_set_event_mgr((opj_common_ptr)cinfo, &useless_events, stderr);
	opj_set_default_encoder_parameters(&par);
	par.tcp_numlayers = 1;
	par.tcp_rates[0] = settings->jp2_rate;
	par.cp_disto_alloc = 1;
	opj_setup_encoder(cinfo, &par, image);
	cio = opj_cio_open((opj_common_ptr)cinfo, NULL, 0);
	if (!cio) goto fail;
	if (!opj_encode(cinfo, cio, image, NULL)) goto fail;

	/* Write it */
	k = cio_tell(cio);
	if (fwrite(cio->buffer, 1, k, fp) == k) res = 0;

fail:	if (cio) opj_cio_close(cio);
	opj_destroy_compress(cinfo);
	opj_image_destroy(image);
	if (!settings->silent) progress_end();
ffail:	fclose(fp);
	return (res);
}
#endif

