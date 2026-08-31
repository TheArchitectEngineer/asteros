/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
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

/* $Id: operation.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>
#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <X11/xpm.h>

#include "xaw_incdir/Form.h"
#include "xaw_incdir/SmeBSB.h"
#include "xaw_incdir/Box.h"
#include "xaw_incdir/Toggle.h"
#include "xaw_incdir/Viewport.h"

#include "xpaint.h"
#include "misc.h"
#include "menu.h"
#include "messages.h"
#include "Paint.h"
#include "text.h"
#include "ops.h"
#include "graphic.h"
#include "image.h"
#include "operation.h"
#include "protocol.h"
#include "region.h"

/* Pixmaps for toolbox icons */

#include "bitmaps/tools/brushOp.xpm"
#include "bitmaps/tools/eraseOp.xpm"
#include "bitmaps/tools/sprayOp.xpm"
#include "bitmaps/tools/pencilOp.xpm"
#include "bitmaps/tools/dotPenOp.xpm"
#include "bitmaps/tools/dynPenOp.xpm"
#include "bitmaps/tools/lineOp.xpm"
#include "bitmaps/tools/brokenlineOp.xpm"
#include "bitmaps/tools/arcOp.xpm"
#include "bitmaps/tools/arrowOp.xpm"
#include "bitmaps/tools/smearOp.xpm"
#include "bitmaps/tools/textOp.xpm"
#include "bitmaps/tools/boxOp.xpm"
/* #include "bitmaps/tools/rayOp.xpm" */
#include "bitmaps/tools/filledBoxOp.xpm"
#include "bitmaps/tools/filledOvalOp.xpm"
#include "bitmaps/tools/filledPolygonOp.xpm"
#include "bitmaps/tools/filledFreehandOp.xpm"
#include "bitmaps/tools/filledSplineOp.xpm"
#include "bitmaps/tools/ovalOp.xpm"
#include "bitmaps/tools/splineOp.xpm"
#include "bitmaps/tools/polygonOp.xpm"
#include "bitmaps/tools/freehandOp.xpm"
#include "bitmaps/tools/selectBoxOp.xpm"
#include "bitmaps/tools/selectOvalOp.xpm"
#include "bitmaps/tools/selectPolygonOp.xpm"
#include "bitmaps/tools/selectSplineOp.xpm"
#include "bitmaps/tools/selectFreehandOp.xpm"
#include "bitmaps/tools/fillOp.xpm"
#include "bitmaps/tools/gradientFillOp.xpm"
#ifdef FEATURE_FRACTAL
#include "bitmaps/tools/fractalFillOp.xpm"
#endif

/* static void changeLineAction(Widget, XEvent *); */
static void changeDynPencilAction(Widget, XEvent *);
static void changeBrushAction(Widget, XEvent *);
static void changeBrushParamAction(Widget, XEvent *);
static void changeSprayAction(Widget, XEvent *);
static void changeArrowAction(Widget, XEvent *);
static void changeFontAction(Widget, XEvent *);
static void changeBoxParamAction(Widget, XEvent *);
static void changePolygonParamAction(Widget, XEvent *);
static void changeGradientFillParamAction(Widget, XEvent *);
static void changeFractalFillParamAction(Widget, XEvent *);

static void 
setOperation(Widget w, XtPointer opArg, XtPointer junk2)
{
    OperationPair *op = (OperationPair *) opArg;

    if (op == CurrentOp || op == None)
	return;

    routine = XtName(w);

    /* terminate possibly unfinished operation */
    if (CurrentOp)
        GraphicSetOp((OperationRemoveProc) CurrentOp->remove, 
                     (OperationAddProc) NULL);


    /* start new one */
    CurrentOp = op;
    GraphicAll(setToolIconPixmap, NULL);
    GraphicSetOp((OperationRemoveProc) NULL, 
                 (OperationAddProc) CurrentOp->add);
}

/*
** Set brush parameters callbacks
 */
static char brushOpacityStr[10] = "20";
static void 
brushOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int opacity = atof(info->prompts[0].rstr);

    if (opacity < 0 || opacity > 100) {
	Notice(w, msgText[OPACITY_SHOULD_BE_BETWEEN_ZERO_AND_HUNDRED_PERCENT]);
	return;
    }
    BrushSetParameters(opacity / 100.0);

    sprintf(brushOpacityStr, "%d", opacity);
}

static void 
brushMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[1];

    value[0].prompt = msgText[OPACITY];
    value[0].str = brushOpacityStr;
    value[0].len = 4;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_BRUSH_PARAMETERS];
    info.nprompt = 1;

    TextPrompt(w, "brushparams", &info, brushOkCallback, NULL, NULL);
}

/*
** Set fractal parameter callbacks
 */
char fractalDensityStr[10] = "40";
static void 
fractalOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int opacity = atof(info->prompts[0].rstr);

    if (opacity < 0 || opacity > 100) {
	Notice(w, msgText[OPACITY_SHOULD_BE_BETWEEN_ZERO_AND_HUNDRED_PERCENT]);
	return;
    }
    BrushSetParameters(opacity / 100.0);

    sprintf(fractalDensityStr, "%d", opacity);
}

static void 
fractalMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[1];

    value[0].prompt = msgText[DENSITY];
    value[0].str = fractalDensityStr;
    value[0].len = 4;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_FRACTAL_DENSITY];
    info.nprompt = 1;

    TextPrompt(w, "fractalparams", &info, fractalOkCallback, NULL, NULL);
}

/*
** Set dynamic pencil parameters callbacks
*/
static char dynPencilWidthStr[10] = "10";
static char dynPencilMassStr[10] = "600";
static char dynPencilDragStr[10] = "15";

static void dynPencilOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo*)infoArg;
    float width = atof(info->prompts[0].rstr);
    float mass = atof(info->prompts[1].rstr);
    float drag = atof(info->prompts[2].rstr);
    
    if (width < 1 || width > 100) {
	Notice(w, msgText[WIDTH_SHOULD_BE_BETWEEN_ONE_AND_ONE_HUNDRED]);
	return;
    }

    if (mass < 10 || mass > 2000) {
	Notice(w, msgText[MASS_SHOULD_BE_BETWEEN_TEN_AND_TWO_THOUSAND]);
	return;
    }

    if (drag < 0 || drag > 50) {
	Notice(w, msgText[DRAG_SHOULD_BE_BETWEEN_ZERO_AND_FIFTY]);
	return;
    }

    DynPencilSetParameters(width, mass, drag);
  
    sprintf(dynPencilWidthStr, "%0.4g", width);
    sprintf(dynPencilMassStr, "%0.4g", mass);
    sprintf(dynPencilDragStr, "%0.4g", drag);
}

