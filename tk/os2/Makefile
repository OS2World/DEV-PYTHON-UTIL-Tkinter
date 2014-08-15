
!ifdef NODEBUG
DEBUG=
!else
DEBUG=-Ti+
!endif

# you can use "DYNAMIC=1" on the make command line to compile a version
# of the libraries that is statically linked
!ifndef STATIC
!ifdef DYNAMIC
STATIC=-Gd+ -DDYNAMICLIBS
!else
STATIC=-Gd-
!endif
!endif

INCLUDE=..\..\tcl\os2;.;$(INCLUDE)
LIB=..\..\tcl\os2;$(LIB)
DEFS=

All:
   $(MAKE) pre
   $(MAKE) bins

pre:

deps:
   if exist Makefile.bak attrib -r Makefile.bak
   $(MDEP) -i INCLUDE $(DEFS)  tkOS2Clipboard.c tkOS2Color.c tkOS2Cursor.c tkOS2Dll.c tkOS2Draw.c tkOS2Font.c tkOS2Image.c tkOS2ImgUtil.c tkOS2Init.c tkOS2Key.c tkOS2Pixmap.c tkOS2Pointer.c tkOS2Region.c tkOS2Window.c tkOS2Wm.c tkOS2X.c tkConsole.c os2Main.c xutil.c stubs.c tkCanvPs.c tkMain.c tkOption.c tkSend.c tkWindow.c tk3d.c tkArgv.c tkAtom.c tkBind.c tkBitmap.c tkButton.c tkCanvArc.c tkCanvBmap.c tkCanvImg.c tkCanvLine.c tkCanvPoly.c tkCanvText.c tkCanvUtil.c tkCanvWind.c tkCanvas.c tkClipboard.c tkCmds.c tkColor.c tkConfig.c tkConsole.c tkCursor.c tkEntry.c tkError.c tkEvent.c tkFocus.c tkFont.c tkFrame.c tkGC.c tkGeometry.c tkGet.c tkGrab.c tkGrid.c tkImage.c tkImgBmap.c tkImgGIF.c tkImgPPM.c tkImgPhoto.c tkListbox.c tkMenu.c tkMenubutton.c tkMessage.c tkPack.c tkPlace.c tkRectOval.c tkScale.c tkScrollbar.c tkSelect.c tkText.c tkTextBTree.c tkTextDisp.c tkTextIndex.c tkTextMark.c tkTextTag.c tkTextWind.c tkTrig.c tkUtil.c tkVisual.c tkXId.c xdraw.c xgc.c ximage.c

clean_targets:
   if exist tk41va.lib del tk41va.lib
   if exist tkos2.lib del tkos2.lib
   if exist wish.exe del wish.exe
   if exist tk41va.dll del tk41va.dll

bins: tk41va.lib tkos2.lib wish.exe tk41va.dll

