INCLUDES = -I$(top_srcdir)/src/include

# install plugins under:
plugindir = $(libdir)/libextractor

SUBDIRS = .

LIBS = \
 @LTLIBINTL@ @LIBS@

plugin_LTLIBRARIES = \
 libextractor_pdf.la

PLUGINFLAGS = -Wl,-Bsymbolic -avoid-version -module -no-undefined

libextractor_pdf_la_LINK = \
  /bin/sh ../../../libtool --mode=link $(CXXLD) -o libextractor_pdf.la
libextractor_pdf_la_LDFLAGS = \
  $(PLUGINFLAGS)  -Wl,--retain-symbols-file -Wl,SYMBOLS \
  $(XTRA_CPPLIBS)
libextractor_pdf_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/plugins/libconvert.la \
  -lm 

libextractor_pdf_la_SOURCES = \
 Array.cc\
 Catalog.cc\
 Decrypt.cc\
 Dict.cc\
 Error.cc\
 FontEncoding.cc\
 FontFile.cc\
 Function.cc\
 GString.cc\
 Lexer.cc\
 Link.cc\
 Object.cc\
 PDFDoc.cc\
 Page.cc\
 Params.cc\
 Parser.cc\
 Stream.cc\
 XRef.cc\
 gfile.cc\
 gmem.cc\
 gmempp.cc\
 parseargs.cc\
 pdfextractor.cc

# gcc 3.3 produces BROKEN code for -O1 and -O2 (PDF extraction
# would fail silently) hence we MUST override the user flag here
# which may contain -O1 or -O2!
CXXFLAGS = -O0

EXTRA_DIST = \
 SYMBOLS \
 darwin.lt.rb \
 aconf.h \
 aconf2.h \
 ErrorCodes.h \
 Array.h\
 Catalog.h\
 CompactFontInfo.h\
 Decrypt.h\
 Dict.h\
 Error.h\
 FontEncoding.h\
 FontFile.h\
 FontInfo.h\
 Function.h\
 GString.h\
 Lexer.h\
 Link.h\
 Object.h\
 PDFDoc.h\
 Page.h\
 Params.h\
 Parser.h\
 StdFontInfo.h \
 Stream-CCITT.h\
 Stream.h\
 XRef.h\
 gfile.h\
 gmem.h\
 gtypes.h\
 parseargs.h