static void dynPencilMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[3];

    value[0].prompt = msgText[DYNAMIC_WIDTH];
    value[0].str = dynPencilWidthStr;
    value[0].len = 5;
    value[1].prompt = msgText[DYNAMIC_MASS];
    value[1].str = dynPencilMassStr;;
    value[1].len = 5;
    value[2].prompt = msgText[DYNAMIC_DRAG];
    value[2].str = dynPencilDragStr;;
    value[2].len = 5;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_DYNAMIC_PENCIL_PARAMETERS];
    info.nprompt = 3;

    TextPrompt(w, "dynpencilparams", &info, dynPencilOkCallback, NULL, NULL);
}

/*
** Set spray parameters callbacks
 */
static char sprayDensityStr[10] = "10";
static char sprayRadiusStr[10] = "10";
static char sprayRateStr[10] = "10";
static void 
sprayOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int radius = atoi(info->prompts[0].rstr);
    int density = atoi(info->prompts[1].rstr);
    int rate = atoi(info->prompts[2].rstr);

    if (radius < 1 || radius > 100) {
	Notice(w, msgText[RADIUS_SHOULD_BE_BETWEEN_ONE_AND_ONE_HUNDRED]);
	return;
    }
    if (density < 1 || density > 500) {
	Notice(w, msgText[DENSITY_SHOULD_BE_BETWEEN_ONE_AND_FIVE_HUNDRED]);
	return;
    }
    if (rate < 1 || rate > 500) {
	Notice(w, msgText[RATE_SHOULD_BE_BETWEEN_ONE_AND_FIVE_HUNDRED]);
	return;
    }
    SpraySetParameters(radius, density, rate);

    sprintf(sprayDensityStr, "%d", density);
    sprintf(sprayRadiusStr, "%d", radius);
    sprintf(sprayRateStr, "%d", rate);
}

static void 
sprayMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[3];

    value[0].prompt = msgText[SPRAY_RADIUS];
    value[0].str = sprayRadiusStr;
    value[0].len = 4;
    value[1].prompt = msgText[SPRAY_DENSITY];
    value[1].str = sprayDensityStr;
    value[1].len = 4;
    value[2].prompt = msgText[SPRAY_RATE];
    value[2].str = sprayRateStr;
    value[2].len = 4;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_SPRAY_PARAMETERS];
    info.nprompt = 3;

    TextPrompt(w, "sprayparams", &info, sprayOkCallback, NULL, NULL);
}

/*
** Set arrow head parameters callbacks
*/
static char arrowHeadTypeStr[10] = "1";
static char arrowHeadSizeStr[10] = "15";
static char arrowHeadAngleStr[10] = "25.0";

static void arrowHeadOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo*)infoArg;
    int headtype = atoi(info->prompts[0].rstr);
    int size = atoi(info->prompts[1].rstr);
    float angle = atof(info->prompts[2].rstr);
    
    if (headtype < 1 || headtype > 3) {
	Notice(w, msgText[TYPE_SHOULD_BE_BETWEEN_ONE_AND_MAXTYPE]);
	return;
    }

    if (size < 1 || size > 1000) {
	Notice(w, msgText[SIZE_SHOULD_BE_BETWEEN_ONE_AND_ONE_THOUSAND]);
	return;
    }

    if (angle < 0.0 || angle > 60.0) {
	Notice(w, msgText[ANGLE_SHOULD_BE_BETWEEN_ZERO_AND_SIXTY]);
	return;
    }

    ArrowHeadSetParameters(headtype, size, angle);
  
    sprintf(arrowHeadTypeStr, "%d", headtype);
    sprintf(arrowHeadSizeStr, "%d", size);
    sprintf(arrowHeadAngleStr, "%2.4g", angle);
}

static void arrowHeadMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[3];

    value[0].prompt = msgText[ARROWHEAD_TYPE];
    value[0].str = arrowHeadTypeStr;
    value[0].len = 5;
    value[1].prompt = msgText[ARROWHEAD_SIZE];
    value[1].str = arrowHeadSizeStr;;
    value[1].len = 5;
    value[2].prompt = msgText[ARROWHEAD_ANGLE];
    value[2].str = arrowHeadAngleStr;;
    value[2].len = 5;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_ARROWHEAD_PARAMETERS];
    info.nprompt = 3;

    TextPrompt(w, "arrowheadparams", &info, arrowHeadOkCallback, NULL, NULL);
}

/*
** Gradient fill set parameters callbacks
 */
static char gradientFillAngleStr[10] = "0";
static char gradientFillPadStr[10] = "0";
static char gradientFillHOffsetStr[10] = "0";
static char gradientFillVOffsetStr[10] = "0";
static char gradientFillStepsStr[10] = "";

static void 
gradientFillOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    float HOffset, VOffset, Pad, Angle;
    int Steps;

    Angle = atof(info->prompts[0].rstr);
    Pad = atof(info->prompts[1].rstr);
    HOffset = atof(info->prompts[2].rstr);
    VOffset = atof(info->prompts[3].rstr);
    Steps = atoi(info->prompts[4].rstr);
    if (HOffset < -100 || HOffset > 100) {
	Notice(w, msgText[
            HORIZONTAL_OFFSET_SHOULD_BE_BETWEEN_PLUS_MINUS_HUNDRED_PERCENT]);
	return;
    }
    if (VOffset < -100 || VOffset > 100) {
	Notice(w, msgText[
            VERTICAL_OFFSET_SHOULD_BE_BETWEEN_PLUS_MINUS_HUNDRED_PERCENT]);
	return;
    }
    if (Pad < -49 || Pad > 49) {
	Notice(w, msgText[
            PAD_SHOULD_BE_BETWEEN_PLUS_MINUS_FOURTY_NINE_PERCENT]);
	return;
    }
    if (Angle < -360 || Angle > 360) {
	Notice(w, msgText[
            ANGLE_SHOULD_BE_BETWEEN_PLUS_MINUS_THREE_HUNDRED_SIXTY_DEGREES]);
	return;
    }
    if (Steps < 1 || Steps > 300) {
	Notice(w, msgText[
            NUMBER_OF_STEPS_SHOULD_BE_BETWEEN_ONE_AND_THREE_HUNDRED]);
	return;
    }
    GradientFillSetParameters((float)(VOffset/100.0), (float)(HOffset/100.0), 
                              (float)(Pad/100.0), (float)Angle, Steps);

    sprintf(gradientFillVOffsetStr, "%3.4g", VOffset);
    sprintf(gradientFillHOffsetStr, "%3.4g", HOffset);
    sprintf(gradientFillPadStr, "%3.4g", Pad);
    sprintf(gradientFillAngleStr, "%3.4g", Angle);
    sprintf(gradientFillStepsStr, "%3d", Steps);
}

