# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.57)
AC_INIT([libextractor], [0.5.18a], [bug-libextractor@gnu.org])
AC_REVISION($Revision: 1.67 $)
AM_INIT_AUTOMAKE([libextractor], [0.5.18a])
AM_CONFIG_HEADER(src/include/config.h)

AH_TOP([#define _GNU_SOURCE  1])

# Checks for programs.
AC_CANONICAL_HOST

AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)
AM_CONDITIONAL(HAVE_CXX, test "x$HAVE_CXX" = "xyes")
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LIBTOOL_DLOPEN
AC_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AC_LIBLTDL_INSTALLABLE
AC_PROG_LIBTOOL
AC_CONFIG_SUBDIRS(libltdl)
AC_CANONICAL_HOST
AC_LIB_LTDL
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_SUBST(MKDIR_P)



case "$host_os" in
linux*)
     AC_DEFINE(LINUX,1,[This is a Linux system])
     AC_DEFINE_UNQUOTED(LINUX,1,[This is a LINUX system])
     AM_CONDITIONAL(HAVE_GNU_LD, true)    
     AM_CONDITIONAL(SOMEBSD, false)
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
freebsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     LIBEXT=.so
     ;;
openbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     LIBEXT=.so
     ;;
netbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     LIBEXT=.so
     ;;
*solaris*)
     AC_DEFINE_UNQUOTED(SOLARIS,1,[This is a Solaris system])
     AC_CHECK_LIB(resolv, res_init)
     XTRA_CPPLIBS=-lstdc++
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     CFLAGS="-D_POSIX_PTHREAD_SEMANTICS $CFLAGS"
     LIBEXT=.so
     ;;
darwin*)
     AC_DEFINE_UNQUOTED(DARWIN,1,[This is a Darwin system])
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     LIBEXT=.dylib
     ;;
cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a CYGWIN system])
     LDFLAGS="$LDFLAGS -no-undefined"
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     LIBEXT=.dll
     ;;
mingw*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_LIB(intl, gettext)
     AC_CHECK_LIB(ws2_32, _head_libws2_32_a,,[AC_MSG_ERROR([libextractor requires Winsock2])])
     AC_CHECK_LIB(plibc, plibc_init,,[AC_MSG_ERROR([libextractor requires PlibC])])
     LDFLAGS="$LDFLAGS -no-undefined -Wl,--export-all-symbols"
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, false)
     LIBEXT=.dll
     ;;
*)
     AC_MSG_RESULT(Unrecognised OS $host_os)
     AC_DEFINE_UNQUOTED(OTHEROS,1,[Some strange OS])
     AC_MSG_RESULT(otheros)
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     LIBEXT=.so
     ;;
esac

AM_ICONV

# We define the paths here, because MinGW/GCC expands paths
# passed through the command line ("-DLOCALEDIR=..."). This would
# lead to hard-coded paths ("C:\mingw\mingw\bin...") that do
# not contain the actual installation.
AC_DEFINE_DIR([LOCALEDIR], [datadir/locale], [gettext catalogs])

ISOPFX=`pkg-config --variable=prefix iso-codes`
pkg-config --variable=prefix iso-codes 2> /dev/null || ISOPFX=/usr
AC_DEFINE_DIR([ISOLOCALEDIR], [$ISOPFX/share/locale], [iso-639 catalog])


# large file support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

AM_CONDITIONAL(CYGWIN, test "$build_os" = "cygwin")
AM_CONDITIONAL(MINGW, test "$build_os" = "mingw32")


# use '-fno-strict-aliasing', but only if the compiler can take it
if gcc -fno-strict-aliasing -S -o /dev/null -xc /dev/null >/dev/null 2>&1; 
then
 CFLAGS="-fno-strict-aliasing $CFLAGS"
fi

# Checks for libraries.

# the library tests that follow should not result in every plugin linking to them,
# thus we need to backup LIBS and restore
LIBSOLD=$LIBS

# Redhat 9, gcc 3.2, libextractor_pdf barfs if we don't check for this
AC_CHECK_LIB(stdc++, fclose)

AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_LIB(dl, dlopen)
LIBS=$LIBSOLD
# FIXME: allow --with-oggvorbis=PFX

# test if we have vorbisfile
# prior versions had ov_open_callbacks in libvorbis, test that, too.
AC_CHECK_LIB(vorbisfile, ov_open_callbacks,
        [AM_CONDITIONAL(HAVE_VORBISFILE, true)
         AC_DEFINE(HAVE_VORBISFILE,1,[Have vorbisfile])],
        [AM_CONDITIONAL(HAVE_VORBISFILE, false)])