clean:
   if exist tk41va.lib del tk41va.lib
   if exist tkos2.lib del tkos2.lib
   if exist tkOS2Clipboard.OBJ del tkOS2Clipboard.OBJ
   if exist tkOS2Color.OBJ del tkOS2Color.OBJ
   if exist tkOS2Cursor.OBJ del tkOS2Cursor.OBJ
   if exist tkOS2Dll.OBJ del tkOS2Dll.OBJ
   if exist tkOS2Draw.OBJ del tkOS2Draw.OBJ
   if exist tkOS2Font.OBJ del tkOS2Font.OBJ
   if exist tkOS2Image.OBJ del tkOS2Image.OBJ
   if exist tkOS2ImgUtil.OBJ del tkOS2ImgUtil.OBJ
   if exist tkOS2Init.OBJ del tkOS2Init.OBJ
   if exist tkOS2Key.OBJ del tkOS2Key.OBJ
   if exist tkOS2Pixmap.OBJ del tkOS2Pixmap.OBJ
   if exist tkOS2Pointer.OBJ del tkOS2Pointer.OBJ
   if exist tkOS2Region.OBJ del tkOS2Region.OBJ
   if exist tkOS2Window.OBJ del tkOS2Window.OBJ
   if exist tkOS2Wm.OBJ del tkOS2Wm.OBJ
   if exist tkOS2X.OBJ del tkOS2X.OBJ
   if exist wish.exe del wish.exe
   if exist tkConsole.OBJ del tkConsole.OBJ
   if exist os2Main.OBJ del os2Main.OBJ
   if exist tk41va.dll del tk41va.dll
   if exist xutil.OBJ del xutil.OBJ
   if exist stubs.OBJ del stubs.OBJ
   if exist tkCanvPs.OBJ del tkCanvPs.OBJ
   if exist tkMain.OBJ del tkMain.OBJ
   if exist tkOption.OBJ del tkOption.OBJ
   if exist tkSend.OBJ del tkSend.OBJ
   if exist tkWindow.OBJ del tkWindow.OBJ
   if exist tk3d.OBJ del tk3d.OBJ
   if exist tkArgv.OBJ del tkArgv.OBJ
   if exist tkAtom.OBJ del tkAtom.OBJ
   if exist tkBind.OBJ del tkBind.OBJ
   if exist tkBitmap.OBJ del tkBitmap.OBJ
   if exist tkButton.OBJ del tkButton.OBJ
   if exist tkCanvArc.OBJ del tkCanvArc.OBJ
   if exist tkCanvBmap.OBJ del tkCanvBmap.OBJ
   if exist tkCanvImg.OBJ del tkCanvImg.OBJ
   if exist tkCanvLine.OBJ del tkCanvLine.OBJ
   if exist tkCanvPoly.OBJ del tkCanvPoly.OBJ
   if exist tkCanvText.OBJ del tkCanvText.OBJ
   if exist tkCanvUtil.OBJ del tkCanvUtil.OBJ
   if exist tkCanvWind.OBJ del tkCanvWind.OBJ
   if exist tkCanvas.OBJ del tkCanvas.OBJ
   if exist tkClipboard.OBJ del tkClipboard.OBJ
   if exist tkCmds.OBJ del tkCmds.OBJ
   if exist tkColor.OBJ del tkColor.OBJ
   if exist tkConfig.OBJ del tkConfig.OBJ
   if exist tkConsole.OBJ del tkConsole.OBJ
   if exist tkCursor.OBJ del tkCursor.OBJ
   if exist tkEntry.OBJ del tkEntry.OBJ
   if exist tkError.OBJ del tkError.OBJ
   if exist tkEvent.OBJ del tkEvent.OBJ
   if exist tkFocus.OBJ del tkFocus.OBJ
   if exist tkFont.OBJ del tkFont.OBJ
   if exist tkFrame.OBJ del tkFrame.OBJ
   if exist tkGC.OBJ del tkGC.OBJ
   if exist tkGeometry.OBJ del tkGeometry.OBJ
   if exist tkGet.OBJ del tkGet.OBJ
   if exist tkGrab.OBJ del tkGrab.OBJ
   if exist tkGrid.OBJ del tkGrid.OBJ
   if exist tkImage.OBJ del tkImage.OBJ
   if exist tkImgBmap.OBJ del tkImgBmap.OBJ
   if exist tkImgGIF.OBJ del tkImgGIF.OBJ
   if exist tkImgPPM.OBJ del tkImgPPM.OBJ
   if exist tkImgPhoto.OBJ del tkImgPhoto.OBJ
   if exist tkListbox.OBJ del tkListbox.OBJ
   if exist tkMenu.OBJ del tkMenu.OBJ
   if exist tkMenubutton.OBJ del tkMenubutton.OBJ
   if exist tkMessage.OBJ del tkMessage.OBJ
   if exist tkPack.OBJ del tkPack.OBJ
   if exist tkPlace.OBJ del tkPlace.OBJ
   if exist tkRectOval.OBJ del tkRectOval.OBJ
   if exist tkScale.OBJ del tkScale.OBJ
   if exist tkScrollbar.OBJ del tkScrollbar.OBJ
   if exist tkSelect.OBJ del tkSelect.OBJ
   if exist tkText.OBJ del tkText.OBJ
   if exist tkTextBTree.OBJ del tkTextBTree.OBJ
   if exist tkTextDisp.OBJ del tkTextDisp.OBJ
   if exist tkTextIndex.OBJ del tkTextIndex.OBJ
   if exist tkTextMark.OBJ del tkTextMark.OBJ
   if exist tkTextTag.OBJ del tkTextTag.OBJ
   if exist tkTextWind.OBJ del tkTextWind.OBJ
   if exist tkTrig.OBJ del tkTrig.OBJ
   if exist tkUtil.OBJ del tkUtil.OBJ
   if exist tkVisual.OBJ del tkVisual.OBJ
   if exist tkXId.OBJ del tkXId.OBJ
   if exist xdraw.OBJ del xdraw.OBJ
   if exist xgc.OBJ del xgc.OBJ
   if exist ximage.OBJ del ximage.OBJ
	if exist TK41VA.def del TK41VA.def