static void 
gradientFillMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[5];

    if (!gradientFillStepsStr[0]) {
	 if (DefaultDepthOfScreen(XtScreen(GetShell(w))) <= 8)
	      strcpy(gradientFillStepsStr, "25");
	 else
	      strcpy(gradientFillStepsStr, "300");
    }

    value[0].prompt = msgText[GRADIENT_ANGLE];
    value[0].str = gradientFillAngleStr;
    value[0].len = 8;
    value[1].prompt = msgText[GRADIENT_PAD];
    value[1].str = gradientFillPadStr;
    value[1].len = 8;
    value[2].prompt = msgText[GRADIENT_HORIZONTAL_OFFSET];
    value[2].str = gradientFillHOffsetStr;
    value[2].len = 8;
    value[3].prompt = msgText[GRADIENT_VERTICAL_OFFSET];
    value[3].str = gradientFillVOffsetStr;
    value[3].len = 8;
    value[4].prompt = msgText[GRADIENT_STEPS];
    value[4].str = gradientFillStepsStr;
    value[4].len = 6;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_GRADIENT_FILL_PARAMETERS];
    info.nprompt = 5;

    TextPrompt(w, "gradientFillparams", &info, gradientFillOkCallback, NULL, NULL);
}

/*
** Set box parameters callbacks
*/
static char boxSizeStr[10] = "7";
static char boxRatioStr[10] = "0";

static void boxOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo*)infoArg;
    int   boxSize = atoi(info->prompts[0].rstr);
    float boxRatio = atof(info->prompts[1].rstr);
    
    if (boxSize < 0 || boxSize > 1000) {
	Notice(w, msgText[BOX_ROUND_CORNER_ABSOLUTE_SIZE_SHOULD_BE_BETWEEN_ZERO_AND_ONE_THOUSAND]);
        return;
    }
    if (boxRatio < 0.0 || boxRatio > 1.0) {
	Notice(w, msgText[BOX_ROUND_CORNER_RELATIVE_SIZE_SHOULD_BE_BETWEEN_ZERO_AND_ONE]);
	return;
    }
    if (boxSize > 0 && boxRatio > 0.0) {
	Notice(w, msgText[BOX_ROUND_CORNER_ABSOLUTE_AND_RELATIVE_SIZE_CANNOT_BE_BOTH_POSITIVE]);
	return;
    }

    BoxSetParameter(boxSize, boxRatio);
    sprintf(boxSizeStr, "%d", boxSize);
    sprintf(boxRatioStr, "%2.4g", boxRatio);
}

static void boxMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[2];

    value[0].prompt = msgText[BOX_ROUND_CORNER_ABSOLUTE_SIZE];
    value[0].str = boxSizeStr;;
    value[0].len = 5;
    value[1].prompt = msgText[BOX_ROUND_CORNER_RELATIVE_SIZE];
    value[1].str = boxRatioStr;;
    value[1].len = 5;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_ROUND_CORNER_SIZE];
    info.nprompt = 2;

    TextPrompt(w, "boxparams", &info, boxOkCallback, NULL, NULL);
}

/*
** Set polygon parameters callbacks
*/
static char polygonSidesStr[10] = "5";
static char polygonRatioStr[10] = "0.382";

static void polygonOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo*)infoArg;
    int sides = atoi(info->prompts[0].rstr);
    float ratio = atof(info->prompts[1].rstr);
    
    if (sides < 3 || sides > 100) {
	Notice(w, msgText[NUMBER_OF_SIDES_SHOULD_BE_BETWEEN_THREE_AND_ONE_HUNDRED]);
	return;
    }

    if (ratio < 0.0 || ratio > 1.0) {
	Notice(w, msgText[INNER_RATIO_SHOULD_BE_BETWEEN_ZERO_AND_ONE]);
	return;
    }

    PolygonSetParameters(0, sides, ratio);
  
    sprintf(polygonSidesStr, "%d", sides);
    sprintf(polygonRatioStr, "%2.4g", ratio);
}

static void polygonMenuCallback(Widget w)
{
    static TextPromptInfo info;
    static struct textPromptInfo value[2];

    value[0].prompt = msgText[NUMBER_OF_SIDES_OF_POLYGON];
    value[0].str = polygonSidesStr;;
    value[0].len = 5;
    value[1].prompt = msgText[STARLIKE_POLYGON_INNER_RATIO];
    value[1].str = polygonRatioStr;;
    value[1].len = 5;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_SPECIAL_POLYGON_PARAMETERS];
    info.nprompt = 2;

    TextPrompt(w, "polygonparams", &info, polygonOkCallback, NULL, NULL);
}

/*
**  Exit callback (simple)
 */
static void 
exitOkCallback(Widget w, XtPointer junk, XtPointer junk2)
{
#if 0
    XtDestroyWidget(GetToplevel(w));
    XtDestroyApplicationContext(XtWidgetToApplicationContext(w));
    Global.timeToDie = True;
#else
    exit(0);
#endif
}

static void 
exitCancelCallback(Widget paint, XtPointer junk, XtPointer junk2)
{
}

static void 
exitPaintCheck(Widget paint, void *sumArg)
{
    int *sum = (int *) sumArg;
    Boolean flg;
    XtVaGetValues(paint, XtNdirty, &flg, NULL);
    *sum += flg ? 1 : 0;
}

void 
exitPaint(Widget w, XtPointer junk, XtPointer junk2)
{
    int total = 0;

    GraphicAll(exitPaintCheck, (void *) &total);

    if (total == 0) {
	exitOkCallback(w, NULL, NULL);
	return;
    }
    AlertBox(w, msgText[UNSAVED_CHANGES_WISH_TO_QUIT],
	     exitOkCallback, exitCancelCallback, NULL);
}

/*
**
 */
void 
takeScreenshot(Widget w, XtPointer junk, XtPointer junk2)
{
    if (XtParent(w)) w = GetToplevel(XtParent(w));
    if (w) {
       PopdownMenusGlobal();
       XUnmapWindow(XtDisplay(Global.toplevel), XtWindow(Global.toplevel));
       XFlush(XtDisplay(Global.toplevel));
       usleep(200000);
       ScreenshotImage(w, junk, 0);
       XMapWindow(XtDisplay(Global.toplevel), XtWindow(Global.toplevel));
    }
}

/*
**  Button popups 
 */
void brushPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void erasePopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void arcPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void polygonPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void fillPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void gradientFillPopupCB2(Widget w, XtPointer junk, XtPointer junk2);
#ifdef FEATURE_FRACTAL
static void fractalFillPopupCB2(Widget w, XtPointer junk, XtPointer junk2);
#endif
static void selectPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void dynPencilPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void sprayPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void linePopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void ovalPopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void splinePopupCB(Widget w, XtPointer junk, XtPointer junk2);
static void boxPopupCB(Widget w, XtPointer junk, XtPointer junk2);

#define GENERATE_HELP(name, hlpstr)				\
		static PaintMenuItem name [] = {		\
			MI_SEPARATOR(),				\
			MI_SIMPLECB("help", HelpDialog, hlpstr)	\
		};

GENERATE_HELP(pencilPopup, "toolbox.tools.pencil")

GENERATE_HELP(dotPencilPopup, "toolbox.tools.dotPencil")

