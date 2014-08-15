SHELL = f:/bin/sh.exe
ROOT = ../../..
WINRC = $(ROOT)/win/rc
CP = cp
INCLUDEDIR = $(ROOT)/generic
INCLUDE = -I$(INCLUDEDIR)
rcinclude = -i $(INCLUDEDIR)
RCWINFILES = $(wildcard $(WINRC)/*.cur) $(wildcard $(WINRC)/*.ico)
RCFILES = $(notdir $(RCWINFILES))

all: tk.res cursors.h

tk.res: tk.rc $(RCFILES)
	rc -r -n -x2 $(rcinclude) $<

tk.rc: tkwin.rc
	smartrc $(INCLUDE) $< && $(CP) tkwin.rcx $@

cursors.h tkwin.rc: $(WINRC)/tk.rc fixrc.pl
	perl fixrc.pl $< > $@

%.cuw: $(WINRC)/%.cur
	$(CP) $< $@
	chmod a+w $@

%.icw: $(WINRC)/%.ico
	$(CP) $< $@
	chmod a+w $@



# "-" Needed?!

%.cur: %.cuw
	-smartcvt $< $@

%.ico: %.icw
	-smartcvt $< $@

