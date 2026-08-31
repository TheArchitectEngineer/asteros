/* Xpaint-procedure */

#include <Xpaint.h>

/* 
 * The key-word "ImageCreate" is reserved for user-defined image routines;
 * Such a routine should create and process an image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, op = output pixel.
 * The procedure below just creates an image by mapping given functions
 * as red, green, blue components...
 */

#define NUM 4    /* number of user defined parameters */

#ifndef STATIC
struct textPromptInfo * prompts;
int *threshold;
#else
static struct textPromptInfo prompts[NUM];
static int threshold[NUM];
#endif

/* 
 * An example of user defined function which performs "image cleaning"
 * depending on the values of threshold[i], i=0..NUM-1.
 */

Image * function()
{
    Image * output;
    unsigned char *p;
    int x, y, a, b;

    output = ImageNew(threshold[0], threshold[1]);
    a = threshold[2];
    b = threshold[3];

    for (y = 0; y < output->height; y++) {
        for (x = 0; x < output->width; x++) {
            p = ImagePixel(output, x, y);
            p[0] = (x+a*y+b*x*(1000-x)/500)%256;
            p[1] = y%256;
            p[2] = x%256;
        }
    }
    return output;
}

static void image_proc(TextPromptInfo * info)
{
    Widget w = Global.toplevel;
    Image * image;
    int i;
 
    /* read user supplied data, and write them back to prompt strings */
    for (i=0; i<NUM; i++) {
      if (!info->prompts[i].rstr) continue;
      threshold[i] = atoi(info->prompts[i].rstr);
      info->prompts[i].str = info->prompts[i].rstr;
    }

    if (threshold[0]<10) {
      Notice(w, "Width should be at least equal to 10");
      return;
    }

    if (threshold[1]<10) {
      Notice(w, "Height should be at least equal to 10");
      return;
    }

    /* create image with user supplied function */    
    image = function();
    if (!image) return;

    /* open canvas of corresponding size */
    Global.curpaint = graphicCreate(makeGraphicShell(w), 
                      image->width, image->height, 1, None, None);

    /* copy image to canvas (with no mask) */
    ImageToCanvas(image, None);

#ifndef STATIC
    /* free data structures */
    free(prompts);
    free(threshold);
#endif
}

void PaintProcedure()
{
    static step =0;
    int i;

#ifndef STATIC
    prompts = (struct textPromptInfo *)malloc(NUM*sizeof(struct textPromptInfo));
    threshold = (int *)malloc(sizeof(int));
#endif
    /* Define NUM prompt strings (but only at first invocation) */
    if (step==0) {
      prompts[0].prompt = "Width";
      prompts[0].str =  strdup("600");
      prompts[0].rstr =  NULL;
      prompts[0].len = 12;
      prompts[1].prompt = "Height";
      prompts[1].str =  strdup("400");
      prompts[1].rstr =  NULL;
      prompts[1].len = 12;
      prompts[2].prompt = "Parameter a";
      prompts[2].str =  strdup("1");
      prompts[2].rstr =  NULL;
      prompts[2].len = 12;
      prompts[3].prompt = "Parameter b";
      prompts[3].str =  strdup("1");
      prompts[3].rstr =  NULL;
      prompts[3].len = 12;
      ++step;
    }

    for (i=0; i<NUM; i++)
      threshold[i] = atoi(prompts[i].str);

    /* start popup with num values to adjust */
    (void) PopupMenu("Create image :", NUM, prompts, &image_proc);
}