static PaintMenuItem dynPencilPopup[] = 
{
    MI_SEPARATOR(),
    MI_FLAGCB("autofinish", MF_CHECK, dynPencilPopupCB, 0),
    MI_SIMPLECB("select", changeDynPencilAction, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.dynpencil"),
};

static PaintMenuItem brushPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("opaque", MF_CHECKON | MF_GROUP1, brushPopupCB, 0), 
    MI_FLAGCB("transparent", MF_CHECK | MF_GROUP1, brushPopupCB, 1),
    MI_FLAGCB("stain", MF_CHECK | MF_GROUP1, brushPopupCB, 2),
    MI_SIMPLECB("select", BrushSelect, NULL),
    MI_SIMPLECB("param", changeBrushParamAction, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.brush"),
};

static PaintMenuItem sprayPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("gauss", MF_CHECKON, sprayPopupCB, 0),
    MI_SIMPLECB("select", changeSprayAction, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.spray"),
};

static PaintMenuItem smearPopup[] =
{
    MI_SEPARATOR(),
    MI_SIMPLECB("select", BrushSelect, NULL),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.smear"),
};

static PaintMenuItem linePopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("multi", MF_CHECK, linePopupCB, 0),
    MI_FLAGCB("vector", MF_CHECK, linePopupCB, 1),
    MI_SIMPLECB("param", arrowHeadMenuCallback, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.line"),
};

GENERATE_HELP(brokenlinePopup, "toolbox.tools.brokenline")

static PaintMenuItem arcPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("connect", MF_CHECKON | MF_GROUP1, arcPopupCB, 0),
    MI_FLAGCB("quadrant", MF_CHECK | MF_GROUP1, arcPopupCB, 1),
    MI_FLAGCB("centered", MF_CHECK | MF_GROUP1, arcPopupCB, 2),
    MI_FLAGCB("boxed", MF_CHECK | MF_GROUP1, arcPopupCB, 3),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.arc"),
};

static PaintMenuItem arrowPopup[] =
{
    MI_SEPARATOR(),
    MI_SIMPLECB("param", arrowHeadMenuCallback, NULL),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.arrow"),
};

static PaintMenuItem textPopup[] =
{
    MI_SEPARATOR(),
    MI_SIMPLECB("select", changeFontAction, NULL),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.text"),
};

static PaintMenuItem erasePopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("original", MF_CHECKON, erasePopupCB, 0),
    MI_SIMPLECB("select", changeBrushAction, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.erase"),
};

static PaintMenuItem boxPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("center", MF_CHECK, boxPopupCB, 0),
    MI_FLAGCB("round", MF_CHECK, boxPopupCB, 1),
    MI_SIMPLECB("param", changeBoxParamAction, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.box"),
};

static PaintMenuItem filledBoxPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("center", MF_CHECK, boxPopupCB, 0),
    MI_FLAGCB("round", MF_CHECK, boxPopupCB, 1),
    MI_SIMPLECB("param", changeBoxParamAction, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.box"),
};

static PaintMenuItem selectBoxPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("center", MF_CHECK, boxPopupCB, 0),
    MI_FLAGCB("round", MF_CHECK, boxPopupCB, 1),
    MI_SIMPLECB("param", changeBoxParamAction, NULL),
    MI_SEPARATOR(),
    MI_FLAGCB("shape", MF_CHECKON | MF_GROUP1, selectPopupCB, 0),
    MI_FLAGCB("not_color", MF_CHECK | MF_GROUP1, selectPopupCB, 1),
    MI_FLAGCB("only_color", MF_CHECK | MF_GROUP1, selectPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.select"),
};

static PaintMenuItem ovalPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("center", MF_CHECK, ovalPopupCB, 0),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.oval"),
};

static PaintMenuItem filledOvalPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("center", MF_CHECK, ovalPopupCB, 0),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.oval"),
};

static PaintMenuItem selectOvalPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("center", MF_CHECK, ovalPopupCB, 0),
    MI_SEPARATOR(),
    MI_FLAGCB("shape", MF_CHECKON | MF_GROUP1, selectPopupCB, 0),
    MI_FLAGCB("not_color", MF_CHECK | MF_GROUP1, selectPopupCB, 1),
    MI_FLAGCB("only_color", MF_CHECK | MF_GROUP1, selectPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.select"),
};

GENERATE_HELP(freehandPopup, "toolbox.tools.freehand")

static PaintMenuItem selectFreehandPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("shape", MF_CHECKON | MF_GROUP1, selectPopupCB, 0),
    MI_FLAGCB("not_color", MF_CHECK | MF_GROUP1, selectPopupCB, 1),
    MI_FLAGCB("only_color", MF_CHECK | MF_GROUP1, selectPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.select"),
};

static PaintMenuItem polygonPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("arbitrary", MF_CHECKON | MF_GROUP1, polygonPopupCB, 0), 
    MI_FLAGCB("regular", MF_CHECK | MF_GROUP1, polygonPopupCB, 1),
    MI_FLAGCB("starlike", MF_CHECK | MF_GROUP1, polygonPopupCB, 2),
    MI_SIMPLECB("param", polygonMenuCallback, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.polygon"),
};

static PaintMenuItem filledPolygonPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("arbitrary", MF_CHECKON | MF_GROUP1, polygonPopupCB, 0), 
    MI_FLAGCB("regular", MF_CHECK | MF_GROUP1, polygonPopupCB, 1),
    MI_FLAGCB("starlike", MF_CHECK | MF_GROUP1, polygonPopupCB, 2),
    MI_SIMPLECB("param", polygonMenuCallback, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.polygon"),
};

static PaintMenuItem selectPolygonPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("arbitrary", MF_CHECKON | MF_GROUP1, polygonPopupCB, 0), 
    MI_FLAGCB("regular", MF_CHECK | MF_GROUP1, polygonPopupCB, 1),
    MI_FLAGCB("starlike", MF_CHECK | MF_GROUP1, polygonPopupCB, 2),
    MI_SIMPLECB("param", polygonMenuCallback, NULL),
    MI_SEPARATOR(),
    MI_FLAGCB("shape", MF_CHECKON | MF_GROUP2, selectPopupCB, 0),
    MI_FLAGCB("not_color", MF_CHECK | MF_GROUP2, selectPopupCB, 1),
    MI_FLAGCB("only_color", MF_CHECK | MF_GROUP2, selectPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.select"),
};

static PaintMenuItem splinePopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("open", MF_CHECKON | MF_GROUP1, splinePopupCB, 0),
    MI_FLAGCB("closed", MF_CHECK | MF_GROUP1, splinePopupCB, 1),
    MI_FLAGCB("closed_up", MF_CHECK | MF_GROUP1, splinePopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.spline"),
};

static PaintMenuItem filledSplinePopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("open", MF_CHECKON | MF_GROUP1, splinePopupCB, 0),
    MI_FLAGCB("closed", MF_CHECK | MF_GROUP1, splinePopupCB, 1),
    MI_FLAGCB("closed_up", MF_CHECK | MF_GROUP1, splinePopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.spline"),
};

