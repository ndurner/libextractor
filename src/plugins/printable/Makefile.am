include ../Makefile-plugins.am

noinst_PROGRAMS = dictionary-builder

da_LANG=da_0.c da_1.c da_2.c da_3.c da_4.c da_5.c da_6.c da_7.c da_8.c da_9.c da_10.c da_11.c da_12.c da_13.c da_14.c da_15.c da_16.c da_17.c da_18.c da_19.c da_20.c da_21.c da_22.c da_23.c da_24.c da_25.c da_26.c da_27.c da_28.c da_29.c da_30.c da_31.c
de_LANG=de_0.c de_1.c de_2.c de_3.c de_4.c de_5.c de_6.c de_7.c de_8.c de_9.c de_10.c de_11.c de_12.c de_13.c de_14.c de_15.c de_16.c de_17.c de_18.c de_19.c de_20.c de_21.c de_22.c de_23.c de_24.c de_25.c de_26.c de_27.c de_28.c de_29.c de_30.c de_31.c
en_LANG=en_0.c en_1.c en_2.c en_3.c en_4.c en_5.c en_6.c en_7.c en_8.c en_9.c en_10.c en_11.c en_12.c en_13.c en_14.c en_15.c en_16.c en_17.c en_18.c en_19.c en_20.c en_21.c en_22.c en_23.c en_24.c en_25.c en_26.c en_27.c en_28.c en_29.c en_30.c en_31.c
es_LANG=es_0.c es_1.c es_2.c es_3.c es_4.c es_5.c es_6.c es_7.c es_8.c es_9.c es_10.c es_11.c es_12.c es_13.c es_14.c es_15.c es_16.c es_17.c es_18.c es_19.c es_20.c es_21.c es_22.c es_23.c es_24.c es_25.c es_26.c es_27.c es_28.c es_29.c es_30.c es_31.c
it_LANG=it_0.c it_1.c it_2.c it_3.c it_4.c it_5.c it_6.c it_7.c it_8.c it_9.c it_10.c it_11.c it_12.c it_13.c it_14.c it_15.c it_16.c it_17.c it_18.c it_19.c it_20.c it_21.c it_22.c it_23.c it_24.c it_25.c it_26.c it_27.c it_28.c it_29.c it_30.c it_31.c
no_LANG=no_0.c no_1.c no_2.c no_3.c no_4.c no_5.c no_6.c no_7.c no_8.c no_9.c no_10.c no_11.c no_12.c no_13.c no_14.c no_15.c no_16.c no_17.c no_18.c no_19.c no_20.c no_21.c no_22.c no_23.c no_24.c no_25.c no_26.c no_27.c no_28.c no_29.c no_30.c no_31.c
pt_LANG=pt_0.c pt_1.c pt_2.c pt_3.c pt_4.c pt_5.c pt_6.c pt_7.c pt_8.c pt_9.c pt_10.c pt_11.c pt_12.c pt_13.c pt_14.c pt_15.c pt_16.c pt_17.c pt_18.c pt_19.c pt_20.c pt_21.c pt_22.c pt_23.c pt_24.c pt_25.c pt_26.c pt_27.c pt_28.c pt_29.c pt_30.c pt_31.c

CLEANFILES = da.c de.c en.c es.c it.c no.c pt.c peda.c pede.c peen.c pees.c peit.c peno.c pept.c $(da_LANG) $(de_LANG) $(es_LANG) $(en_LANG) $(it_LANG) $(no_LANG) $(pt_LANG)

da.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/da da > da.c
de.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/de de > de.c
en.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/en en > en.c
es.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/es es > es.c
it.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/it it > it.c
no.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/no no > no.c
pt.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/pt pt > pt.c

peda.c:
	cat peXX.c | sed -e "s/XX/da/" > peda.c
pede.c:
	cat peXX.c | sed -e "s/XX/de/" > pede.c
peen.c:
	cat peXX.c | sed -e "s/XX/en/" > peen.c
pees.c:
	cat peXX.c | sed -e "s/XX/es/" > pees.c
peit.c:
	cat peXX.c | sed -e "s/XX/it/" > peit.c
peno.c:
	cat peXX.c | sed -e "s/XX/no/" > peno.c
pept.c:
	cat peXX.c | sed -e "s/XX/pt/" > pept.c

dictionary_builder_SOURCES = \
 dictionary-builder.c 
dictionary_builder_LDADD = \
 $(LIBINTL) 

plugin_LTLIBRARIES = \
 libextractor_printable_da.la \
 libextractor_printable_de.la \
 libextractor_printable_en.la \
 libextractor_printable_es.la \
 libextractor_printable_it.la \
 libextractor_printable_no.la \
 libextractor_printable_pt.la 


libextractor_printable_da_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_de_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_en_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_es_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_it_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_no_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_pt_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la


libextractor_printable_da_la_SOURCES = \
  da.c $(da_LANG) peda.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_da_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_de_la_SOURCES = \
  de.c $(de_LANG) pede.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_de_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_en_la_SOURCES = \
  en.c $(en_LANG) peen.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_en_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_es_la_SOURCES = \
  es.c $(es_LANG) pees.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_es_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_it_la_SOURCES = \
  it.c $(it_LANG) peit.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_it_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_no_la_SOURCES = \
  no.c $(no_LANG) peno.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_no_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_pt_la_SOURCES = \
  pt.c $(pt_LANG) pept.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_pt_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)
