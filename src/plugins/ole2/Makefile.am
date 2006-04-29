include ../Makefile-plugins.am


plugin_LTLIBRARIES = \
  libextractor_ole2.la

libextractor_ole2_la_CFLAGS = \
  $(GSF_CFLAGS) 
libextractor_ole2_la_LIBADD = \
  $(LIBADD) $(GSF_LIBS) \
  $(top_builddir)/src/plugins/libconvert.la \
  $(top_builddir)/src/main/libextractor.la
libextractor_ole2_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand) 
libextractor_ole2_la_SOURCES =  \
        ole2extractor.c