static PaintMenuItem selectSplinePopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("open", MF_CHECKON | MF_GROUP1, splinePopupCB, 0),
    MI_FLAGCB("closed", MF_CHECK | MF_GROUP1, splinePopupCB, 1),
    MI_FLAGCB("closed_up", MF_CHECK | MF_GROUP1, splinePopupCB, 2),
    MI_SEPARATOR(),
    MI_FLAGCB("shape", MF_CHECKON | MF_GROUP2, selectPopupCB, 0),
    MI_FLAGCB("not_color", MF_CHECK | MF_GROUP2, selectPopupCB, 1),
    MI_FLAGCB("only_color", MF_CHECK | MF_GROUP2, selectPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.select"),
};

static PaintMenuItem fillPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("fill", MF_CHECKON | MF_GROUP1, fillPopupCB, 0),
    MI_FLAGCB("change", MF_CHECK | MF_GROUP1, fillPopupCB, 1),
    MI_FLAGCB("fill_range", MF_CHECK | MF_GROUP1, fillPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.fill"),
};

static PaintMenuItem gradientFillPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("radial", MF_CHECKON | MF_GROUP1, gradientFillPopupCB2, GFILL_RADIAL),
    MI_FLAGCB("linear", MF_CHECK | MF_GROUP1, gradientFillPopupCB2, GFILL_LINEAR),
    MI_FLAGCB("conical", MF_CHECK | MF_GROUP1, gradientFillPopupCB2, GFILL_CONE),
    MI_FLAGCB("square", MF_CHECK | MF_GROUP1, gradientFillPopupCB2, GFILL_SQUARE),
    MI_SIMPLECB("param", changeGradientFillParamAction, NULL),
    MI_SEPARATOR(),
    MI_FLAGCB("fill", MF_CHECKON | MF_GROUP2, fillPopupCB, 0),
    MI_FLAGCB("change", MF_CHECK | MF_GROUP2, fillPopupCB, 1),
    MI_FLAGCB("fill_range", MF_CHECK | MF_GROUP2, fillPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.gradientFill"),
};

#ifdef FEATURE_FRACTAL
static PaintMenuItem fractalFillPopup[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("plasma", MF_CHECKON | MF_GROUP1, fractalFillPopupCB2, 0),
    MI_FLAGCB("clouds", MF_CHECK | MF_GROUP1, fractalFillPopupCB2, 1),
    MI_FLAGCB("landscape", MF_CHECK | MF_GROUP1, fractalFillPopupCB2, 2),
    MI_SIMPLECB("param", changeFractalFillParamAction, NULL),
    MI_SEPARATOR(),
    MI_FLAGCB("fill", MF_CHECKON | MF_GROUP2, fillPopupCB, 0),
    MI_FLAGCB("change", MF_CHECK | MF_GROUP2, fillPopupCB, 1),
    MI_FLAGCB("fill_range", MF_CHECK | MF_GROUP2, fillPopupCB, 2),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "toolbox.tools.fractalFill"),
};
#endif

void 
brushPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    BrushSetMode(nm);
    MenuCheckItem(brushPopup[nm + 1].widget, True);
}

static void 
ovalPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    Boolean m = !CircleGetStyle();

    CircleSetStyle(m);
    MenuCheckItem(ovalPopup[1].widget, m);
    MenuCheckItem(filledOvalPopup[1].widget, m);
    MenuCheckItem(selectOvalPopup[1].widget, m);
}

static void 
polygonPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    MenuCheckItem(polygonPopup[nm + 1].widget, True);
    MenuCheckItem(filledPolygonPopup[nm + 1].widget, True);
    MenuCheckItem(selectPolygonPopup[nm + 1].widget, True);
    PolygonSetParameters(nm+1, 0.0, 0.0);
}

static void 
boxPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    Boolean b;
    int m;
    int nm = (int)(long)junk;

    m = BoxGetStyle(); 
    b = (m & (1<<nm))? False : True;
    if (nm == 0) m = (m&2) + 1-(m&1);
    if (nm == 1) m = 2 - (m&2) + (m&1);
    BoxSetStyle(m);
    MenuCheckItem(boxPopup[nm + 1].widget, b);
    MenuCheckItem(filledBoxPopup[nm + 1].widget, b);
    MenuCheckItem(selectBoxPopup[nm + 1].widget, b);
}

static void 
arcPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    ArcSetMode(nm);
    MenuCheckItem(w, True);
}

static void 
splinePopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    MenuCheckItem(splinePopup[nm + 1].widget, True);
    MenuCheckItem(filledSplinePopup[nm + 1].widget, True);
    MenuCheckItem(selectSplinePopup[nm + 1].widget, True);
    SplineSetMode(nm);
}

void 
OperationSelectCall(int n)
{
    MenuCheckItem(selectBoxPopup[n + 5].widget, True);
    MenuCheckItem(selectOvalPopup[n + 3].widget, True);
    MenuCheckItem(selectFreehandPopup[n + 1].widget, True);
    MenuCheckItem(selectPolygonPopup[n + 6].widget, True);
    MenuCheckItem(selectSplinePopup[n + 5].widget, True);
    SelectSetCutMode(n);
}

static void 
selectPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;
    OperationSelectCall(nm);
}

static void 
fillPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    FillSetMode(nm);
    MenuCheckItem(fillPopup[nm+1].widget, True);
    MenuCheckItem(gradientFillPopup[nm+7].widget, True);
    MenuCheckItem(fractalFillPopup[nm+6].widget, True);
}

static void 
gradientFillPopupCB2(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    GradientFillSetMode(nm);
    MenuCheckItem(w, True);
}

#ifdef FEATURE_FRACTAL
static void 
fractalFillPopupCB2(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;

    FractalFillSetMode(nm);
    MenuCheckItem(w, True);
}

#endif

static void
dynPencilPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    Boolean	m = !DynPencilGetFinish();

    DynPencilSetFinish(m);
    MenuCheckItem(w, m);
}

static void 
sprayPopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    Boolean m = !SprayGetStyle();

    SpraySetStyle(m);
    MenuCheckItem(w, m);
}

static void 
linePopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    int nm = (int)(long)junk;
    Boolean m;

    if (nm == 0) {
         m = !MultiGetStyle();
         MultiSetStyle(m);
         MenuCheckItem(linePopup[1].widget, m);
    }
    if (nm == 1) {
         m = !ArrowGetStyle();
         ArrowSetStyle(m);
         MenuCheckItem(linePopup[2].widget, m);
    }
}

static void 
erasePopupCB(Widget w, XtPointer junk, XtPointer junk2)
{
    Boolean m = !EraseGetMode();

    EraseSetMode(m);
    MenuCheckItem(w, m);
}

/*
**  Done with operation popup menus
 */

#define GENERATE_OP(name)				\
	static OperationPair  CONCAT(name,Op) = {	\
	  CONCAT(name,Add), CONCAT(name,Remove)		\
	};

