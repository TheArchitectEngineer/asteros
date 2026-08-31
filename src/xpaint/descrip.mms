#*****************************************************************************
#                                                                            *
# Make file for VMS                                                          *
# Author : J.Jansen (joukj@hrem.stm.tudelft.nl)                              *
# Date : 10 May 2004                                                         *
#                                                                            *
#*****************************************************************************

.first
	define xaw_incdir xaw3d

.c.obj :
	copy vms.h,$(MMS$TARGET_NAME).c $(MMS$TARGET_NAME).c_
	pipe gsed -e "s/X11\/Xaw/Xaw3d/" -e "s/X11\/extensions/extensions/"\
	$(MMS$TARGET_NAME).c_ > $(MMS$TARGET_NAME).c__
	cc$(CFLAGS) $(MMS$TARGET_NAME).c__
	delete $(MMS$TARGET_NAME).c_;*
	delete $(MMS$TARGET_NAME).c__;*

CFLAGS=/name=(as_is,short)/float=ieee

XPSRC = chroma.c color.c colorEdit.c dialog.c fatBitsEdit.c \
	fileName.c fontSelect.c grab.c graphic.c hash.c help.c \
	image.c imageComp.c iprocess.c main.c menu.c misc.c \
	operation.c palette.c pattern.c print.c protocol.c readRC.c size.c \
	snapshot.c text.c texture.c typeConvert.c 

XPOBJ = chroma.obj,color.obj,colorEdit.obj,dialog.obj,fatBitsEdit.obj,\
	fileName.obj,fontSelect.obj,grab.obj,graphic.obj,hash.obj,help.obj,\
	image.obj,imageComp.obj,iprocess.obj,main.obj,menu.obj,misc.obj,\
	operation.obj,palette.obj,pattern.obj,print.obj,protocol.obj,readRC.obj,size.obj,\
	snapshot.obj,text.obj,texture.obj,typeConvert.obj

OPSRC = arcOp.c blobOp.c boxOp.c brushOp.c circleOp.c fillOp.c \
	fontOp.c lineOp.c pencilOp.c polyOp.c splineOp.c selectOp.c \
	sprayOp.c dynPenOp.c

OPOBJ = arcOp.obj,blobOp.obj,boxOp.obj,brushOp.obj,circleOp.obj,fillOp.obj,\
	fontOp.obj,lineOp.obj,pencilOp.obj,polyOp.obj,splineOp.obj,selectOp.obj,\
	sprayOp.obj,dynPenOp.obj

XPWIDSRC = Colormap.c Paint.c PaintEvent.c PaintRegion.c PaintUndo.c
XPWIDOBJ = Colormap.obj,Paint.obj,PaintEvent.obj,PaintRegion.obj,PaintUndo.obj

RWSRC =	[.rw]libpnmrw.c [.rw]readGIF.c [.rw]readJPEG.c [.rw]readTIFF.c \
	[.rw]readWritePNM.c [.rw]readWriteSGI.c [.rw]readWriteXBM.c \
	[.rw]readWriteXPM.c [.rw]readWriteXWD.c [.rw]rwTable.c \
	[.rw]writeGIF.c [.rw]writeJPEG.c [.rw]writePS.c [.rw]writeTIFF.c \
	[.rw]libpnmrw.h [.rw]rwTable.h 
RWOBJ =	[.rw]libpnmrw.obj,[.rw]readGIF.obj,[.rw]readJPEG.obj,[.rw]readTIFF.obj,\
	[.rw]readWritePNM.obj,[.rw]readWriteSGI.obj,[.rw]readWriteXBM.obj,\
	[.rw]readWriteXPM.obj,[.rw]readWriteXWD.obj,[.rw]rwTable.obj,\
	[.rw]writeGIF.obj,[.rw]writePS.obj,[.rw]writeTIFF.obj

SRCS = $(XPSRC) $(OPSRC) $(XPWIDSRC)
OBJS = $(XPOBJ),$(OPOBJ),$(XPWIDOBJ)

HDRS =	Colormap.h ColormapP.h hash.h Paint.h \
	PaintP.h palette.h xpaint.h menu.h \
	text.h region.h image.h patchlevel.h \
	misc.h rc.h ops.h color.h graphic.h operation.h protocol.h 

