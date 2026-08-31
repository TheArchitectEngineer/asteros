/* Xpaint-filter */

#include <Xpaint.h>

/* 
 * The key-word "FilterProcess" is reserved for user-defined filter routines;
 * Such a filter processes an "input" image and renders an "output" image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, cp = canvas pixel, ip = input pixel, op = output pixel
 * the procedure merges the input (region) with the underlying canvas image
 *
 * This is a sophisticated example showing the use of a popup menu by which
 * the user can enter further parameters to adjust the given filter routine.
 *
 */

#define NUM 2    /* number of user defined parameters */
static struct textPromptInfo prompts[NUM];
static int threshold[NUM];

/* 
 * An example of user defined function which performs "image cleaning"
 * depending on the values of threshold[i], i=0..NUM-1.
 */
Image * function(Image * input)
{
    Image * output;
    unsigned char *ip, *op;
    int x, y;

    output = ImageNew(input->width, input->height);
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x, y);
            if (ip[0]+ip[1]+ip[2]>threshold[0])
 	      op[0]=op[1]=op[2]=255;
            else
            if (ip[0]+ip[1]+ip[2]<threshold[1])
 	      op[0]=op[1]=op[2]=0;
            else
	      memcpy(op, ip, 3);
        }
    }
    return output;
}

/*
 * Fairly standard procedure. 
 */
void filter_proc(TextPromptInfo *info)
{
    Widget paint = Global.curpaint;
    int i;

    /* read user supplied data, and write them back to prompt strings */
    for (i=0; i<NUM; i++) {
      if (!info->prompts[i].rstr) continue;
      threshold[i] = atoi(info->prompts[i].rstr);
      if (info->prompts[i].rstr != info->prompts[i].str) {
        free(info->prompts[i].str);
        info->prompts[i].str = strdup(info->prompts[i].rstr);
      }
    }

    /* convert region image by user supplied function */    
    ImageRegionProcess(paint, function);
}

void FilterProcess(Image * input, Image * output)
{
    static step =0;

    /* Define NUM prompt strings (but only at first invocation) */
    if (step==0) {
      prompts[0].prompt = "Light threshold";
      prompts[0].str =  strdup("700");
      prompts[0].rstr =  NULL;
      prompts[0].len = 12;
      prompts[1].prompt = "Dark threshold";
      prompts[1].str =  strdup("0");
      prompts[1].rstr =  NULL;
      prompts[1].len = 12;
      ++step;
    }

    /* copy blindly image input into output before any further action */
    ImageCopy(input, output);

    /* start popup with num values to adjust */
    PopupMenu("Clean sheet with given thresholds", NUM, prompts, filter_proc);
}


