SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

# Define a C macro LOCALEDIR indicating where catalogs will be installed.
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

AM_CFLAGS = \
  -DPLUGIN_PATH="\"${plugindir}\""

extract_LDADD = \
  $(top_builddir)/src/main/libextractor.la 

libextractor_la_LDFLAGS = \
  -export-dynamic -version-info 1:0:0 $(LIBICONV)
libextractor_la_LIBADD = \
  $(LIBLTDL)
libextractor_la_DEPENDENCIES = \
  $(LIBLTDL) 


EXTRA_DIST = \
  winproc.c \
  libextractor_python.c \
  extract.py

if MINGW
  winproc = winproc.c
endif

libextractor_la_SOURCES = \
  extractor.c \
  $(winproc) \
  org_ovmj_libextractor_Extractor.h

extract_SOURCES = \
  extract.c \
  getopt.c \
  getopt.h \
  getopt1.c 