BRBDIR = [.bitmaps.brushes]
BRUSHBITMAPS = $(BRBDIR)paintA.xpm \
	$(BRBDIR)paintB.xpm $(BRBDIR)paintC.xpm $(BRBDIR)paintD.xpm \
	$(BRBDIR)paintE.xpm $(BRBDIR)paintF.xpm $(BRBDIR)paintG.xpm \
	$(BRBDIR)paintH.xpm $(BRBDIR)paintI.xpm $(BRBDIR)paintJ.xpm \
	$(BRBDIR)paintK.xpm $(BRBDIR)paintL.xpm $(BRBDIR)paintM.xpm \
	$(BRBDIR)paintN.xpm $(BRBDIR)paintO.xpm $(BRBDIR)paintP.xpm \
	$(BRBDIR)paintQ.xpm $(BRBDIR)paintR.xpm $(BRBDIR)paintS.xpm \
	$(BRBDIR)paintT.xpm

OPBDIR = [.bitmaps.big_tools]
OPBITMAPS = $(OPBDIR)clineOp.xpm $(OPBDIR)lassoOp.xpm $(OPBDIR)rayOp.xpm \
	$(OPBDIR)selpolyOp.xpm $(OPBDIR)selectOp.xpm $(OPBDIR)arcOp.xpm \
	$(OPBDIR)boxOp.xpm $(OPBDIR)brushOp.xpm \
	$(OPBDIR)dotPenOp.xpm $(OPBDIR)eraseOp.xpm $(OPBDIR)fboxOp.xpm \
	$(OPBDIR)ffreehandOp.xpm $(OPBDIR)fillOp.xpm $(OPBDIR)fovalOp.xpm \
	$(OPBDIR)fpolyOp.xpm $(OPBDIR)freehandOp.xpm $(OPBDIR)lineOp.xpm \
	$(OPBDIR)ovalOp.xpm $(OPBDIR)pencilOp.xpm $(OPBDIR)polyOp.xpm \
	$(OPBDIR)smearOp.xpm $(OPBDIR)sprayOp.xpm $(OPBDIR)textOp.xpm \
	$(OPBDIR)tfillOp.xpm $(OPBDIR)dynPenOp.xpm

XBMDIR = [.bitmaps.xbm]

all :
	set def [.rw]
	$(MMS)
	set def [-]
	$(MMS) xpaint.exe
	@ write sys$output "all done"

xpaint.exe : $(OBJS) rw.olb link.opt
	link/exec=xpaint.exe $(OBJS),rw/lib,link/opt

Colormap.obj : Colormap.c vms.h ColormapP.h Colormap.h
Paint.obj : Paint.c vms.h PaintP.h Paint.h xpaint.h misc.h
PaintEvent.obj : PaintEvent.c vms.h PaintP.h Paint.h xpaint.h
PaintRegion.obj : PaintRegion.c vms.h PaintP.h Paint.h protocol.h
PaintUndo.obj : PaintUndo.c vms.h xpaint.h misc.h PaintP.h Paint.h
blobOp.obj : blobOp.c vms.h xpaint.h misc.h Paint.h ops.h
boxOp.obj : boxOp.c vms.h xpaint.h misc.h Paint.h ops.h
brushOp.obj : brushOp.c vms.h xpaint.h misc.h Paint.h palette.h \
	graphic.h protocol.h ops.h $(BRUSHBITMAPS)
circleOp.obj : circleOp.c vms.h xpaint.h misc.h Paint.h ops.h
chroma.obj : chroma.c vms.h xpaint.h Paint.h palette.h protocol.h color.h messages.h \
	 misc.h operation.h ops.h
color.obj : color.c vms.h messages.h palette.h protocol.h color.h xpaint.h misc.h image.h
colorEdit.obj : colorEdit.c vms.h misc.h palette.h color.h protocol.h
dialog.obj : dialog.c vms.h misc.h xpaint.h protocol.h
fatBitsEdit.obj : fatBitsEdit.c vms.h Paint.h xpaint.h messages.h palette.h menu.h misc.h region.h \
	protocol.h graphic.h
fileName.obj : fileName.c vms.h Paint.h messages.h misc.h image.h [.rw]rwTable.h graphic.h \
	protocol.h
fontSelect.obj : fontSelect.c vms.h xpaint.h messages.h misc.h operation.h ops.h graphic.h protocol.h
grab.obj : grab.c vms.h image.h
graphic.obj : graphic.c vms.h xpaint.h palette.h messages.h misc.h menu.h text.h graphic.h \
	image.h region.h operation.h rc.h protocol.h color.h [.rw]rwTable.h
hash.obj : hash.c vms.h misc.h hash.h
help.obj : help.c vms.h Paint.h misc.h protocol.h
image.obj : image.c vms.h image.h hash.h palette.h misc.h protocol.h
imageComp.obj : imageComp.c vms.h image.h hash.h protocol.h misc.h
iprocess.obj : iprocess.c vms.h xpaint.h image.h misc.h protocol.h graphic.h
lineOp.obj : lineOp.c vms.h xpaint.h misc.h Paint.h ops.h
main.obj : main.c vms.h XPaint.ad.h messages.h misc.h graphic.h protocol.h [.rw]rwTable.h\
	XPaintIcon.xpm
