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
 mt_plugintest1 \
 mt_plugintest2 \
 mt_extracttest1 \
 mt_extracttest2

TESTS = $(check_PROGRAMS)

plugintest_SOURCES = \
 plugintest.c 

trivialtest_SOURCES = \
 trivialtest.c 

keywordlisttest_SOURCES = \
 keywordlisttest.c 

multiload_SOURCES = \
 multiload.c 

mt_plugintest1_SOURCES = \
 mt_plugintest1.c 
mt_plugintest1_LDFLAGS = \
 -lpthread

mt_plugintest2_SOURCES = \
 mt_plugintest2.c 
mt_plugintest2_LDFLAGS = \
 -lpthread

mt_extracttest1_SOURCES = \
 mt_extracttest1.c 
mt_extracttest1_LDFLAGS = \
 -lpthread

mt_extracttest2_SOURCES = \
 mt_extracttest2.c 
mt_extracttest2_LDFLAGS = \
 -lpthread


