INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_hash_md5.la \
 libextractor_hash_sha1.la \
 libextractor_hash_rmd160.la

PLUGINFLAGS = -export-dynamic -avoid-version -module

libextractor_hash_md5_la_SOURCES = \
  md5.h \
  md5.c \
  md5extractor.c
libextractor_hash_md5_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_hash_sha1_la_SOURCES = \
  sha1.h \
  sha1.c \
  sha1extractor.c
libextractor_hash_sha1_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_hash_rmd160_la_SOURCES = \
  rmd160extractor.c
libextractor_hash_rmd160_la_LDFLAGS = \
  $(PLUGINFLAGS)
