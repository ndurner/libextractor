# spec file for package libextractor
#
# (C) 2002, 2003, 2004 SSS Lab, CS, Purdue and Compilers Group, CS,  UCLA 
#
# please send bugfixes or comments to 
# libextractor@cs.purdue.edu

# sanitized by Nils Philippsen <nils@redhat.de>

Summary: meta-data extraction library 
Name: libextractor
Version: 0.3.0
Release: 0
License: GPL
Group: System Environment/Libraries
Requires: glibc >= 2.2.4, libvorbis, libogg, zlib
URL: http://www.ovmj.org/libextractor/
Source: http://www.ovmj.org/libextractor/download/libextractor-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%package devel
Summary: Development files for libextractor
Group: Development/Libraries

%description

libextractor is a simple library for meta-data extraction.
libextractor uses a plugin-mechanism that makes it easy to add support
for more file formats, allowing anybody to add new extractors quickly.

libextractor currently features meta-data extractors for  HTML, PDF, PS, 
MP3, OGG, JPEG, GIF, PNG, TIFF, RPM, ZIP, REAL, RIFF (AVI), MPEG, QT 
and ASF. It also detects many more MIME-types in a fashion similar to 
the well-known "file" tool.  Furthermore, a generic extractor that 
extracts dictionary words from binaries is included.  Supported 
dictionaries are currently da, en, de, it and no.

Each item of meta-data that is extracted from a file is categorized
into one of currently about 40 meta-data categories (e.g. title,
author, description or MIME-type). 

This libextractor package also contains a little binary tool "extract"
that can be used to invoke libextractor from the command
line.  "extract" can be used similar to "file", but while "file"
currently supports a wider range of file types, "extract" should be
able to provide more precise and more detailed information for the
supported types of documents. 

%description devel
This package contains files to develop with libextractor, that is either to
create plugins or to compile applications with libextractor.

%prep
%setup -q -n libextractor-%{version}

%build
%configure
make

%install
%makeinstall
# Sanitize plugins
pushd $RPM_BUILD_ROOT%{_libdir}
rm -f libextractor_*.a 

%post -p /sbin/ldconfig
%postun
if [ $1 = 0 ]; then
	/sbin/ldconfig
fi

%files
%defattr(-, root, root)
%{_libdir}/libextractor.so
%{_libdir}/libextractor.so.*
%{_libdir}/libextractor_*.so
%{_libdir}/libextractor_*.so.*
%{_bindir}/extract
%doc %{_mandir}/man1/*

%files devel
%{_libdir}/libextractor.so
%{_libdir}/*.a
%{_libdir}/*.la
%{_includedir}/extractor.h
%doc %{_mandir}/man3/*

%changelog
* Wed Apr 14 2004  Vids Samanta <vids@cs.ucla.edu>
- updated description
- set version number to 0.3.0

* Wed Apr  7 2004 Vids Samanta <vids@cs.ucla.edu>
- added libextractor_*.so.* files to libdir  
- set version number to 0.2.7

* Tue Oct 14 2003 Vids Samanta <vids@cs.ucla.edu>
- set version to 0.2.6

* Wed Jul 16 2003 Vids Samanta <vids@cs.purdue.edu>
- set version to 0.2.5
- removed pspell dependecy

* Mon Jun 30 2003 Christian Grothoff <grothoff@cs.purdue.edu>
- updated description
- set version to 0.2.4

* Fri Apr 18 2003 Vids Samanta <vids@cs.purdue.edu>
- added symlink /usr/lib/libextractor.so -> /usr/lib/libextractor.so.0.0.0
- set release number to 1

* Fri Apr 11 2003 Vids Samanta <vids@cs.purdue.edu>
- removed rpm-devel from dependecies. 
- updated list of extractors. 
- bumped up version to 0.2.3

* Wed Feb 26 2003 Vids Samanta <vids@cs.purdue.edu>
- bumped version to 0.2.2, updated list of extractors.

* Thu Feb  6 2003 Christian Grothoff <grothoff@cs.purdue.edu>
- bumped version to 0.2.1

* Sat Feb  1 2003 Christian Grothoff <grothoff@cs.purdue.edu>
- bumped version to 0.2.0

* Mon Jan  7 2003 Christian Grothoff <grothoff@cs.purdue.edu>
- bumped version to 0.1.4, minor adjustments to description

* Fri Jul 26 2002 Christian Grothoff <grothoff@cs.purdue.edu>
- bumped version to 0.1.1

* Wed Jun 19 2002 Nils Philippsen <nils@redhat.de>
- sanitize spec file
- split off devel package

* Wed Jun  5 2002 Christian Grothoff <grothoff@cs.purdue.edu>
- drafted spec file. No surprise here.

