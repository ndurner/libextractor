SUBDIRS  = m4 po libltdl src doc . 
EXTRA_DIST = config.rpath \
  ABOUT-NLS \
  PLATFORMS \
  src/org/gnunet/libextractor/Extractor.java \
  src/org/gnunet/libextractor/Xtract.java
# yayh: automake hacks!
dist-hook:
	rm -f $(distdir)/src/plugins/printable/??.c
	rm -f $(distdir)/src/plugins/printable/peen.c
	rm -f $(distdir)/src/plugins/printable/pede.c
	rm -f $(distdir)/src/plugins/printable/peit.c
	rm -f $(distdir)/src/plugins/printable/pees.c
	rm -f $(distdir)/src/plugins/printable/peno.c
	rm -f $(distdir)/src/plugins/printable/peda.c

ACLOCAL_AMFLAGS = -I m4