tk41va.lib: tk41va.dll
   implib /NOI tk41va.lib tk41va.def

tkos2.lib: tkOS2Clipboard.OBJ tkOS2Color.OBJ tkOS2Cursor.OBJ tkOS2Dll.OBJ tkOS2Draw.OBJ tkOS2Font.OBJ tkOS2Image.OBJ tkOS2ImgUtil.OBJ tkOS2Init.OBJ tkOS2Key.OBJ tkOS2Pixmap.OBJ tkOS2Pointer.OBJ tkOS2Region.OBJ tkOS2Window.OBJ tkOS2Wm.OBJ tkOS2X.OBJ
	-del tkos2.lib
   ilib tkos2.lib tkOS2Clipboard.OBJ tkOS2Color.OBJ tkOS2Cursor.OBJ tkOS2Dll.OBJ tkOS2Draw.OBJ tkOS2Font.OBJ tkOS2Image.OBJ tkOS2ImgUtil.OBJ tkOS2Init.OBJ tkOS2Key.OBJ tkOS2Pixmap.OBJ tkOS2Pointer.OBJ tkOS2Region.OBJ tkOS2Window.OBJ tkOS2Wm.OBJ tkOS2X.OBJ;

tkOS2Clipboard.OBJ: tkOS2Clipboard.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Clipboard.c

tkOS2Color.OBJ: tkOS2Color.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Color.c

tkOS2Cursor.OBJ: tkOS2Cursor.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Cursor.c

tkOS2Dll.OBJ: tkOS2Dll.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Dll.c

tkOS2Draw.OBJ: tkOS2Draw.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Draw.c

tkOS2Font.OBJ: tkOS2Font.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Font.c

tkOS2Image.OBJ: tkOS2Image.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Image.c

tkOS2ImgUtil.OBJ: tkOS2ImgUtil.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2ImgUtil.c

tkOS2Init.OBJ: tkOS2Init.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Init.c

tkOS2Key.OBJ: tkOS2Key.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Key.c

tkOS2Pixmap.OBJ: tkOS2Pixmap.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Pixmap.c

tkOS2Pointer.OBJ: tkOS2Pointer.c
	echo %%INCLUDE%%
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Pointer.c

tkOS2Region.OBJ: tkOS2Region.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Region.c

tkOS2Window.OBJ: tkOS2Window.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Window.c

tkOS2Wm.OBJ: tkOS2Wm.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2Wm.c

tkOS2X.OBJ: tkOS2X.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkOS2X.c

wish.exe: tkConsole.OBJ os2Main.OBJ tk41va.lib
   icc $(DEBUG) $(LINKOPTS) -B"/PM:PM"  -Gm+ $(STATIC) -Fewish.exe tkConsole.OBJ os2Main.OBJ tk41va.lib tcl75va.lib

tkConsole.OBJ: tkConsole.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tkConsole.c

os2Main.OBJ: os2Main.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  os2Main.c

