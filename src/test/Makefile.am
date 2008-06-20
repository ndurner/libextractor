INCLUDES = -I$(top_srcdir)/src/include

SUBDIRS = .

ELIBDIR = $(libdir)

AM_CFLAGS = -DELIBDIR=\"$(ELIBDIR)\" -DPLUGIN_PATH="\"${libdir}\"" -DTESTDATADIR="\"$(top_srcdir)/test\""

LDADD = \
 $(top_builddir)/src/main/libextractor.la  

check_PROGRAMS = \
 trivialtest \
 keywordlisttest \
 plugintest \
 multiload \
 multithreadtest

TESTS = $(check_PROGRAMS)

plugintest_SOURCES = \
 plugintest.c 

trivialtest_SOURCES = \
 trivialtest.c 

keywordlisttest_SOURCES = \
 keywordlisttest.c 

multiload_SOURCES = \
 multiload.c 

multithreadtest_SOURCES = \
 multithreadtest.c 
multithreadtest_LDFLAGS = \
 -lpthread

