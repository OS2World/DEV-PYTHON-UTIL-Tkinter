
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

All:
   $(MAKE) pre
   $(MAKE) bins

pre:

deps:
   if exist Makefile.bak attrib -r Makefile.bak
   $(MDEP) -i INCLUDE $(DEFS)  tclOS2Chan.c tclOS2Console.c tclOS2Dll.c tclOS2Env.c tclOS2Error.c tclOS2File.c tclOS2FileName.c tclOS2Init.c tclOS2Load.c tclOS2Main.c tclOS2NoSock.c tclOS2Notify.c tclOS2Pipe.c tclOS2Test.c tclOS2Time.c tclOS2Util.c tclTest.c panic.c regexp.c tclAsync.c tclBasic.c tclCkalloc.c tclClock.c tclCmdAH.c tclCmdIL.c tclCmdMZ.c tclDate.c tclEnv.c tclEvent.c tclExpr.c tclFHandle.c tclGet.c tclHash.c tclHistory.c tclInterp.c tclIO.c tclIOCmd.c tclIOSock.c tclIOUtil.c tclLink.c tclLoad.c tclNotify.c tclParse.c tclPkg.c tclPosixStr.c tclPreserve.c tclProc.c tclUtil.c tclVar.c unixlib.c tclAppInit.c

clean_targets:
   if exist tcl75va.dll del tcl75va.dll
   if exist tcl75va.lib del tcl75va.lib
   if exist tclsh.exe del tclsh.exe

bins: tcl75va.dll tcl75va.lib tclsh.exe

clean:
   if exist tcl75va.dll del tcl75va.dll
   if exist tclOS2Chan.OBJ del tclOS2Chan.OBJ
   if exist tclOS2Console.OBJ del tclOS2Console.OBJ
   if exist tclOS2Dll.OBJ del tclOS2Dll.OBJ
   if exist tclOS2Env.OBJ del tclOS2Env.OBJ
   if exist tclOS2Error.OBJ del tclOS2Error.OBJ
   if exist tclOS2File.OBJ del tclOS2File.OBJ
   if exist tclOS2FileName.OBJ del tclOS2FileName.OBJ
   if exist tclOS2Init.OBJ del tclOS2Init.OBJ
   if exist tclOS2Load.OBJ del tclOS2Load.OBJ
   if exist tclOS2Main.OBJ del tclOS2Main.OBJ
   if exist tclOS2NoSock.OBJ del tclOS2NoSock.OBJ
   if exist tclOS2Notify.OBJ del tclOS2Notify.OBJ
   if exist tclOS2Pipe.OBJ del tclOS2Pipe.OBJ
   if exist tclOS2Test.OBJ del tclOS2Test.OBJ
   if exist tclOS2Time.OBJ del tclOS2Time.OBJ
   if exist tclOS2Util.OBJ del tclOS2Util.OBJ
   if exist tclTest.OBJ del tclTest.OBJ
   if exist panic.OBJ del panic.OBJ
   if exist regexp.OBJ del regexp.OBJ
   if exist tclAsync.OBJ del tclAsync.OBJ
   if exist tclBasic.OBJ del tclBasic.OBJ
   if exist tclCkalloc.OBJ del tclCkalloc.OBJ
   if exist tclClock.OBJ del tclClock.OBJ
   if exist tclCmdAH.OBJ del tclCmdAH.OBJ
   if exist tclCmdIL.OBJ del tclCmdIL.OBJ
   if exist tclCmdMZ.OBJ del tclCmdMZ.OBJ
   if exist tclDate.OBJ del tclDate.OBJ
   if exist tclEnv.OBJ del tclEnv.OBJ
   if exist tclEvent.OBJ del tclEvent.OBJ
   if exist tclExpr.OBJ del tclExpr.OBJ
   if exist tclFHandle.OBJ del tclFHandle.OBJ
   if exist tclGet.OBJ del tclGet.OBJ
   if exist tclHash.OBJ del tclHash.OBJ
   if exist tclHistory.OBJ del tclHistory.OBJ
   if exist tclInterp.OBJ del tclInterp.OBJ
   if exist tclIO.OBJ del tclIO.OBJ
   if exist tclIOCmd.OBJ del tclIOCmd.OBJ
   if exist tclIOSock.OBJ del tclIOSock.OBJ
   if exist tclIOUtil.OBJ del tclIOUtil.OBJ
   if exist tclLink.OBJ del tclLink.OBJ
   if exist tclLoad.OBJ del tclLoad.OBJ
   if exist tclNotify.OBJ del tclNotify.OBJ
   if exist tclParse.OBJ del tclParse.OBJ
   if exist tclPkg.OBJ del tclPkg.OBJ
   if exist tclPosixStr.OBJ del tclPosixStr.OBJ
   if exist tclPreserve.OBJ del tclPreserve.OBJ
   if exist tclProc.OBJ del tclProc.OBJ
   if exist tclUtil.OBJ del tclUtil.OBJ
   if exist tclVar.OBJ del tclVar.OBJ
   if exist unixlib.OBJ del unixlib.OBJ
   if exist tcl75va.lib del tcl75va.lib
   if exist tclsh.exe del tclsh.exe
   if exist tclAppInit.OBJ del tclAppInit.OBJ
	if exist TCL75VA.def del TCL75VA.def


