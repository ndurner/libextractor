# Makefile
#
# Christian Seberino
# chris@pythonsoft.com
# November 5, 2004
#
# Simple Makefile that builds the cbag shared object.

all:
	gcc cbag.c -o cbag.so -I /usr/include/python2.2 -fpic -shared

clean:
	rm -f *.so *.pyc
