include ../Makefile-plugins.am

plugin_LTLIBRARIES = \
 libextractor_word.la

libextractor_word_la_LINK = \
  /bin/sh ../../../libtool --mode=link $(CXXLD) -o libextractor_word.la
libextractor_word_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand) \
  $(XTRA_CPPLIBS)
libextractor_word_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/plugins/libconvert.la \
  -lm 

libextractor_word_la_SOURCES = \
 pole.h pole.cpp \
 wordleaker.h \
 wordextractor.cc 

# gcc 3.3 produces BROKEN code for -O1 and -O2 (PDF extraction
# would fail silently) hence we MUST override the user flag here
# which may contain -O1 or -O2!
# CXXFLAGS = -O0

