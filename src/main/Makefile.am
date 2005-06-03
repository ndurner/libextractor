SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

AM_CFLAGS = \
  -DPLUGIN_PATH="\"${plugindir}\""

extract_LDADD = \
  $(top_builddir)/src/main/libextractor.la 

if !MINGW
  dlflag=-ldl
endif

libextractor_la_LDFLAGS = \
  -export-dynamic -version-info 1:1:0 $(LIBICONV)
libextractor_la_LIBADD = \
  $(LIBLTDL) $(dlflag)
libextractor_la_DEPENDENCIES = \
  $(LIBLTDL) 


EXTRA_DIST = \
  winproc.c \
  libextractor_python.c \
  Extractor.py \
  libextractor_python_setup.py \
  extract.py \
  iconv.c

if MINGW
  winproc = winproc.c
endif

libextractor_la_SOURCES = \
  extractor.c \
  $(winproc) \
  org_gnunet_libextractor_Extractor.h

extract_SOURCES = \
  extract.c \
  getopt.c \
  getopt.h \
  getopt1.c 

if HAVE_PYTHON
install-exec-local:
	python libextractor_python_setup.py $(libdir) build
	python libextractor_python_setup.py $(libdir) install --prefix=$(PYTHON_PREFIX)
endif