AC_CHECK_LIB(FLAC, FLAC__stream_decoder_init_stream,
        [AM_CONDITIONAL(HAVE_FLAC, true)
         AC_DEFINE(HAVE_FLAC,1,[Have flac])],
        [AM_CONDITIONAL(HAVE_FLAC, false)])
AC_CHECK_LIB(vorbisfile, vorbis_comment_query,
        [AM_CONDITIONAL(NEED_VORBIS, false)],
        [AM_CONDITIONAL(NEED_VORBIS, true)])

AC_CHECK_LIB(z, inflate,
        [AM_CONDITIONAL(HAVE_ZLIB, true)
        AC_DEFINE(HAVE_ZLIB,1,[Have zlib])],
        [AM_CONDITIONAL(HAVE_ZLIB, false)])

AC_CHECK_LIB(bz2, BZ2_decompress,
        [AM_CONDITIONAL(HAVE_BZ2, true)
        AC_DEFINE(HAVE_LIBBZ2,1,[Have libbz2])],
        [AM_CONDITIONAL(HAVE_BZ2, false)])

AC_CHECK_LIB(mpeg2, mpeg2_init,
        [AM_CONDITIONAL(HAVE_MPEG2, true)
        AC_DEFINE(HAVE_MPEG2,1,[Have libmpeg2])],
        [AM_CONDITIONAL(HAVE_MPEG2, false)])

# restore LIBS
LIBS=$LIBSOLD

# Special check for broken Operating Systems (OS X)
AC_CACHE_CHECK(whether ${CC-cc} accepts -no-cpp-precomp,
cv_prog_cc_darwin_cpp_precomp,
[echo 'void f(){}' > conftest.c
if test -z "`${CC-cc} -no-cpp-precomp -c conftest.c 2>&1`"; then
  cv_prog_cc_darwin_cpp_precomp=yes
else
  cv_prog_cc_darwin_cpp_precomp=no
fi
rm -f conftest*
])
if test $cv_prog_cc_darwin_cpp_precomp = yes; then
  CFLAGS="$CFLAGS -no-cpp-precomp"
fi


# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([fcntl.h netinet/in.h stdlib.h string.h unistd.h libintl.h limits.h stddef.h zlib.h])
AC_CHECK_HEADERS([ltdl.h iconv.h bzlib.h mpeg2dec/mpeg2.h])
# fixme, we need to die here if a header is not found!

AC_CHECK_HEADERS([vorbis/vorbisfile.h])

AC_CHECK_HEADERS([FLAC/all.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_PID_T
GNUPG_CHECK_ENDIAN

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_VPRINTF
AC_FUNC_MMAP
AC_FUNC_STAT
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([mkstemp strndup munmap strcasecmp strdup strncasecmp memmove memset strtoul floor getcwd pow setenv sqrt strchr strcspn strrchr strnlen strndup ftruncate])

AM_GNU_GETTEXT_VERSION([0.13.1])
AM_GNU_GETTEXT([external])

# check for GNU LD
AC_LIB_PROG_LD_GNU

# check for glib >= 2.0.0
AC_MSG_CHECKING(for glib)
AM_PATH_GLIB_2_0(2.0.0,
	         without_glib=false,
                 without_glib=true)
AC_MSG_CHECKING([whether glib is disabled])
AC_ARG_ENABLE(glib,
  [AC_HELP_STRING([--disable-glib],[disable glib support])],
  [case "$enableval" in
   no)  AC_MSG_RESULT(disabled)
        without_glib=true
        ;;
   *)   AC_MSG_RESULT(allowed)
        ;;
   esac])

if test x$without_glib != xtrue
then
  if test $with_gnu_ld = yes
  then
# We need both GNU LD and GLIB here!
    AM_CONDITIONAL(HAVE_GLIB,true)
    AC_DEFINE(HAVE_GLIB, 1, [Have glib])
  else
# We may have GLIB, but without GNU LD we must not use it!
    AM_CONDITIONAL(HAVE_GLIB,false)
    AC_DEFINE(HAVE_GLIB, 0, [Have glib, but not GNU LD])
  fi
else
 AM_CONDITIONAL(HAVE_GLIB,false)
 AC_DEFINE(HAVE_GLIB, 0, [Have glib])
fi
        

# check for gtk >= 2.6.0
AC_MSG_CHECKING(for gtk)
AM_PATH_GTK_2_0(2.6.0,without_gtk=false,without_gtk=true)
AM_CONDITIONAL(HAVE_GTK, test x$without_gtk != xtrue)
if test $without_gtk != true
then
 AC_DEFINE_UNQUOTED([HAVE_GTK], 1, [We have GTK])
else
 AM_CONDITIONAL(HAVE_GTK,false)
fi