menu.obj : menu.c vms.h menu.h $(XBMDIR)checkmark.xbm $(XBMDIR)pullright.xbm
misc.obj : misc.c vms.h $(XBMDIR)background.xbm xpaint.h misc.h messages.h
operation.obj : operation.c vms.h $(OPBITMAPS) Local.config ops.h xpaint.h misc.h \
	menu.h Paint.h text.h graphic.h image.h operation.h protocol.h \
	region.h messages.h
	copy vms.h,operation.c operation.c_
	pipe gsed -e "s/X11\/Xaw/Xaw3d/"\
	-e "s/bitmaps\/tools\//[.bitmaps.big_tools]/" operation.c_ > \
	operation.c__
	cc$(CFLAGS)/include=$(OPBDIR) operation.c__
	delete operation.c_;*
	delete operation.c__;*
palette.obj : palette.c vms.h messages.h palette.h hash.h misc.h image.h xpaint.h
pattern.obj : pattern.c vms.h Colormap.h Paint.h palette.h xpaint.h menu.h misc.h image.h \
	region.h text.h graphic.h operation.h color.h protocol.h messages.h
pencilOp.obj : pencilOp.c vms.h xpaint.h Paint.h misc.h ops.h
print.obj : print.c vms.h xpaint.h menu.h image.h messages.h misc.h region.h text.h \
	graphic.h operation.h color.h protocol.h
protocol.obj : protocol.c vms.h xpaint.h messages.h misc.h protocol.h \
	$(XBMDIR)wait1.xbm $(XBMDIR)wait2.xbm \
	$(XBMDIR)wait3.xbm $(XBMDIR)wait4.xbm
readRC.obj : readRC.c vms.h image.h rc.h misc.h DefaultRC.txt.h
size.obj : size.c vms.h Paint.h messages.h misc.h text.h
snapshot.obj : snapshot.c vms.h Paint.h image.h messages.h
text.obj : text.c vms.h misc.h protocol.h text.h
typeConvert.obj : typeConvert.c vms.h palette.h misc.h
fontOp.obj : fontOp.c vms.h xpaint.h Paint.h graphic.h misc.h ops.h
arcOp.obj : arcOp.c vms.h xpaint.h Paint.h misc.h ops.h
polyOp.obj : polyOp.c vms.h xpaint.h misc.h Paint.h ops.h
fillOp.obj : fillOp.c vms.h Paint.h protocol.h xpaint.h ops.h image.h misc.h palette.h
selectOp.obj : selectOp.c vms.h xpaint.h Paint.h protocol.h palette.h color.h misc.h \
	operation.h ops.h region.h
sprayOp.obj : sprayOp.c vms.h xpaint.h Paint.h misc.h ops.h

texture.obj : texture.c vms.h

splineOp.obj : splineOp.c vms.h

dynPenOp.obj : dynPenOp.c vms.h

messages.h : preproc.exe [.share.messages]Messages.
	pipe run preproc > messages.h

preproc.exe : preproc.obj
	link preproc.obj

preproc.obj : preproc.c
	cc $(CFLAGS) preproc.c

XPaint.ad : substads.exe [.app-defaults]XPaint_ad.in\
	[.app-defaults]XPaint_fr_ad.in
	set def [.app-defaults]
	mc [-]substads.exe -appdefs \
	            XPAINT_VERSION "2.8.0" \
		    XPAINT_SHAREDIR "XPAINT$SHARE" \
		    XPAINT_PRINT_COMMAND "PRINT" \
		    XPAINT_POSTSCRIPT_VIEWER "gs" \
		    XPAINT_EXTERN_VIEWER "XPAINT$VIEWER"
	set def [-]
	copy [.app-defaults.out]XPaint. []XPaint.ad

substads.exe : substads.obj
	link substads.obj

substads.obj : substads.c
	cc $(CFLAGS) substads.c

XPaint.ad.h : XPaint.ad substads.exe
	mc []substads -ad2c XPaint.ad XPaint.ad.h

[.app-defaults]xpaint_ad.in : [.app-defaults]XPaint.ad.in
	set file/enter=[.app-defaults]xpaint_ad.in [.app-defaults]XPaint^.ad.in

[.app-defaults]xpaint_fr_ad.in : [.app-defaults]XPaint_fr.ad.in
	set file/enter=[.app-defaults]xpaint_fr_ad.in [.app-defaults]XPaint_fr^.ad.in

DefaultRC.txt.h : DefaultRC. substads.exe
	mc []substads -ad2c DefaultRC. DefaultRC.txt.h
