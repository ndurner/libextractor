# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.61)
AC_INIT([libextractor], [0.6.3], [bug-libextractor@gnu.org])
AM_INIT_AUTOMAKE([libextractor], [0.6.3])
AC_CONFIG_HEADERS([config.h])
AH_TOP([#define _GNU_SOURCE  1])

LIB_VERSION_CURRENT=3
LIB_VERSION_REVISION=2
LIB_VERSION_AGE=0
AC_SUBST(LIB_VERSION_CURRENT)
AC_SUBST(LIB_VERSION_REVISION)
AC_SUBST(LIB_VERSION_AGE)

# Checks for programs.
AC_CANONICAL_HOST
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CANONICAL_HOST
# save LIBS, libtool does a AC_SEARCH_LIBS(dlopen, dl), but plugins
# need not have -ldl added
LIBSOLD=$LIBS
LT_INIT([disable-static dlopen win32-dll])
LTDL_INIT
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
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
openbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
netbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     XTRA_CPPLIBS=-lstdc++
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
     CFLAGS="-fno-common $CFLAGS"
     LIBEXT=.so
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
     LDFLAGS="$LDFLAGS -Wl,-no-undefined -Wl,--export-all-symbols"
		 LIBSOLD=$LIBS
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
AC_DEFINE_DIR([LOCALEDIR], [datarootdir/locale], [gettext catalogs])

ISOPFX=`pkg-config --variable=prefix iso-codes`
pkg-config --variable=prefix iso-codes 2> /dev/null || ISOPFX=/usr
AC_DEFINE_DIR([ISOLOCALEDIR], [ISOPFX/share/locale], [iso-639 catalog])

# relative plugin directory
rplugindir="libextractor"
AC_ARG_WITH(plugindirname,
	AC_HELP_STRING(
		[--with-plugindirname],
		[install directory for plugins (always relative to libdir)]),
	[rplugindir=$withval])
AC_SUBST(RPLUGINDIR, $rplugindir)

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

AM_CONDITIONAL(HAVE_CXX, test "x$HAVE_CXX" = "xyes")


# Checks for libraries.

# Redhat 9, gcc 3.2, libextractor_pdf barfs if we don't check for this
AC_CHECK_LIB(stdc++, fclose)

AC_CHECK_HEADERS(langinfo.h)
LIBS=$LIBSOLD
# FIXME: allow --with-oggvorbis=PFX

# test if we have vorbisfile
# prior versions had ov_open_callbacks in libvorbis, test that, too.
AC_CHECK_LIB(vorbisfile, ov_open_callbacks,
        [AC_CHECK_HEADERS([vorbis/vorbisfile.h],
          AM_CONDITIONAL(HAVE_VORBISFILE, true)
          AC_DEFINE(HAVE_VORBISFILE,1,[Have vorbisfile]),
          AM_CONDITIONAL(HAVE_VORBISFILE, false)
          AC_DEFINE(HAVE_VORBISFILE,0,[lacking vorbisfile]))],
        AM_CONDITIONAL(HAVE_VORBISFILE, false),
        -lvorbis -logg)
AC_CHECK_LIB(FLAC, FLAC__stream_decoder_init_stream,
        [AC_CHECK_HEADERS([FLAC/all.h],
         AM_CONDITIONAL(HAVE_FLAC, true)
          AC_DEFINE(HAVE_FLAC,1,[Have flac]),
         AM_CONDITIONAL(HAVE_FLAC, false))],
        AM_CONDITIONAL(HAVE_FLAC, false),
        -logg)
# test without -logg to see whether we really need it (libflac can be without)
AC_CHECK_LIB(FLAC, FLAC__stream_decoder_init_ogg_stream,
         AM_CONDITIONAL(HAVE_FLAC, true)
         AC_DEFINE(HAVE_FLAC,1,[Have flac])
         AM_CONDITIONAL(NEED_OGG, false),
        [AM_CONDITIONAL(NEED_OGG, true)])
AC_CHECK_LIB(vorbisfile, vorbis_comment_query,
        AM_CONDITIONAL(NEED_VORBIS, false),
        AM_CONDITIONAL(NEED_VORBIS, true),
        -logg)

AC_CHECK_LIB(z, inflate,
        [AC_CHECK_HEADERS([zlib.h],
         AM_CONDITIONAL(HAVE_ZLIB, true)
         AC_DEFINE(HAVE_ZLIB,1,[Have zlib]),
         AM_CONDITIONAL(HAVE_ZLIB, false))],
         AM_CONDITIONAL(HAVE_ZLIB, false))

AC_CHECK_LIB(bz2, BZ2_decompress,
        [AC_CHECK_HEADERS([bzlib.h],
          AM_CONDITIONAL(HAVE_BZ2, true)
          AC_DEFINE(HAVE_LIBBZ2,1,[Have libbz2]),
         AM_CONDITIONAL(HAVE_BZ2, false))],
         AM_CONDITIONAL(HAVE_BZ2, false))

AC_CHECK_LIB(rpm, rpmReadPackageFile,
        [AC_CHECK_HEADERS([rpm/rpmlib.h],
          AM_CONDITIONAL(HAVE_LIBRPM, true)
          AC_DEFINE(HAVE_LIBRPM,1,[Have librpm]),
         AM_CONDITIONAL(HAVE_LIBRPM, false))],
         AM_CONDITIONAL(HAVE_LIBRPM, false))

AC_CHECK_LIB(mpeg2, mpeg2_init,
        [AC_CHECK_HEADERS([mpeg2dec/mpeg2.h],
           AM_CONDITIONAL(HAVE_MPEG2, true)
           AC_DEFINE(HAVE_MPEG2,1,[Have libmpeg2]),
         AM_CONDITIONAL(HAVE_MPEG2, false))],
         AM_CONDITIONAL(HAVE_MPEG2, false))

AC_CHECK_LIB(poppler, _ZTI9MemStream,
        [AC_CHECK_HEADERS([poppler/goo/gmem.h],
           AM_CONDITIONAL(HAVE_POPPLER, true)
           AC_DEFINE(HAVE_POPPLER,1,[Have libpoppler]),
         AM_CONDITIONAL(HAVE_POPPLER, false))],
         AM_CONDITIONAL(HAVE_POPPLER, false))

AC_MSG_CHECKING(for ImageFactory::iptcData in -lexiv2)
AC_LANG_PUSH(C++)
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -lexiv2"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([[#include <iostream>
#include <exiv2/exif.hpp>
#include <exiv2/image.hpp>
#include <exiv2/futils.hpp>]],
    [[Exiv2::Image *foo = NULL; foo->iptcData(); ]])],
  [AC_MSG_RESULT(yes)
   AM_CONDITIONAL(HAVE_EXIV2, true)
   AC_DEFINE(HAVE_EXIV2,1,[Have exifData in libexiv2])],
  [AC_MSG_RESULT(no)
   AM_CONDITIONAL(HAVE_EXIV2, false)])