tk41va.dll: tkos2.lib xutil.OBJ stubs.OBJ tkCanvPs.OBJ tkMain.OBJ tkOption.OBJ tkSend.OBJ tkWindow.OBJ tk3d.OBJ tkArgv.OBJ tkAtom.OBJ tkBind.OBJ tkBitmap.OBJ tkButton.OBJ tkCanvArc.OBJ tkCanvBmap.OBJ tkCanvImg.OBJ tkCanvLine.OBJ tkCanvPoly.OBJ tkCanvText.OBJ tkCanvUtil.OBJ tkCanvWind.OBJ tkCanvas.OBJ tkClipboard.OBJ tkCmds.OBJ tkColor.OBJ tkConfig.OBJ tkConsole.OBJ tkCursor.OBJ tkEntry.OBJ tkError.OBJ tkEvent.OBJ tkFocus.OBJ tkFont.OBJ tkFrame.OBJ tkGC.OBJ tkGeometry.OBJ tkGet.OBJ tkGrab.OBJ tkGrid.OBJ tkImage.OBJ tkImgBmap.OBJ tkImgGIF.OBJ tkImgPPM.OBJ tkImgPhoto.OBJ tkListbox.OBJ tkMenu.OBJ tkMenubutton.OBJ tkMessage.OBJ tkPack.OBJ tkPlace.OBJ tkRectOval.OBJ tkScale.OBJ tkScrollbar.OBJ tkSelect.OBJ tkText.OBJ tkTextBTree.OBJ tkTextDisp.OBJ tkTextIndex.OBJ tkTextMark.OBJ tkTextTag.OBJ tkTextWind.OBJ tkTrig.OBJ tkUtil.OBJ tkVisual.OBJ tkXId.OBJ xdraw.OBJ xgc.OBJ ximage.OBJ
   echo LIBRARY TK41VA INITINSTANCE TERMINSTANCE >TK41VA.def
   echo PROTMODE >>TK41VA.def
   echo DATA MULTIPLE NONSHARED READWRITE LOADONCALL >>TK41VA.def
   echo CODE LOADONCALL >>TK41VA.def
   echo EXPORTS >>TK41VA.def
   cppfilt -B -P -X  xutil.OBJ stubs.OBJ tkCanvPs.OBJ tkMain.OBJ tkOption.OBJ tkSend.OBJ tkWindow.OBJ tk3d.OBJ tkArgv.OBJ tkAtom.OBJ tkBind.OBJ tkBitmap.OBJ tkButton.OBJ tkCanvArc.OBJ tkCanvBmap.OBJ tkCanvImg.OBJ tkCanvLine.OBJ tkCanvPoly.OBJ tkCanvText.OBJ tkCanvUtil.OBJ tkCanvWind.OBJ tkCanvas.OBJ tkClipboard.OBJ tkCmds.OBJ tkColor.OBJ tkConfig.OBJ tkConsole.OBJ tkCursor.OBJ tkEntry.OBJ tkError.OBJ tkEvent.OBJ tkFocus.OBJ tkFont.OBJ tkFrame.OBJ tkGC.OBJ tkGeometry.OBJ tkGet.OBJ tkGrab.OBJ tkGrid.OBJ tkImage.OBJ tkImgBmap.OBJ tkImgGIF.OBJ tkImgPPM.OBJ tkImgPhoto.OBJ tkListbox.OBJ tkMenu.OBJ tkMenubutton.OBJ tkMessage.OBJ tkPack.OBJ tkPlace.OBJ tkRectOval.OBJ tkScale.OBJ tkScrollbar.OBJ tkSelect.OBJ tkText.OBJ tkTextBTree.OBJ tkTextDisp.OBJ tkTextIndex.OBJ tkTextMark.OBJ tkTextTag.OBJ tkTextWind.OBJ tkTrig.OBJ tkUtil.OBJ tkVisual.OBJ tkXId.OBJ xdraw.OBJ xgc.OBJ ximage.OBJ >>TK41VA.def
   icc $(DEBUG) $(LINKOPTS) -B"/NOE" -Ge- -Gm+ $(STATIC) -Fetk41va.dll tkos2.lib tcl75va.lib xutil.OBJ stubs.OBJ tkCanvPs.OBJ tkMain.OBJ tkOption.OBJ tkSend.OBJ tkWindow.OBJ tk3d.OBJ tkArgv.OBJ tkAtom.OBJ tkBind.OBJ tkBitmap.OBJ tkButton.OBJ tkCanvArc.OBJ tkCanvBmap.OBJ tkCanvImg.OBJ tkCanvLine.OBJ tkCanvPoly.OBJ tkCanvText.OBJ tkCanvUtil.OBJ tkCanvWind.OBJ tkCanvas.OBJ tkClipboard.OBJ tkCmds.OBJ tkColor.OBJ tkConfig.OBJ tkConsole.OBJ tkCursor.OBJ tkEntry.OBJ tkError.OBJ tkEvent.OBJ tkFocus.OBJ tkFont.OBJ tkFrame.OBJ tkGC.OBJ tkGeometry.OBJ tkGet.OBJ tkGrab.OBJ tkGrid.OBJ tkImage.OBJ tkImgBmap.OBJ tkImgGIF.OBJ tkImgPPM.OBJ tkImgPhoto.OBJ tkListbox.OBJ tkMenu.OBJ tkMenubutton.OBJ tkMessage.OBJ tkPack.OBJ tkPlace.OBJ tkRectOval.OBJ tkScale.OBJ tkScrollbar.OBJ tkSelect.OBJ tkText.OBJ tkTextBTree.OBJ tkTextDisp.OBJ tkTextIndex.OBJ tkTextMark.OBJ tkTextTag.OBJ tkTextWind.OBJ tkTrig.OBJ tkUtil.OBJ tkVisual.OBJ tkXId.OBJ xdraw.OBJ xgc.OBJ ximage.OBJ TK41VA.def

