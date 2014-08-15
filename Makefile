
# set this to the name of your makedepend program
MDEP=mdep

# you can use "DYNAMIC=1" on the make command line to compile a version
# of the libraries that is statically linked.  This feature is handled by
# each of the sub-makefiles.

ALL:
	cd tcl\os2
	$(MAKE)
	cd ..\..
	cd tk\os2
	$(MAKE)
	cd ..\..
	cd tkinter
	$(MAKE)
	cd ..
	cd pycmd
	$(MAKE)
	cd ..

clean:
	cd tcl\os2
	$(MAKE) clean
	cd ..\..
	cd tk\os2
	$(MAKE) clean
	cd ..\..
	cd tkinter
	$(MAKE) clean
	cd ..
	cd pycmd
	$(MAKE) clean
	cd ..

clean_targets:
	cd tcl\os2
	$(MAKE) clean_targets
	cd ..\..
	cd tk\os2
	$(MAKE) clean_targets
	cd ..\..
	cd tkinter
	$(MAKE) clean_targets
	cd ..
	cd pycmd
	$(MAKE) clean_targets
	cd ..

deps:
	cd tcl\os2
	$(MAKE) deps
	cd ..\..
	cd tk\os2
	$(MAKE) deps
	cd ..\..
	cd tkinter
	$(MAKE) deps
	cd ..
	cd pycmd
	$(MAKE) deps
	cd ..


