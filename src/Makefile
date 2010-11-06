SRCDIR = ./src
BINDIR = ./bin
DISTFILES = Makefile configure README COPYING COPYRIGHT CHANGES CREDITS

targets all install:	$(SRCDIR)/config.h $(BINDIR)/cm $(BINDIR)/west.db

$(SRCDIR)/Makefile $(BINDIR)/Makefile $(SRCDIR)/config.h:	$(SRCDIR)/Makefile.in $(BINDIR)/Makefile.in $(SRCDIR)/config.h.in
	sh configure

$(BINDIR)/cm:  $(SRCDIR)/config.h
	cd src; make

$(BINDIR)/west.db:  $(BINDIR)/cm $(BINDIR)/Makefile
	cd bin; make

clean:
	cd src; make clean

dist:
	pwd | sed 's|.*/||' > .dname
	rm -rf `cat .dname`
	mkdir `cat .dname`
	ln $(DISTFILES) `cat .dname`
	cd src; make $(MFLAGS) dist
	cd bin; make $(MFLAGS) dist
	tar cf - `cat .dname` | gzip > `cat .dname`.tar.gz
	rm -rf `cat .dname` .dname