LDFLAGS=$SAVED_LDFLAGS
AC_LANG_POP(C++)



AC_MSG_CHECKING(for getPDFMajorVersion in -lpoppler)
AC_LANG_PUSH(C++)
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -lpoppler"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([[#include <poppler/PDFDoc.h>]],
    [[PDFDoc *foo = NULL; foo->getPDFMajorVersion(); ]])],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(HAVE_POPPLER_GETPDFMAJORVERSION,1,[Have getPDFMajorVersion in libpoppler])],
  [AC_MSG_RESULT(no)])
LDFLAGS=$SAVED_LDFLAGS
AC_LANG_POP(C++)

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
AC_CHECK_HEADERS([iconv.h fcntl.h netinet/in.h stdlib.h string.h unistd.h libintl.h limits.h stddef.h zlib.h])

# Checks for typedefs, structures, and compiler characteristics.
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
AC_SEARCH_LIBS(dlopen, dl)
AC_SEARCH_LIBS(shm_open, rt)
AC_CHECK_FUNCS([mkstemp strndup munmap strcasecmp strdup strncasecmp memmove memset strtoul floor getcwd pow setenv sqrt strchr strcspn strrchr strnlen strndup ftruncate shm_open shm_unlink])
LE_LIB_LIBS=$LIBS
LIBS=$LIBSOLD

AM_GNU_GETTEXT_VERSION([0.16.1])
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
qt=0
qt4=0

if test "x$HAVE_CXX" = "xyes"
then
  
# check for Qt 4
SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LDFLAGS=$LDFLAGS
AC_LANG_PUSH([C++])
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
QT_CPPFLAGS=$CPPFLAGS
QT_LDFLAGS=$LDFLAGS
LDFLAGS=$SAVED_LDFLAGS
CPPFLAGS=$SAVED_CPPFLAGS
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

ABI_GSF

AM_CONDITIONAL(WITH_GSF, test "x$have_gsf" = "xtrue")

# produce new line
echo ""
AC_CHECK_LIB(gsf-1, gsf_init, AC_DEFINE(HAVE_GSF_INIT,1,[gsf_init supported]))

AC_CHECK_PROG([HAVE_ZZUF],[zzuf], 1, 0)
AM_CONDITIONAL(HAVE_ZZUF, test 0 != $HAVE_ZZUF)

ffmpeg_enabled=0
AC_MSG_CHECKING([whether to enable the FFmpeg thumbnail extractor])
AC_ARG_ENABLE(ffmpeg,
 [AC_HELP_STRING([--enable-ffmpeg],[Enable FFmpeg support])
  AC_HELP_STRING([--disable-ffmpeg],[Disable FFmpeg support])],
 [case "$enableval" in
  no)	AC_MSG_RESULT(no)
        ffmpeg_enabled=0
	;;
  *)    AC_MSG_RESULT(yes)
        ffmpeg_enabled=1
	;;
  esac],
  [     AC_MSG_RESULT(no)
        ffmpeg_enabled=0])
