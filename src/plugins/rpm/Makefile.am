INCLUDES = -I$(top_srcdir)/src/include

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

PLUGINFLAGS = -export-dynamic -avoid-version -module

libextractor_rpm_la_SOURCES = \
 rpmextractor.c
libextractor_rpm_la_LDFLAGS = \
 $(PLUGINFLAGS) $(WINFLAGS)