# check for all C++ dependencies...
xpdf=0
exiv2=0

if test "x$HAVE_CXX" = "xyes"
then
  
# check for Qt 4
AC_LANG_PUSH([C++])
qt=0
qt4=0
AC_MSG_CHECKING(for Qt)
AC_ARG_WITH(qt,
   [  --with-qt=PFX    Base of Qt installation],
   [AC_MSG_RESULT([$with_qt])
    case $with_qt in
      no)
        ;;
      yes)
        AC_CHECK_HEADERS([Qt/qpixmap.h],
          AC_CHECK_LIB([QtGui4], [_ZN7QPixmap12defaultDepthEv],
            qt4=1,
              AC_CHECK_LIB([QtGui], [_ZN7QPixmap12defaultDepthEv],
              qt=1)))
        ;;
      *)
        LDFLAGS="-L$with_qt/lib $LDFLAGS"
        CPPFLAGS="-I$with_qt/include -I$with_qt/include/qt4 $CPPFLAGS"
        AC_CHECK_HEADERS([Qt/qpixmap.h],
          AC_CHECK_LIB([QtGui4], [_ZN7QPixmap12defaultDepthEv],
            EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
            qt4=1,
              AC_CHECK_LIB([QtGui], [_ZN7QPixmap12defaultDepthEv],
              EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
              qt=1)))
        ;;
    esac
   ],
   [AC_MSG_RESULT([--with-qt not specified])
    LDFLAGS="-L/usr/lib $LDFLAGS"
    CPPFLAGS="-I/usr/include -I/usr/include/qt4 $CPPFLAGS"    
    AC_CHECK_HEADERS([Qt/qpixmap.h],
          AC_CHECK_LIB([QtGui4], [_ZN7QPixmap12defaultDepthEv],
            EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
            qt4=1,
              AC_CHECK_LIB([QtGui], [_ZN7QPixmap12defaultDepthEv],
              EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
              qt=1)))])

# check for Qt SVG module
qt_svg=0
qt_svg4=0
AC_MSG_CHECKING(for Qt SVG)
AC_ARG_WITH(qt,
   [  --with-qt=PFX    Base of Qt installation],
   [AC_MSG_RESULT([$with_qt])
    case $with_qt in
      no)
        ;;
      yes)
        AC_CHECK_HEADERS([Qt/qsvgrenderer.h],
          AC_CHECK_LIB([QtSvg4], [_ZN10QSvgWidgetD0Ev],
            qt_svg4=1,
              AC_CHECK_LIB([QtSvg], [_ZN10QSvgWidgetD0Ev],
              qt_svg=1)))
        ;;
      *)
        LDFLAGS="-L$with_qt/lib $LDFLAGS"
        CPPFLAGS="-I$with_qt/include -I$with_qt/include/qt4 $CPPFLAGS"
        AC_CHECK_HEADERS([Qt/qsvgrenderer.h],
          AC_CHECK_LIB([QtSvg4], [_ZN10QSvgWidgetD0Ev],
            EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
            qt_svg4=1,
              AC_CHECK_LIB([QtSvg], [_ZN10QSvgWidgetD0Ev],
              EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
              qt_svg=1)))
        ;;
    esac
   ],
   [AC_MSG_RESULT([--with-qt not specified])
    LDFLAGS="-L/usr/lib $LDFLAGS"
    CPPFLAGS="-I/usr/include -I/usr/include/qt4 $CPPFLAGS"    
    AC_CHECK_HEADERS([Qt/qsvgrenderer.h],
          AC_CHECK_LIB([QtSvg4], [_ZN10QSvgWidgetD0Ev],
            EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
            qt_svg4=1,
              AC_CHECK_LIB([QtSvg], [_ZN10QSvgWidgetD0Ev],
              EXT_LIB_PATH="-L$with_qt/lib $EXT_LIB_PATH"
              qt_svg=1)))])

AC_MSG_CHECKING([whether to enable xpdf-based extractor])
AC_ARG_ENABLE(xpdf,
 [AC_HELP_STRING([--enable-xpdf],[Enable xpdf-based extractor])
  AC_HELP_STRING([--disable-xpdf],[Disable xpdf-based extractor])],
 [case "$enableval" in
  no)	AC_MSG_RESULT(no)
        xpdf=0
	;;
  *)    AC_MSG_RESULT(yes)
        xpdf=1
	;;
  esac],
  [     AC_MSG_RESULT(no)
        xpdf=0])

