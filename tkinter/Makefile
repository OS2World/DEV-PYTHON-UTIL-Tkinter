
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

INCLUDE=../tcl/os2;../tk/os2;$(INCLUDE)
LIB=../tcl/os2;../tk/os2;$(LIB)
DEFS=

LINKOPTS=-B"/ST:1000000"


All:
   $(MAKE) pre
   $(MAKE) bins

pre:

deps:
   if exist Makefile.bak attrib -r Makefile.bak
   $(MDEP) -i INCLUDE $(DEFS)  python.c _tkinter.c

clean_targets:
   if exist pmpython.exe del pmpython.exe
   if exist _tkinter.dll del _tkinter.dll

bins: pmpython.exe _tkinter.dll

clean:
   if exist pmpython.exe del pmpython.exe
   if exist python.OBJ del python.OBJ
   if exist _tkinter.dll del _tkinter.dll
   if exist _tkinter.OBJ del _tkinter.OBJ
	if exist _TKINTER.def del _TKINTER.def

pmpython.exe: python.OBJ
   icc $(DEBUG) $(LINKOPTS) -B"/PM:PM"  -Gm+ $(STATIC) -Fepmpython.exe python.OBJ python15.lib

python.OBJ: python.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  python.c

_tkinter.dll: _tkinter.OBJ
   echo LIBRARY _TKINTER INITINSTANCE TERMINSTANCE >_TKINTER.def
   echo PROTMODE >>_TKINTER.def
   echo DATA MULTIPLE NONSHARED READWRITE LOADONCALL >>_TKINTER.def
   echo CODE LOADONCALL >>_TKINTER.def
   echo EXPORTS >>_TKINTER.def
   cppfilt -B -X  _tkinter.OBJ >>_TKINTER.def
   icc $(DEBUG) $(LINKOPTS)  -Ge- -Gm+ $(STATIC) -Fe_tkinter.dll _tkinter.OBJ tcl75va.lib tk41va.lib Python15.lib _TKINTER.def

_tkinter.OBJ: _tkinter.c
   icc -c $(DEBUG) -Gm+ -Ss $(STATIC) -Ge-  _tkinter.c