tcl75va.dll: tclOS2Chan.OBJ tclOS2Console.OBJ tclOS2Dll.OBJ tclOS2Env.OBJ tclOS2Error.OBJ tclOS2File.OBJ tclOS2FileName.OBJ tclOS2Init.OBJ tclOS2Load.OBJ tclOS2Main.OBJ tclOS2NoSock.OBJ tclOS2Notify.OBJ tclOS2Pipe.OBJ tclOS2Test.OBJ tclOS2Time.OBJ tclOS2Util.OBJ tclTest.OBJ panic.OBJ regexp.OBJ tclAsync.OBJ tclBasic.OBJ tclCkalloc.OBJ tclClock.OBJ tclCmdAH.OBJ tclCmdIL.OBJ tclCmdMZ.OBJ tclDate.OBJ tclEnv.OBJ tclEvent.OBJ tclExpr.OBJ tclFHandle.OBJ tclGet.OBJ tclHash.OBJ tclHistory.OBJ tclInterp.OBJ tclIO.OBJ tclIOCmd.OBJ tclIOSock.OBJ tclIOUtil.OBJ tclLink.OBJ tclLoad.OBJ tclNotify.OBJ tclParse.OBJ tclPkg.OBJ tclPosixStr.OBJ tclPreserve.OBJ tclProc.OBJ tclUtil.OBJ tclVar.OBJ unixlib.OBJ
   echo LIBRARY TCL75VA INITINSTANCE TERMINSTANCE >TCL75VA.def
   echo PROTMODE >>TCL75VA.def
   echo DATA MULTIPLE NONSHARED READWRITE LOADONCALL >>TCL75VA.def
   echo CODE LOADONCALL >>TCL75VA.def
   echo EXPORTS >>TCL75VA.def
   cppfilt -B -P -X  tclOS2Chan.OBJ tclOS2Console.OBJ tclOS2Dll.OBJ tclOS2Env.OBJ tclOS2Error.OBJ tclOS2File.OBJ tclOS2FileName.OBJ tclOS2Init.OBJ tclOS2Load.OBJ tclOS2Main.OBJ tclOS2NoSock.OBJ tclOS2Notify.OBJ tclOS2Pipe.OBJ tclOS2Test.OBJ tclOS2Time.OBJ tclOS2Util.OBJ tclTest.OBJ panic.OBJ regexp.OBJ tclAsync.OBJ tclBasic.OBJ tclCkalloc.OBJ tclClock.OBJ tclCmdAH.OBJ tclCmdIL.OBJ tclCmdMZ.OBJ tclDate.OBJ tclEnv.OBJ tclEvent.OBJ tclExpr.OBJ tclFHandle.OBJ tclGet.OBJ tclHash.OBJ tclHistory.OBJ tclInterp.OBJ tclIO.OBJ tclIOCmd.OBJ tclIOSock.OBJ tclIOUtil.OBJ tclLink.OBJ tclLoad.OBJ tclNotify.OBJ tclParse.OBJ tclPkg.OBJ tclPosixStr.OBJ tclPreserve.OBJ tclProc.OBJ tclUtil.OBJ tclVar.OBJ unixlib.OBJ >>TCL75VA.def
   icc $(DEBUG) -B"/NOE"  -Ge- -Gm+ $(STATIC) -Fetcl75va.dll tclOS2Chan.OBJ tclOS2Console.OBJ tclOS2Dll.OBJ tclOS2Env.OBJ tclOS2Error.OBJ tclOS2File.OBJ tclOS2FileName.OBJ tclOS2Init.OBJ tclOS2Load.OBJ tclOS2Main.OBJ tclOS2NoSock.OBJ tclOS2Notify.OBJ tclOS2Pipe.OBJ tclOS2Test.OBJ tclOS2Time.OBJ tclOS2Util.OBJ tclTest.OBJ panic.OBJ regexp.OBJ tclAsync.OBJ tclBasic.OBJ tclCkalloc.OBJ tclClock.OBJ tclCmdAH.OBJ tclCmdIL.OBJ tclCmdMZ.OBJ tclDate.OBJ tclEnv.OBJ tclEvent.OBJ tclExpr.OBJ tclFHandle.OBJ tclGet.OBJ tclHash.OBJ tclHistory.OBJ tclInterp.OBJ tclIO.OBJ tclIOCmd.OBJ tclIOSock.OBJ tclIOUtil.OBJ tclLink.OBJ tclLoad.OBJ tclNotify.OBJ tclParse.OBJ tclPkg.OBJ tclPosixStr.OBJ tclPreserve.OBJ tclProc.OBJ tclUtil.OBJ tclVar.OBJ unixlib.OBJ so32dll.lib tcp32dll.lib TCL75VA.def

