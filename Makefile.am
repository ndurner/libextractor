SUBDIRS  = m4 po src doc . 
EXTRA_DIST = config.rpath \
  ABOUT-NLS mkinstalldirs install-sh \
  PLATFORMS README.debian 
# yayh: automake hacks!
dist-hook:
	cp -p $(srcdir)/src/plugins/printable/peXX.c $(distdir)/src/plugins/printable/
	cp -p $(srcdir)/src/plugins/printable/??.txt $(distdir)/src/plugins/printable/
	rm -f $(distdir)/src/plugins/printable/??_*.c
	rm -f $(distdir)/src/plugins/printable/??.c
	rm -f $(distdir)/src/plugins/printable/peda.c
	rm -f $(distdir)/src/plugins/printable/pede.c
	rm -f $(distdir)/src/plugins/printable/peen.c
	rm -f $(distdir)/src/plugins/printable/pees.c
	rm -f $(distdir)/src/plugins/printable/peit.c
	rm -f $(distdir)/src/plugins/printable/peno.c
	rm -f $(distdir)/src/plugins/printable/pept.c

ACLOCAL_AMFLAGS = -I m4

pkgconfigdatadir = $(libdir)/pkgconfig
pkgconfigdata_DATA = libextractor.pc
