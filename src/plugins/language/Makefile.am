include ../Makefile-plugins.am

noinst_PROGRAMS = language-compiler

CLEANFILES = languages.c

EXTRA_DIST = afrikaans.klp albanian.klp arabic2.klp arabic.klp armenian.klp \
	basque.klp belarus.klp bosnian.klp breton.klp bulgarian.klp catalan.klp \
	chinese1.klp chinese2.klp chinese.klp croatian.klp czech.klp danish.klp dutch.klp \
	english.klp esperanto.klp estonian.klp finnish.klp french.klp frisian.klp \
	georgian.klp german.klp greek.klp hawaian.klp hebrew.klp hindi.klp hungarian.klp \
	icelandic.klp indonesian.klp irish.klp italian.klp japanese-euc_jp.klp \
	japanese-shift_jis.klp korean.klp latin.klp latvian.klp lithuanian.klp malay.klp \
	manx.klp marathi.klp mf.klp middle-frisian.klp mingo.klp nepali.klp norwegian.klp \
	persian.klp polish.klp portuguese.klp quechua.klp romanian.klp rumantsch.klp \
	russian1.klp russian2.klp russian.klp sanskrit.klp scots_gaelic.klp scots.klp \
	serbian.klp slovak2.klp slovak.klp slovenian2.klp slovenian.klp spanish.klp \
	swahili.klp tagalog.klp tamil.klp thai.klp turkish.klp ukrainian.klp \
	vietnamese.klp welsh.klp

language.c: language-compiler$(EXEEXT)
	./language-compiler$(EXEEXT) *.klp > languages.c

language_compiler_SOURCES = \
 language-compiler.c 
language_compiler_LDADD = \
 $(LIBINTL)

plugin_LTLIBRARIES = \
 libextractor_language.la

libextractor_language_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_language_la_SOURCES = \
  languageextractor.c languages.c
libextractor_language_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

