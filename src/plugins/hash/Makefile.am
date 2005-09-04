INCLUDES = -I$(top_srcdir)/src/include

EXTRA_DIST = \
 MD5_SYMBOLS \
 SHA1_SYMBOLS \
 RMD160_SYMBOLS

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_hash_md5.la \
 libextractor_hash_sha1.la \
 libextractor_hash_rmd160.la

PLUGINFLAGS = -Wl,-Bsymbolic -avoid-version -module -no-undefined

libextractor_hash_md5_la_SOURCES = \
  md5extractor.c
libextractor_hash_md5_la_LDFLAGS = \
  $(PLUGINFLAGS)  -Wl,--retain-symbols-file -Wl,MD5_SYMBOLS

libextractor_hash_sha1_la_SOURCES = \
  sha1.h \
  sha1.c \
  sha1extractor.c
libextractor_hash_sha1_la_LDFLAGS = \
  $(PLUGINFLAGS)  -Wl,--retain-symbols-file -Wl,SHA1_SYMBOLS

libextractor_hash_rmd160_la_SOURCES = \
  rmd160extractor.c
libextractor_hash_rmd160_la_LDFLAGS = \
  $(PLUGINFLAGS)  -Wl,--retain-symbols-file -Wl,RMD160_SYMBOLS
