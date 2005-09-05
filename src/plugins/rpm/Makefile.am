INCLUDES = -I$(top_srcdir)/src/include

EXTRA_DIST = SYMBOLS

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

SUBDIRS = .

if MINGW
  WINFLAGS = -lpthread -lws2_32
endif

plugin_LTLIBRARIES = \
 libextractor_rpm.la

PLUGINFLAGS = -Wl,-Bsymbolic -avoid-version -module -no-undefined

libextractor_rpm_la_SOURCES = \
 rpmextractor.c
libextractor_rpm_la_LDFLAGS = \
 $(PLUGINFLAGS)  -Wl,--retain-symbols-file -Wl,SYMBOLS \
 $(WINFLAGS)