exiv2=1
AC_MSG_CHECKING([whether to enable exiv2 extractor])
AC_ARG_ENABLE(exiv2,
 [AC_HELP_STRING([--enable-exiv2],[Enable exiv2 support])
  AC_HELP_STRING([--disable-exiv2],[Disable exiv2 support])],
 [case "$enableval" in
  no)	AC_MSG_RESULT(no)
        exiv2=0
	;;
  *)    AC_MSG_RESULT(yes)
        exiv2=1
	;;
  esac],
  [     AC_MSG_RESULT(yes)
        exiv2=1])
AC_LANG_POP([C++])
# end C++ checks
fi

# set C++-dependent defines
if test x$qt != x0
then
 AC_DEFINE(HAVE_QT, 1, [We have Qt])
fi
AM_CONDITIONAL(HAVE_QT, test x$qt != x0)
if test x$qt4 != x0
then
 AC_DEFINE(HAVE_QT4, 1, [We have Qt4])
fi
AM_CONDITIONAL(HAVE_QT4, test x$qt4 != x0)

if test x$qt_svg != x0
then
 AC_DEFINE(HAVE_QT_SVG, 1, [We have Qt SVG])
fi
if test x$qt_svg4 != x0
then
 AC_DEFINE(HAVE_QT_SVG4, 1, [We have Qt SVG4])
fi
AM_CONDITIONAL(HAVE_QT_SVG, test x$qt_svg != x0)
AM_CONDITIONAL(HAVE_QT_SVG4, test x$qt_svg4 != x0)

AM_CONDITIONAL(HAVE_XPDF, test x$xpdf != x0)

AM_CONDITIONAL(HAVE_EXIV2, test x$exiv2 != x0)
AC_DEFINE_UNQUOTED([HAVE_EXIV2], $exiv2, [We use EXIV2])

ABI_GSF

# produce new line
echo ""
AC_CHECK_LIB(gsf-1, gsf_init, AC_DEFINE(HAVE_GSF_INIT,1,[gsf_init supported]))

printable=1
AC_MSG_CHECKING([whether to enable printable extractors])
AC_ARG_ENABLE(printable,
 [AC_HELP_STRING([--enable-printable],[Enable printable support])
  AC_HELP_STRING([--disable-printable],[Disable printable support])],
 [case "$enableval" in
  no)	AC_MSG_RESULT(no)
        printable=0
	;;
  *)    AC_MSG_RESULT(yes)
        printable=1
	;;
  esac],
  [     AC_MSG_RESULT(yes)
        printable=1])
AM_CONDITIONAL(HAVE_PRINTABLE, test x$printable != x0)


AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(XTRA_CPPLIBS)
AC_SUBST(LIBEXT)

AC_CONFIG_FILES([Makefile po/Makefile.in m4/Makefile 


doc/Makefile
src/Makefile
src/include/Makefile
src/main/Makefile
src/plugins/Makefile
src/plugins/ole2/Makefile
src/plugins/oo/Makefile
src/plugins/pdf/Makefile
src/plugins/rpm/Makefile
src/plugins/printable/Makefile
src/plugins/hash/Makefile
src/plugins/thumbnail/Makefile
src/plugins/exiv2/Makefile
src/test/Makefile
])

LIBEXTRACTOR_VERSION="0.5.18a"
AX_CREATE_PKGCONFIG_INFO([libextractor.pc],,[-lextractor],[Metadata extraction library],,)


AC_OUTPUT

if test "x$xpdf" = "x1"
then
 AC_MSG_NOTICE([NOTICE: xpdf enabled (xpdf has a bad security record)])
else 
 AC_MSG_NOTICE([NOTICE: xpdf disabled (result: limited PDF support)])
fi

if test "x$exiv2" = "x0"
then
 AC_MSG_NOTICE([NOTICE: exiv2 disabled])
fi


if test "x$printable" = "x0"
then
 AC_MSG_NOTICE([NOTICE: printable plugins disabled])
else 
 AC_MSG_NOTICE([NOTICE: printable plugins enabled])
fi

if test "x$have_gsf" != "xtrue"
then
 AC_MSG_NOTICE([NOTICE: libgsf not found, no OLE2 (MS Office) support])
fi

if test "x$without_gtk" = "xtrue" -a "x$qt" = "x0"
then
 AC_MSG_NOTICE([NOTICE: neither gtk nor Qt not found, no thumbnail support])
fi

if test "x$HAVE_VORBISFILE_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: vorbis support disabled])
fi

if test "x$HAVE_ZLIB_TRUE" = "x#"
then
 AC_MSG_ERROR([FATAL: zlib not found (headers not installed?)])
fi

if test "x$HAVE_MPEG2_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libmpeg2 not found (will not compile mpeg2 plugin)])
fi

if test "x$HAVE_CXX" != "xyes"
then
 AC_MSG_NOTICE([NOTICE: no C++ compiler found (not compiling plugins that require C++)])
fi