tclOS2Chan.OBJ: tclOS2Chan.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Chan.c

tclOS2Console.OBJ: tclOS2Console.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Console.c

tclOS2Dll.OBJ: tclOS2Dll.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Dll.c

tclOS2Env.OBJ: tclOS2Env.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Env.c

tclOS2Error.OBJ: tclOS2Error.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Error.c

tclOS2File.OBJ: tclOS2File.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2File.c

tclOS2FileName.OBJ: tclOS2FileName.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2FileName.c

tclOS2Init.OBJ: tclOS2Init.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Init.c

tclOS2Load.OBJ: tclOS2Load.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Load.c

tclOS2Main.OBJ: tclOS2Main.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Main.c

tclOS2NoSock.OBJ: tclOS2NoSock.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2NoSock.c

tclOS2Notify.OBJ: tclOS2Notify.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Notify.c

tclOS2Pipe.OBJ: tclOS2Pipe.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Pipe.c

tclOS2Test.OBJ: tclOS2Test.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Test.c

tclOS2Time.OBJ: tclOS2Time.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Time.c

tclOS2Util.OBJ: tclOS2Util.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclOS2Util.c

tclTest.OBJ: tclTest.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclTest.c

panic.OBJ: panic.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  panic.c

regexp.OBJ: regexp.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  regexp.c

tclAsync.OBJ: tclAsync.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclAsync.c

tclBasic.OBJ: tclBasic.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclBasic.c

tclCkalloc.OBJ: tclCkalloc.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclCkalloc.c

tclClock.OBJ: tclClock.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclClock.c

tclCmdAH.OBJ: tclCmdAH.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclCmdAH.c

tclCmdIL.OBJ: tclCmdIL.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclCmdIL.c

tclCmdMZ.OBJ: tclCmdMZ.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclCmdMZ.c

tclDate.OBJ: tclDate.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclDate.c

tclEnv.OBJ: tclEnv.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclEnv.c

tclEvent.OBJ: tclEvent.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclEvent.c

tclExpr.OBJ: tclExpr.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclExpr.c

tclFHandle.OBJ: tclFHandle.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclFHandle.c

tclGet.OBJ: tclGet.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclGet.c

tclHash.OBJ: tclHash.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclHash.c

tclHistory.OBJ: tclHistory.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclHistory.c

tclInterp.OBJ: tclInterp.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclInterp.c

tclIO.OBJ: tclIO.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclIO.c

tclIOCmd.OBJ: tclIOCmd.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclIOCmd.c

tclIOSock.OBJ: tclIOSock.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclIOSock.c

tclIOUtil.OBJ: tclIOUtil.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclIOUtil.c

tclLink.OBJ: tclLink.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclLink.c

tclLoad.OBJ: tclLoad.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclLoad.c

tclNotify.OBJ: tclNotify.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclNotify.c

tclParse.OBJ: tclParse.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclParse.c

tclPkg.OBJ: tclPkg.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclPkg.c

tclPosixStr.OBJ: tclPosixStr.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclPosixStr.c

tclPreserve.OBJ: tclPreserve.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclPreserve.c

tclProc.OBJ: tclProc.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclProc.c

tclUtil.OBJ: tclUtil.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclUtil.c

tclVar.OBJ: tclVar.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  tclVar.c

unixlib.OBJ: unixlib.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  unixlib.c

tcl75va.lib: tcl75va.dll
   implib /NOI tcl75va.lib tcl75va.def

tclsh.exe: tclAppInit.OBJ tcl75va.lib
   icc $(DEBUG) -B"/PM:PM"  -Gm+ $(STATIC) -Fetclsh.exe tclAppInit.OBJ tcl75va.lib

tclAppInit.OBJ: tclAppInit.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  tclAppInit.c

