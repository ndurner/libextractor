INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

noinst_PROGRAMS = dictionary-builder

noinst_LTLIBRARIES = libcommon.la

EXTRA_DIST = \
 en.txt de.txt es.txt da.txt no.txt it.txt peXX.c printableextractor.c 

CLEANFILES = en.c de.c es.c da.c no.c it.c peen.c pede.c pees.c peda.c peit.c

en.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/en > en.c
de.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/de > de.c
es.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/es > es.c
da.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/da > da.c
no.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/no > no.c
it.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/it > it.c
peen.c:
	cat peXX.c | sed -e "s/XX/en/" > peen.c
pede.c:
	cat peXX.c | sed -e "s/XX/de/" > pede.c
pees.c:
	cat peXX.c | sed -e "s/XX/es/" > pees.c
peda.c:
	cat peXX.c | sed -e "s/XX/da/" > peda.c
peno.c:
	cat peXX.c | sed -e "s/XX/no/" > peno.c
peit.c:
	cat peXX.c | sed -e "s/XX/it/" > peit.c

dictionary_builder_SOURCES = \
 dictionary-builder.c 
dictionary_builder_LDADD = \
 $(LIBINTL)

plugin_LTLIBRARIES = \
 libextractor_printable_en.la \
 libextractor_printable_it.la \
 libextractor_printable_da.la \
 libextractor_printable_de.la \
 libextractor_printable_es.la \
 libextractor_printable_no.la 


PLUGINFLAGS = \
  -export-dynamic -avoid-version -module 
libextractor_printable_en_la_LIBADD = \
  libcommon.la \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_it_la_LIBADD = \
  libcommon.la \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_da_la_LIBADD = \
  libcommon.la \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_de_la_LIBADD = \
  libcommon.la \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_es_la_LIBADD = \
  libcommon.la \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_no_la_LIBADD = \
  libcommon.la \
  $(top_builddir)/src/main/libextractor.la

libcommon_la_SOURCES = \
  bloomfilter.c \
  bloomfilter.h \
  sha1.h \
  sha1.c   

libextractor_printable_en_la_SOURCES = \
  en.c peen.c
libextractor_printable_en_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_printable_es_la_SOURCES = \
  es.c pees.c
libextractor_printable_es_la_LDFLAGS = \
 $(PLUGINFLAGS)

libextractor_printable_de_la_SOURCES = \
  de.c pede.c
libextractor_printable_de_la_LDFLAGS = \
 $(PLUGINFLAGS)

libextractor_printable_it_la_SOURCES = \
  it.c peit.c
libextractor_printable_it_la_LDFLAGS = \
 $(PLUGINFLAGS)

libextractor_printable_da_la_SOURCES = \
  da.c peda.c
libextractor_printable_da_la_LDFLAGS = \
 $(PLUGINFLAGS)

libextractor_printable_no_la_SOURCES = \
  no.c peno.c
libextractor_printable_no_la_LDFLAGS = \
 $(PLUGINFLAGS)
