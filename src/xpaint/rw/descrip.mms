#*****************************************************************************
#                                                                            *
# Make file for VMS                                                          *
# Author : J.Jansen (joukj@hrem.stm.tudelft.nl)                              *
# Date : 11 May 2004                                                         *
#                                                                            *
#*****************************************************************************

.c.obj :
	copy [-]vms.h,[]$(MMS$TARGET_NAME).c []$(MMS$TARGET_NAME).c_
	cc$(CFLAGS) $(MMS$TARGET_NAME).c_
	delete $(MMS$TARGET_NAME).c_;*

CFLAGS=/name=(as_is,short)/float=ieee/define=("HAVE_TIFF=1","HAVE_JPEG=1",\
	"HAVE_PNG=1")/include=([],[-])

TIFF_SRC = writeTIFF.c readTIFF.c
TIFF_OBJ = writeTIFF.obj,readTIFF.obj
JPEG_SRC = readJPEG.c writeJPEG.c
JPEG_OBJ = readJPEG.obj,writeJPEG.obj
PNG_SRC = readWritePNG.c
PNG_OBJ = readWritePNG.obj
XPM_SRC = readWriteXPM.c
XPM_OBJ = readWriteXPM.obj
SGI_SRC =
SGI_OBJ =

SRCS = rwTable.c readWriteICO.c readScriptC.c readWriteBMP.c\
	readWriteXBM.c readWritePNM.c readWriteXWD.c readWritePS.c \
	readGIF.c writeGIF.c $(XPM_SRC) $(TIFF_SRC) $(SGI_SRC) \
	$(JPEG_SRC) $(PNG_SRC) libpnmrw.c 
OBJS = rwTable.obj,readWriteICO.obj,readScriptC.obj,readWriteBMP.obj,\
	readWriteXBM.obj,readWritePNM.obj,readWriteXWD.obj,readWritePS.obj,\
	readGIF.obj,writeGIF.obj,$(XPM_OBJ),$(TIFF_OBJ),$(SGI_OBJ)\
	$(JPEG_OBJ),$(PNG_OBJ),libpnmrw.obj

HDRS = libpnmrw.h rwTable.h XWDFile.h [-]vms.h

all : $(OBJS)
	library/create [-]rw.olb $(OBJS)
	@ write sys$output "all done"

writeTIFF.obj : writeTIFF.c $(HDRS)
readTIFF.obj : readTIFF.c $(HDRS)
readJPEG.obj : readJPEG.c $(HDRS)
writeJPEG.obj : writeJPEG.c $(HDRS)
readWritePNG.obj : readWritePNG.c $(HDRS)
readWriteXPM.obj : readWriteXPM.c $(HDRS)
rwTable.obj : rwTable.c $(HDRS)
readWriteICO.obj : readWriteICO.c $(HDRS)
readScriptC.obj : readScriptC.c $(HDRS)
readWriteXBM.obj : readWriteXBM.c $(HDRS)
readWritePNM.obj : readWritePNM.c $(HDRS)
readWriteXWD.obj : readWriteXWD.c $(HDRS)
readWritePS.obj : readWritePS.c $(HDRS)
readGIF.obj : readGIF.c $(HDRS)
writeGIF.obj : writeGIF.c $(HDRS)
libpnmrw.obj : libpnmrw.c $(HDRS)
readWriteBMP.obj : readWriteBMP.c

XWDFile.h : xwdfile.h_vms
	copy xwdfile.h_vms XWDFile.h