AM_CONDITIONAL(HAVE_FFMPEG, test x$ffmpeg_enabled != x0)
if test x$ffmpeg_enabled = x1
then
  AC_CHECK_HEADERS(libavutil/avutil.h ffmpeg/avutil.h)
  AC_CHECK_HEADERS(libavformat/avformat.h ffmpeg/avformat.h)
  AC_CHECK_HEADERS(libavcodec/avcodec.h ffmpeg/avcodec.h)
  AC_CHECK_HEADERS(libswscale/swscale.h ffmpeg/swscale.h)
fi


LE_INTLINCL=""
LE_LIBINTL="$LTLIBINTL"
AC_ARG_ENABLE(framework, [  --enable-framework      enable Mac OS X framework build helpers],enable_framework_build=$enableval)
AM_CONDITIONAL(WANT_FRAMEWORK, test x$enable_framework_build = xyes)
if test x$enable_framework_build = xyes
then
  AC_DEFINE([FRAMEWORK_BUILD], 1, [Build a Mac OS X Framework])
  LE_INTLINCL='-I$(top_srcdir)/src/intlemu'
  LE_LIBINTL='$(top_builddir)/src/intlemu/libintlemu.la -framework CoreFoundation'
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$LE_INTLINCL]) 
fi


LE_LIB_LDFLAGS="-export-dynamic -no-undefined"
LE_PLUGIN_LDFLAGS="-export-dynamic -avoid-version -module -no-undefined"


dnl TODO insert a proper check here
AC_CACHE_CHECK([whether -export-symbols-regex works],
 gn_cv_export_symbols_regex_works, 
 [
  case "$host_os" in
  mingw*) gn_cv_export_symbols_regex_works=no;;
  *) gn_cv_export_symbols_regex_works=yes;;
  esac
 ])
if test "x$gn_cv_export_symbols_regex_works" = "xyes"
then
 LE_LIB_LDFLAGS="$LE_LIB_LDFLAGS -export-symbols-regex \"EXTRACTOR_@<:@a-zA-Z0-9_@:>@*\""
 LE_PLUGIN_LDFLAGS="$LE_PLUGIN_LDFLAGS -export-symbols-regex \"EXTRACTOR_@<:@a-zA-Z0-9_@:>@*_.......\""
fi
AC_SUBST(LE_LIB_LDFLAGS)
AC_SUBST(LE_PLUGIN_LDFLAGS)
AC_SUBST(LE_INTLINCL)
AC_SUBST(LE_LIBINTL)
AC_SUBST(LE_LIB_LIBS)

AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(QT_CPPFLAGS)
AC_SUBST(QT_LDFLAGS)
AC_SUBST(XTRA_CPPLIBS)
AC_SUBST(LIBEXT)

PACKAGE_VERSION_NOALPHA=`echo $PACKAGE_VERSION | sed "s/@<:@A-Za-z@:>@*//g;"`
AC_SUBST(PACKAGE_VERSION_NOALPHA)

AC_CONFIG_FILES([Makefile po/Makefile.in m4/Makefile


contrib/macosx/Info.plist
contrib/macosx/Pkg-Info.plist
doc/Makefile
src/Makefile
src/include/Makefile
src/intlemu/Makefile
src/common/Makefile
src/main/Makefile
src/plugins/Makefile
])

AX_CREATE_PKGCONFIG_INFO([libextractor.pc],,[-lextractor],[Metadata extraction library],,)

AC_OUTPUT

if test "x$HAVE_ZLIB_TRUE" = "x#"
then
 AC_MSG_ERROR([FATAL: zlib not found (headers not installed?)])
fi

if test "x$HAVE_EXIV2" = "x#"
then
 AC_MSG_NOTICE([NOTICE: exiv2 disabled])
fi

if test "x$have_gsf" != "xtrue"
then
 AC_MSG_NOTICE([NOTICE: libgsf not found, no OLE2 (MS Office) support])
fi

if test "x$ffmpeg_enabled" = "x0"
then
 AC_MSG_NOTICE([NOTICE: FFmpeg thumbnailer plugin disabled])
else 
 AC_MSG_NOTICE([NOTICE: FFmpeg thumbnailer plugin enabled (security untested)])
fi

if test "x$without_gtk" = "xtrue" -a "x$qt" = "x0" -a "x$qt4" = "x0" -a "x$ffmpeg_enabled" = "x0"
then
 AC_MSG_NOTICE([NOTICE: neither gtk nor Qt not found, no thumbnail support])
fi

if test "x$HAVE_VORBISFILE_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: vorbis support disabled])
fi

if test "x$HAVE_MPEG2_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libmpeg2 not found (will not compile mpeg2 plugin)])
fi

if test "x$HAVE_POPPLER_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libpoppler not found (will not compile pdf plugin)])
fi

if test "x$HAVE_CXX" != "xyes"
then
 AC_MSG_NOTICE([NOTICE: no C++ compiler found (not compiling plugins that require C++)])
fi