xutil.OBJ: xutil.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  xutil.c

stubs.OBJ: stubs.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  stubs.c

tkCanvPs.OBJ: tkCanvPs.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvPs.c

tkMain.OBJ: tkMain.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkMain.c

tkOption.OBJ: tkOption.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkOption.c

tkSend.OBJ: tkSend.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkSend.c

tkWindow.OBJ: tkWindow.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkWindow.c

tk3d.OBJ: tk3d.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tk3d.c

tkArgv.OBJ: tkArgv.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkArgv.c

tkAtom.OBJ: tkAtom.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkAtom.c

tkBind.OBJ: tkBind.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkBind.c

tkBitmap.OBJ: tkBitmap.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkBitmap.c

tkButton.OBJ: tkButton.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkButton.c

tkCanvArc.OBJ: tkCanvArc.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvArc.c

tkCanvBmap.OBJ: tkCanvBmap.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvBmap.c

tkCanvImg.OBJ: tkCanvImg.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvImg.c

tkCanvLine.OBJ: tkCanvLine.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvLine.c

tkCanvPoly.OBJ: tkCanvPoly.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvPoly.c

tkCanvText.OBJ: tkCanvText.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvText.c

tkCanvUtil.OBJ: tkCanvUtil.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvUtil.c

tkCanvWind.OBJ: tkCanvWind.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvWind.c

tkCanvas.OBJ: tkCanvas.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCanvas.c

tkClipboard.OBJ: tkClipboard.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkClipboard.c

tkCmds.OBJ: tkCmds.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCmds.c

tkColor.OBJ: tkColor.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkColor.c

tkConfig.OBJ: tkConfig.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkConfig.c

tkConsole.OBJ: tkConsole.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkConsole.c

tkCursor.OBJ: tkCursor.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkCursor.c

tkEntry.OBJ: tkEntry.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkEntry.c

tkError.OBJ: tkError.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkError.c

tkEvent.OBJ: tkEvent.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkEvent.c

tkFocus.OBJ: tkFocus.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkFocus.c

tkFont.OBJ: tkFont.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkFont.c

tkFrame.OBJ: tkFrame.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkFrame.c

tkGC.OBJ: tkGC.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkGC.c

tkGeometry.OBJ: tkGeometry.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkGeometry.c

tkGet.OBJ: tkGet.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkGet.c

tkGrab.OBJ: tkGrab.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkGrab.c

tkGrid.OBJ: tkGrid.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkGrid.c

tkImage.OBJ: tkImage.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkImage.c

tkImgBmap.OBJ: tkImgBmap.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkImgBmap.c

tkImgGIF.OBJ: tkImgGIF.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkImgGIF.c

tkImgPPM.OBJ: tkImgPPM.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkImgPPM.c

tkImgPhoto.OBJ: tkImgPhoto.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkImgPhoto.c

tkListbox.OBJ: tkListbox.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkListbox.c

tkMenu.OBJ: tkMenu.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkMenu.c

tkMenubutton.OBJ: tkMenubutton.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkMenubutton.c

tkMessage.OBJ: tkMessage.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkMessage.c

tkPack.OBJ: tkPack.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkPack.c

tkPlace.OBJ: tkPlace.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkPlace.c

tkRectOval.OBJ: tkRectOval.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkRectOval.c

tkScale.OBJ: tkScale.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkScale.c

tkScrollbar.OBJ: tkScrollbar.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkScrollbar.c

tkSelect.OBJ: tkSelect.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkSelect.c

tkText.OBJ: tkText.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkText.c

tkTextBTree.OBJ: tkTextBTree.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTextBTree.c

tkTextDisp.OBJ: tkTextDisp.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTextDisp.c

tkTextIndex.OBJ: tkTextIndex.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTextIndex.c

tkTextMark.OBJ: tkTextMark.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTextMark.c

tkTextTag.OBJ: tkTextTag.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTextTag.c

tkTextWind.OBJ: tkTextWind.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTextWind.c

tkTrig.OBJ: tkTrig.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkTrig.c

tkUtil.OBJ: tkUtil.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkUtil.c

tkVisual.OBJ: tkVisual.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkVisual.c

tkXId.OBJ: tkXId.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tkXId.c

xdraw.OBJ: xdraw.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  xdraw.c

xgc.OBJ: xgc.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  xgc.c

ximage.OBJ: ximage.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  ximage.c

