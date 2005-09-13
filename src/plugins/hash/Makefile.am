include ../Makefile-plugins.am

EXTRA_DIST = \
 MD5_SYMBOLS \
 SHA1_SYMBOLS \
 RMD160_SYMBOLS

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_hash_md5.la \
 libextractor_hash_sha1.la \
 libextractor_hash_rmd160.la

libextractor_hash_md5_la_SOURCES = \
  md5extractor.c
libextractor_hash_md5_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)MD5_SYMBOLS

libextractor_hash_sha1_la_SOURCES = \
  sha1extractor.c
libextractor_hash_sha1_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)SHA1_SYMBOLS

libextractor_hash_rmd160_la_SOURCES = \
  rmd160extractor.c
libextractor_hash_rmd160_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)RMD160_SYMBOLS
