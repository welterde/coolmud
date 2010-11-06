# Generated automatically from Makefile.in by configure.
# Modified by Robin Powell
# Copyright (c) 1993 Stephen F. White

CPP = gcc -E

all: east.db west.db
east.db: cm.core
	rm -f east.tmp
	for file in $?; do cat $$file >> east.tmp; done
	cp east.cl east.cl.c
	$(CPP) east.cl.c | grep -v '^#' >>east.tmp
	rm east.cl.c
	./cm east -i -f east.tmp
	rm -f east.tmp
west.db: cm.core
	rm -f west.tmp
	for file in $?; do cat $$file >> west.tmp; done
	cp west.cl west.cl.c
	$(CPP) west.cl.c | grep -v '^#' >> west.tmp
	rm west.cl.c
	./cm west -i -f west.tmp
	rm -f west.tmp

clean:
	rm -f *.db *.dir *.pag

rm:
	rm -f *.db *.dir *.pag *.log