GENERATE_OP(DotPencil)
GENERATE_OP(Pencil)
GENERATE_OP(DynPencil)
GENERATE_OP(Line)
GENERATE_OP(Brokenline)
GENERATE_OP(Arc)
GENERATE_OP(Arrow)
GENERATE_OP(Erase)
GENERATE_OP(Box)
GENERATE_OP(Oval)
GENERATE_OP(Brush)
GENERATE_OP(Font)
GENERATE_OP(Smear)
GENERATE_OP(Spray)
GENERATE_OP(Polygon)
GENERATE_OP(Freehand)
GENERATE_OP(Spline)
GENERATE_OP(Fill)
GENERATE_OP(GradientFill)
#ifdef FEATURE_FRACTAL
GENERATE_OP(FractalFill)
#endif
GENERATE_OP(FilledBox)
GENERATE_OP(FilledOval)
GENERATE_OP(FilledPolygon)
GENERATE_OP(FilledFreehand)
GENERATE_OP(FilledSpline)
GENERATE_OP(SelectBox)
GENERATE_OP(SelectOval)
GENERATE_OP(SelectFreehand)
GENERATE_OP(SelectPolygon)
GENERATE_OP(SelectSpline)


#define	XPMMAP(name)	(char **) CONCAT(name,Op_xpm)
#define MENU(name)	XtNumber(CONCAT(name,Popup)), CONCAT(name,Popup)

typedef struct {
    char *name;
    char **xpmmap;
    OperationPair *data;
    char *translations;
    int nitems;
    PaintMenuItem *popupMenu;
    Pixmap icon;
} IconListItem;

static Widget iconListWidget;
static IconListItem iconList[] =
{
    /* Vertical arrangement */

    {"pencil", XPMMAP(pencil), &PencilOp,
     NULL, MENU(pencil), None},
    {"dynpencil", XPMMAP(dynPen), &DynPencilOp, 	
     "<BtnDown>(2): changeDynPencil()", MENU(dynPencil), None},
    {"dotPencil", XPMMAP(dotPen), &DotPencilOp,
     NULL, MENU(dotPencil), None},

    {"brush", XPMMAP(brush), &BrushOp,
     "<BtnDown>(2): changeBrush()", MENU(brush), None},
    {"spray", XPMMAP(spray), &SprayOp,
     "<BtnDown>(2): changeSpray()", MENU(spray), None},
    {"smear", XPMMAP(smear), &SmearOp,
     "<BtnDown>(2): changeBrush()", MENU(smear), None},

    {"line", XPMMAP(line), &LineOp,
     "<BtnDown>(2): changeArrow()", MENU(line), None},
    {"brokenline", XPMMAP(brokenline), &BrokenlineOp,
     NULL, MENU(brokenline), None},
    {"arc", XPMMAP(arc), &ArcOp,
     NULL, MENU(arc), None},

    {"arrow", XPMMAP(arrow), &ArrowOp,
     "<BtnDown>(2): changeArrow()", MENU(arrow), None},
    {"text", XPMMAP(text), &FontOp,
     "<BtnDown>(2): changeFont()", MENU(text), None},
    {"erase", XPMMAP(erase), &EraseOp,
     "<BtnDown>(2): changeBrush()", MENU(erase), None},

    {"box", XPMMAP(box), &BoxOp,
     "<BtnDown>(2): changeBoxParam()", MENU(box), None},
    {"filledBox", XPMMAP(filledBox), &FilledBoxOp,
     "<BtnDown>(2): changeBoxParam()", MENU(filledBox), None},
    {"selectBox", XPMMAP(selectBox), &SelectBoxOp,
     "<BtnDown>(2): changeBoxParam()", MENU(selectBox), None},

    {"oval", XPMMAP(oval), &OvalOp,
     NULL, MENU(oval), None},
    {"filledOval", XPMMAP(filledOval), &FilledOvalOp,
     NULL, MENU(filledOval), None},
    {"selectOval", XPMMAP(selectOval), &SelectOvalOp,
     NULL, MENU(selectOval), None},

    {"freehand", XPMMAP(freehand), &FreehandOp,
     NULL, MENU(freehand), None},
    {"filledFreehand", XPMMAP(filledFreehand), &FilledFreehandOp,
     NULL, MENU(freehand), None},
    {"selectFreehand", XPMMAP(selectFreehand), &SelectFreehandOp,
     NULL, MENU(selectFreehand), None},

    {"polygon", XPMMAP(polygon), &PolygonOp,
     "<BtnDown>(2): changePolygonParam()", MENU(polygon), None},
    {"filledPolygon", XPMMAP(filledPolygon), &FilledPolygonOp,
     "<BtnDown>(2): changePolygonParam()", MENU(filledPolygon), None},
    {"selectPolygon", XPMMAP(selectPolygon), &SelectPolygonOp,
     "<BtnDown>(2): changePolygonParam()", MENU(selectPolygon), None},

    {"spline", XPMMAP(spline), &SplineOp,
     NULL, MENU(spline), None},
    {"filledSpline", XPMMAP(filledSpline), &FilledSplineOp,
     NULL, MENU(filledSpline), None},
    {"selectSpline", XPMMAP(selectSpline), &SelectSplineOp,
     NULL, MENU(selectSpline), None},

    {"fill", XPMMAP(fill), &FillOp,
     NULL, MENU(fill), None},
    {"gradientFill", XPMMAP(gradientFill), &GradientFillOp,
     "<BtnDown>(2): changeGradientFillParam()", MENU(gradientFill), None},
#ifdef FEATURE_FRACTAL
    {"fractalFill", XPMMAP(fractalFill), &FractalFillOp,
     "<BtnDown>(2): changeFractalFillParam()", MENU(fractalFill), None},
#endif

    /* Vertical arrangement */
};

int 
getIndexOp()
{
    int i;
    for (i=0; i<XtNumber(iconList); i++)
         if (CurrentOp == iconList[i].data) return i;
    return 0;
}

static void
permuteIcons()
{
#define NUM XtNumber(iconList)
    int i;
    IconListItem tempList[NUM];

    if (ToolsAreHorizontal()) {
    for (i=0; i<NUM; i++)
        tempList[i] = iconList[3*(i%10)+(i/10)];
    for (i=0; i<NUM; i++)
        iconList[i] = tempList[i];
    }
}

void 
OperationSet(String names[], int num)
{
    IconListItem *match = NULL;
    int i, j;

    if (num<0) {
        i = -num-1;
        if (i >= XtNumber(iconList)) i = 0;
        match = &iconList[i];
        if (CurrentOp == match->data) return;
	XawToggleSetCurrent(iconListWidget, (XtPointer) match->name);
        CurrentOp = match->data;
        return;
    }

    for (i = 0; i < XtNumber(iconList); i++) {
	for (j = 0; j < num; j++) {
	    if (strcmp(names[j], iconList[i].name) == 0) {
		if (match == NULL)
		    match = &iconList[i];
		if (CurrentOp == iconList[i].data)
		    return;
	    }
	}
    }
    if (match != NULL) {
	XawToggleSetCurrent(iconListWidget, (XtPointer) match->name);
        CurrentOp = match->data;
    }
}

