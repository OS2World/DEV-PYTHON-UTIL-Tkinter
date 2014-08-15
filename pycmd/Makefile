
# this is another hacked makefile originally created by my makefile generator.
#
# If you have the OS2 version of makedepend installed, I recommend that
# you run "nmake deps" to create the dependencies.
#
# After this, just run "nmake"
#
# "nmake NODEBUG=1" will create a version without debug info.
# "nmake clean" will delete all targets and intermediate files
# "nmake clean_targets" will delete all targets
#

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

DEFS=

All:
   $(MAKE) bins

deps:
   if exist Makefile.bak attrib -r Makefile.bak
   $(MDEP) -i INCLUDE $(DEFS)  pycmd.c pycmd.c

clean_targets:
   if exist pmpycmd.exe del pmpycmd.exe
   if exist pycmd.exe del pycmd.exe

bins: pmpycmd.exe pycmd.exe

clean:
   if exist pmpycmd.exe del pmpycmd.exe
   if exist pycmd.OBJ del pycmd.OBJ
   if exist pycmd.exe del pycmd.exe
   if exist pycmd.OBJ del pycmd.OBJ

pmpycmd.exe: pycmd.OBJ
   icc $(DEBUG) $(LINKOPTS) -B"/PM:PM"  -Gm+ $(STATIC) -Fepmpycmd.exe pycmd.OBJ \
		PYTHON15.lib

pycmd.OBJ: pycmd.c
   icc -c $(DEBUG) $(DEFS) -Gm+ -Ss+ $(STATIC)  pycmd.c

pycmd.exe: pycmd.OBJ
   icc $(DEBUG) $(LINKOPTS)   -Gm+ $(STATIC) -Fepycmd.exe pycmd.OBJ PYTHON15.lib