static PaintMenuItem canvasMenu[] =
{
    MI_SIMPLECB("new", GraphicCreate, 0),
    MI_SIMPLECB("new-size", GraphicCreate, 1),
    MI_SIMPLECB("open", GraphicCreate, 2),
    MI_SIMPLECB("loaded", GraphicCreate, 3),
    MI_SIMPLECB("magnifier", GraphicCreate, 4),
    MI_SIMPLECB("screenshot", takeScreenshot, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("quit", exitPaint, NULL),
};

static PaintMenuItem helpMenu[] =
{
    MI_SIMPLECB("intro", HelpDialog, "introduction"),
    MI_SIMPLECB("tools", HelpDialog, "toolbox.tools"),
    MI_SIMPLECB("canvas", HelpDialog, "canvas"),
    MI_SEPARATOR(),
    MI_SIMPLECB("about", HelpDialog, "about"),
    MI_SIMPLECB("copyright", HelpDialog, "copyright"),
};

static PaintMenuBar menuBar[] =
{
    {None, "canvas", XtNumber(canvasMenu), canvasMenu},
#if 0
    {None, "other", XtNumber(otherMenu), otherMenu},
    {None, "line", XtNumber(lineMenu), lineMenu},
    {None, "font", XtNumber(fontMenu), fontMenu},
#endif
    {None, "help", XtNumber(helpMenu), helpMenu},
};

/*
**  Now for the callback functions
 */
static int argListLen = 0;
static Arg *argList;

void 
OperationSetPaint(Widget paint)
{
    XtSetValues(paint, argList, argListLen);
}

void 
OperationAddArg(Arg arg)
{
    int i;

    for (i = 0; i < argListLen; i++) {
	if (strcmp(argList[i].name, arg.name) == 0) {
	    argList[i].value = arg.value;
	    return;
	}
    }

    if (argListLen == 0)
	argList = (Arg *) XtMalloc(sizeof(Arg) * 2);
    else
	argList = (Arg *) XtRealloc((XtPointer) argList,
				    sizeof(Arg) * (argListLen + 2));

    argList[argListLen++] = arg;
}

/*
**  Double click action callback functions.
 */
void 
changeFontAction(Widget w, XEvent * event)
{
    FontSelect(Global.toplevel, None);
}

static void
changeDynPencilAction(Widget w, XEvent *event)
{
    dynPencilMenuCallback(w);
}

static void 
changeSprayAction(Widget w, XEvent * event)
{
    sprayMenuCallback(w);
}

static void 
changeArrowAction(Widget w, XEvent * event)
{
    arrowHeadMenuCallback(w);
}

static void 
changeGradientFillParamAction(Widget w, XEvent * event)
{
    gradientFillMenuCallback(w);
}

static void 
changeBrushAction(Widget w, XEvent * event)
{
    BrushSelect(w);
}

static void 
changeBrushParamAction(Widget w, XEvent * event)
{
    brushMenuCallback(w);
}

static void 
changeBoxParamAction(Widget w, XEvent * event)
{
    boxMenuCallback(w);
}

static void 
changePolygonParamAction(Widget w, XEvent * event)
{
    polygonMenuCallback(w);
}

static void 
changeFractalFillParamAction(Widget w, XEvent * event)
{
    fractalMenuCallback(w);
}

static void 
switchtoCanvasCallback(Widget w, XtPointer junk, XEvent * event, Boolean * flg)
{
  if (Global.canvas && event->type == ButtonRelease)
       RaiseWindow(XtDisplay(Global.canvas), XtWindow(Global.canvas));
}

void
processEvent(Widget w, XtPointer arg, XEvent * event, Boolean * flg)
{
    KeySym keysym;
    char buf[12];
    int len;

    if (event->type == KeyPress || event->type == KeyRelease) {
        len = XLookupString((XKeyEvent *)event, 
                            buf, sizeof(buf) - 1, &keysym, NULL);
        if (keysym == XK_Escape)
            PopdownMenusGlobal();
        return;
    }

    if (event->type == ButtonPress || event->type == ButtonRelease) {
        if (Global.popped_up) {
	    if (event->type == ButtonRelease)
                PopdownMenusGlobal();
            event->type = None;
            return;
	}
        if (arg) {
            XtVaSetValues(w, XtNstate, True, NULL);
	    setOperation(w, arg, NULL);
	}
    }
}

void
toolsResized(Widget w, XtPointer l, XConfigureEvent * event, Boolean * flg)
{
    Dimension width, height;

    if (event->type != ConfigureNotify) return;

    XtVaGetValues(w, XtNwidth, &width,
		     XtNheight, &height, NULL);

    if (width<40 || height<72)
        XtVaSetValues(w, XtNsensitive, False, NULL);

    XtResizeWidget((Widget)l, (width>40)? width - 8:32 ,
                                  (height>70)? height - 38:32,
#ifdef XAW3D		   
		                  1);
    XtMoveWidget((Widget)l, 4, 35);
#else
		                  0);
    XtMoveWidget((Widget)l, 4, 36);
#endif		
#if defined(XAWPLAIN) || defined(XAW95) || defined(XAW3D)
#ifdef XAW3DG
    if (width>20)
        XtMoveWidget(Global.back, width-20, 8);
#else
    if (width>17)
        XtMoveWidget(Global.back, width-17, 8);
#endif
#else
    if (width>15)
        XtMoveWidget(Global.back, width-15, 8);
#endif
}

/*
**  The real init function
 */
void 
OperationInit(Widget toplevel)
{
    static XtActionsRec acts[] =
    {
      /*	{"changeLine", (XtActionProc) changeLineAction}, */
	{"changeDynPencil", (XtActionProc) changeDynPencilAction},
	{"changeBrush", (XtActionProc) changeBrushAction},
	{"changeSpray", (XtActionProc) changeSprayAction},
	{"changeArrow", (XtActionProc) changeArrowAction},
	{"changeFont", (XtActionProc) changeFontAction},
	{"changeBoxParam", (XtActionProc) changeBoxParamAction},
	{"changePolygonParam", (XtActionProc) changePolygonParamAction},
	{"changeGradientFillParam", (XtActionProc) changeGradientFillParamAction},
	{"changeFractalFillParam", (XtActionProc) changeFractalFillParamAction},
    };
    int i;
    Pixmap pix;
    Widget form, vport, box, icon, firstIcon = None;
    char *defTrans = "\
<Key>Escape: escape()\n\
<BtnDown>,<BtnUp>: set() notify()\n\
";

    XtTranslations trans = XtParseTranslationTable(defTrans);
    IconListItem *cur;
    XpmAttributes attributes;
#ifdef XAW3D
    Pixel bg;
#endif

    form = XtVaCreateManagedWidget("toolbox",
				   formWidgetClass, toplevel,
				   XtNborderWidth, 0,
				   NULL);
    XtAppAddActions(XtWidgetToApplicationContext(toplevel),
		    acts, XtNumber(acts));

    /*
    **  Create the menu bar
     */
    Global.bar = MenuBarCreate(form, XtNumber(menuBar), menuBar);
    XtVaSetValues(Global.bar, 
#ifdef XAW3D
#ifdef XAW3DG
                  XtNhorizDistance, 0,
#else
                  XtNhorizDistance, 1,
#endif
		  XtNvertDistance, 3,
#else
                  XtNhorizDistance, 0,
#endif		  
		  NULL);
    /*
    **  Create the operation icon list
     */
    vport = XtVaCreateManagedWidget("vport",
				    viewportWidgetClass, form,
				    XtNallowVert, True,
				    XtNuseRight, True,
				    XtNfromVert, Global.bar,
				    XtNborderWidth, 0,
#ifdef XAW95
				    XtNvertDistance, 3,
#endif				    	       
#ifdef XAW3D
				    XtNvertDistance, 5,
                                    XtNborderWidth, 0,
#endif				    
				    NULL);

    box = XtVaCreateManagedWidget("box",
				    boxWidgetClass, vport,
#ifdef XAW3D
 			            XtNwidth, 600, XtNheight, 600,
#endif				  
				    XtNtop, XtChainTop,
				    NULL);

    Global.back = XtVaCreateManagedWidget("!", labelWidgetClass, form,
				      XtNwidth, 12,
				      XtNvertDistance, 8,
                                      XtNborderWidth, 1,
                                      XtNsensitive, False,
                                      NULL);

    XtAddEventHandler(Global.back, ButtonPressMask | ButtonReleaseMask, False,
        (XtEventHandler) switchtoCanvasCallback, NULL);

    permuteIcons();

    attributes.valuemask = XpmCloseness;
    attributes.closeness =  40000;

    for (i = 0; i < XtNumber(iconList); i++) {
	cur = &iconList[i];

	icon = XtVaCreateManagedWidget(cur->name,
				       toggleWidgetClass, box,
				       XtNtranslations, trans,
				       XtNradioGroup, firstIcon,
				       XtNradioData, cur->name,
				       NULL);

	if (cur->xpmmap != NULL) {
	    XpmCreatePixmapFromData(XtDisplay(box),
				    DefaultRootWindow(XtDisplay(box)),
				    cur->xpmmap,
				    &pix, NULL, &attributes);
	} else {
	    pix = None;
	}

	if (pix == None) {
	    fprintf(stderr, "%s", msgText[UNABLE_TO_ALLOCATE_SUFFICIENT_COLOR_ENTRIES]);
	    fprintf(stderr, "%s", msgText[TRY_EXITING_OTHER_COLOR_INTENSIVE_APPLICATIONS]);
	    exit(1);
	}
	
	cur->icon = pix;

	if (pix != None)
	    XtVaSetValues(icon, XtNbitmap, pix, NULL);

	if (cur->translations != NULL) {
	    XtTranslations moreTrans;

	    moreTrans = XtParseTranslationTable(cur->translations);
	    XtAugmentTranslations(icon, moreTrans);
	}
	if (firstIcon == NULL) {
	    XtVaSetValues(icon, XtNstate, True, NULL);
	    setOperation(icon, cur->data, NULL);

	    firstIcon = icon;
	    iconListWidget = icon;
	}

        XtAddEventHandler(icon, ButtonPressMask | ButtonReleaseMask |
                          KeyPressMask | KeyReleaseMask, False, 
			  (XtEventHandler) processEvent, cur->data);

	/* 
         *  More sophisticated event handler above
         *  XtAddCallback(icon, XtNcallback, setOperation, cur->data);
         */
#if defined(XAW3D) && !defined(XAW3DG)
#ifdef BIGTOOLICONS
        XtVaSetValues(icon, XtNwidth, 52, XtNheight, 48, XtNborderWidth, 0,
#else
        XtVaSetValues(icon, XtNwidth, 36, XtNheight, 32, XtNborderWidth, 0,
#endif
                            NULL);
#endif				       
#if defined(XAW3D) && defined(XAW3DG)
#ifdef BIGTOOLICONS
	XtResizeWidget(icon, 48, 48, 0);
#else
	XtResizeWidget(icon, 34, 32, 0);
#endif
#endif				       
#ifdef XAW3D
        XtVaGetValues(icon, XtNbackground, &bg, NULL);
        XtVaSetValues(icon, XtNforeground, bg, NULL);
#endif
	if (cur->nitems != 0 && cur->popupMenu != NULL)
	    MenuPopupCreate(icon, "popup-menu", cur->nitems, cur->popupMenu);
    }

    if (ToolsAreHorizontal())
        XtResizeWidget(vport,
#if defined(XAW3D) && !defined(XAW3DG)		      
#ifdef BIGTOOLICONS
	          565, 157, 0);
#else
	          424, 108, 0);
#endif
#endif
#ifdef XAW3DG
#ifdef BIGTOOLICONS
	          529, 149, 0);
#else
	          388, 112, 0);
#endif
#endif
#ifdef XAW95
#ifdef BIGTOOLICONS
	          550, 155, 0);
#else
	          388, 106, 0);
#endif
#endif
#ifdef XAWPLAIN		      
#ifdef BIGTOOLICONS
		  544, 152, 0);
#else
		  386, 102, 0);
#endif
#endif	      
    else
        XtResizeWidget(vport,
#if defined(XAW3D) && !defined(XAW3DG)
#ifdef BIGTOOLICONS
		      173, 521, 0);
#else
		      124, 361, 0);
#endif
#endif
#ifdef XAW3DG
#ifdef BIGTOOLICONS
	              165, 485, 0);
#else
                      122, 364, 0);
#endif
#endif
#ifdef XAW95
#ifdef BIGTOOLICONS
                      172, 503, 0);
#else
                      123, 346, 0);
#endif
#endif
#ifdef XAWPLAIN		      
#ifdef BIGTOOLICONS
                      167, 502, 0);
#else
                      118, 340, 0);
#endif
#endif	      
  
    AddDestroyCallback(toplevel, (DestroyCallbackFunc) exitPaint, NULL);

    XtAddEventHandler(toplevel, StructureNotifyMask, False,
		      (XtEventHandler) toolsResized, (XtPointer) vport);

    XtAddEventHandler(vport, 
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask, False, 
                (XtEventHandler) processEvent, (XtPointer)NULL);

    XtAddEventHandler(toplevel, 
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask, False, 
                (XtEventHandler) processEvent, (XtPointer)NULL);
}

Image *
OperationIconImage(Widget w, char *name)
{
    int i;

    for (i = 0; i < XtNumber(iconList); i++)
	if (strcmp(name, iconList[i].name) == 0)
	    break;
    if (i == XtNumber(iconList) || iconList[i].icon == None)
	return NULL;

    return PixmapToImage(w, iconList[i].icon, None);
}

void setToolIconOnWidget(Widget w)
{
    if (w != None)
        XtVaSetValues(w, XtNbitmap, iconList[getIndexOp()].icon, NULL);
}
