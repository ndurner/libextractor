# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.61)
AC_INIT([libextractor], [1.0.1], [bug-libextractor@gnu.org])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AH_TOP([#define _GNU_SOURCE  1])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CANONICAL_SYSTEM

LIB_VERSION_CURRENT=4
LIB_VERSION_REVISION=1
LIB_VERSION_AGE=1
AC_SUBST(LIB_VERSION_CURRENT)
AC_SUBST(LIB_VERSION_REVISION)
AC_SUBST(LIB_VERSION_AGE)

AM_INIT_AUTOMAKE([silent-rules])

# Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CANONICAL_HOST
AC_LIBLTDL_CONVENIENCE
AC_PROG_LIBTOOL
# save LIBS, libtool does a AC_SEARCH_LIBS(dlopen, dl), but plugins
# need not have -ldl added
LIBSOLD=$LIBS
LT_INIT([disable-static dlopen win32-dll])
LTDL_INIT
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_SUBST(MKDIR_P)

case "$target_os" in
*linux-gnu)
     AC_DEFINE(GNU_LINUX,1,[This is a GNU/Linux system])
     AC_DEFINE_UNQUOTED(GNU_LINUX,1,[This is a GNU/Linux system])
     AM_CONDITIONAL(HAVE_GNU_LD, true)    
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(WINDOWS, false)
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
freebsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     AM_CONDITIONAL(WINDOWS, false)
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
openbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     AM_CONDITIONAL(WINDOWS, false)
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
netbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, true)
     AM_CONDITIONAL(WINDOWS, false)
     XTRA_CPPLIBS=-lstdc++
     LIBEXT=.so
     ;;
*solaris*)
     AC_DEFINE_UNQUOTED(SOLARIS,1,[This is a Solaris system])
     AC_CHECK_LIB(resolv, res_init)
     XTRA_CPPLIBS=-lstdc++
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(WINDOWS, false)
     CFLAGS="-D_POSIX_PTHREAD_SEMANTICS $CFLAGS"
     LIBEXT=.so
     ;;
darwin*)
     AC_DEFINE_UNQUOTED(DARWIN,1,[This is a Darwin system])
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(WINDOWS, false)
     CFLAGS="-no-cpp-precomp -fno-common $CFLAGS" 
     LIBEXT=.so
     ;;
cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a CYGWIN system])
     LDFLAGS="$LDFLAGS -no-undefined"
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(WINDOWS, false)
     LIBEXT=.dll
     ;;
mingw*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_LIB(intl, gettext)
     mingw32_ws2=yes
     mingw64_ws2=yes
     AC_CHECK_LIB(ws2_32, _head_libws2_32_a,,mingw32_ws2=no)
     AC_CHECK_LIB(ws2_32, __imp_socket,,mingw64_ws2=no)
     if test "x$mingw32_ws2" = "xno" -a "x$mingw64_ws2" = "xno"; then
       AC_MSG_ERROR([libextractor requires Winsock2])
     fi
     # Sufficiently new Windows XP
     CFLAGS="-D__MSVCRT_VERSION__=0x0601 $CFLAGS"

    AC_MSG_CHECKING(for PlibC)
    plibc=0
    AC_ARG_WITH(plibc,
       [  --with-plibc=PFX    Base of PliBC installation],
       [AC_MSG_RESULT([$with_plibc])
        case $with_plibc in
          no)
            ;;
          yes)
            AC_CHECK_HEADERS([plibc.h],
              AC_CHECK_LIB([plibc], [plibc_init],
                plibc=1))
            ;;
          *)
            LDFLAGS="-L$with_plibc/lib $LDFLAGS"
            CPPFLAGS="-I$with_plibc/include $CPPFLAGS"
            AC_CHECK_HEADERS([plibc.h],
              AC_CHECK_LIB([plibc], [plibc_init],
                EXT_LIB_PATH="-L$with_plibc/lib $EXT_LIB_PATH"
                plibc=1))
            ;;
        esac
       ],
       [AC_MSG_RESULT([--with-plibc not specified])
        LDFLAGS="-L/usr/lib $LDFLAGS"
        CPPFLAGS="-I/usr/include $CPPFLAGS"    
        AC_CHECK_HEADERS([plibc.h],
              AC_CHECK_LIB([plibc], [plibc_init],
                EXT_LIB_PATH="-L$with_plibc/lib $EXT_LIB_PATH"
                plibc=1))])

     if test $plibc -ne 1;
     then
        AC_MSG_ERROR([libextractor requires PlibC])
     else
        LIBS="$LIBS -lplibc"
     fi

     LDFLAGS="$LDFLAGS -Wl,-no-undefined -Wl,--export-all-symbols"
		 LIBSOLD=$LIBS
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(WINDOWS, true)
     LIBEXT=.dll
     ;;
*)
     AC_MSG_RESULT(Unrecognised OS $host_os)
     AC_DEFINE_UNQUOTED(OTHEROS,1,[Some strange OS])
     AC_MSG_RESULT(otheros)
     AC_DEFINE(GNU_LINUX,1,[We hope this is a GNU/Linux-compatible system])
     AC_DEFINE_UNQUOTED(GNU_LINUX,1,[We hope this is a GNU/Linux-compatible system])
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(WINDOWS, false)
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


# Adam shostack suggests the following for Windows:
# -D_FORTIFY_SOURCE=2 -fstack-protector-all
AC_ARG_ENABLE(gcc-hardening,
   AS_HELP_STRING(--enable-gcc-hardening, enable compiler security checks),
[if test x$enableval = xyes; then
    CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2 -fstack-protector-all"
    CFLAGS="$CFLAGS -fwrapv -fPIE -Wstack-protector"
    CFLAGS="$CFLAGS --param ssp-buffer-size=1"
    LDFLAGS="$LDFLAGS -pie"
fi])


# Linker hardening options
# Currently these options are ELF specific - you can't use this with MacOSX
AC_ARG_ENABLE(linker-hardening,
  AS_HELP_STRING(--enable-linker-hardening, enable linker security fixups),
[if test x$enableval = xyes; then
   LDFLAGS="$LDFLAGS -z relro -z now"
fi])


# Checks for libraries.

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

AC_CHECK_LIB(bz2, BZ2_bzDecompress,
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

AC_CHECK_LIB(mp4v2, MP4ReadProvider,
        [AC_CHECK_HEADERS([mp4v2/mp4v2.h],
           AM_CONDITIONAL(HAVE_MP4, true)
           AC_DEFINE(HAVE_MP4,1,[Have libmp4v2]),
         AM_CONDITIONAL(HAVE_MP4, false))],
         AM_CONDITIONAL(HAVE_MP4, false))

AC_CHECK_LIB(jpeg, jpeg_mem_src,
        [AC_CHECK_HEADERS([jpeglib.h],
           AM_CONDITIONAL(HAVE_JPEG, true)
           AC_DEFINE(HAVE_JPEG,1,[Have libjpeg]),
         AM_CONDITIONAL(HAVE_JPEG, false))],
         AM_CONDITIONAL(HAVE_JPEG, false))

AC_CHECK_LIB(tiff, TIFFClientOpen,
        [AC_CHECK_HEADERS([tiffio.h],
           AM_CONDITIONAL(HAVE_TIFF, true)
           AC_DEFINE(HAVE_TIFF,1,[Have libtiff]),
         AM_CONDITIONAL(HAVE_TIFF, false))],
         AM_CONDITIONAL(HAVE_TIFF, false))

AC_CHECK_LIB(archive, archive_read_open,
        [AC_CHECK_HEADERS([archive.h],
           AM_CONDITIONAL(HAVE_ARCHIVE, true)
           AC_DEFINE(HAVE_ARCHIVE,1,[Have libarchive]),
         AM_CONDITIONAL(HAVE_ARCHIVE, false))],
         AM_CONDITIONAL(HAVE_ARCHIVE, false))

# should experimental code be compiled (code that may not yet compile)?
AC_MSG_CHECKING(whether to compile experimental code)
AC_ARG_ENABLE([experimental],
   [AS_HELP_STRING([--enable-experimental], [enable compiling experimental code])],
   [enable_experimental=${enableval}],
   [enable_experimental=no])
AC_MSG_RESULT($enable_experimental)
AM_CONDITIONAL([HAVE_EXPERIMENTAL], [test "x$enable_experimental" = "xyes"])


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


AC_MSG_CHECKING(for DGifOpen -lgif)
SAVED_LDFLAGS=$LDFLAGS
AC_CHECK_LIB(gif, DGifOpen,
  [AC_CHECK_HEADERS([gif_lib.h],
   AM_CONDITIONAL(HAVE_GIF, true),
   AM_CONDITIONAL(HAVE_GIF, false))],
  AM_CONDITIONAL(HAVE_GIF, false))

AC_MSG_CHECKING(for magic_open -lmagic)
SAVED_LDFLAGS=$LDFLAGS
AC_CHECK_LIB(magic, magic_open,
  [AC_CHECK_HEADERS([magic.h],
   AM_CONDITIONAL(HAVE_MAGIC, true),
   AM_CONDITIONAL(HAVE_MAGIC, false))],
  AM_CONDITIONAL(HAVE_MAGIC, false))

AC_MSG_CHECKING(for tidyInitSource -ltidy)
SAVED_LDFLAGS=$LDFLAGS
AC_CHECK_LIB(tidy, tidyInitSource,
  [AC_CHECK_HEADERS([tidy/tidy.h],
   AM_CONDITIONAL(HAVE_TIDY, true),
   AM_CONDITIONAL(HAVE_TIDY, false))],
  AM_CONDITIONAL(HAVE_TIDY, false))

# restore LIBS
LIBS=$LIBSOLD



# should 'make check' run tests?
AC_MSG_CHECKING(whether to run tests)
AC_ARG_ENABLE([testruns],
   [AS_HELP_STRING([--disable-testruns], [disable running tests on make check (default is YES)])],
   [enable_tests_run=${enableval}],
   [enable_tests_run=yes])
AC_MSG_RESULT($enable_test_run)
AM_CONDITIONAL([ENABLE_TEST_RUN], [test "x$enable_tests_run" = "xyes"])



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
AC_CHECK_FUNCS([mkstemp strndup munmap strcasecmp strdup strncasecmp memmove memset strtoul floor getcwd pow setenv sqrt strchr strcspn strrchr strnlen strndup ftruncate shm_open shm_unlink lseek64])
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

# smf requires glib.h
CFLAGS_OLD=$CFLAGS
CPPFLAGS_OLD=$CPPFLAGS
export CFLAGS="$CFLAGS $GLIB_CFLAGS"
export CPPFLAGS="$CPPFLAGS $GLIB_CFLAGS"

AC_CHECK_LIB(smf, smf_load_from_memory,
        [AC_CHECK_HEADERS([smf.h],
           AM_CONDITIONAL(HAVE_SMF, true)
           AC_DEFINE(HAVE_MPEG2,1,[Have libsmf]),
         AM_CONDITIONAL(HAVE_SMF, false))],
         AM_CONDITIONAL(HAVE_SMF, false))
export CFLAGS=$CFLAGS_OLD
export CPPFLAGS=$CPPFLAGS_OLD

# check for gtk >= 2.6.0
AC_MSG_CHECKING(for gtk)
check_for_3=3.0.0
check_for_2=false
AC_ARG_WITH(gtk_version,
   [  --with-gtk-version=VERSION    version number of gtk to use (>=3.0.0 by default)],
   [AC_MSG_RESULT([$with_gtk_version])
    case $with_gtk_version in
      *)
        if test "x${with_gtk_version:0:1}" == "x2"
        then
          check_for_3=false
          check_for_2=$with_gtk_version
        elif test "x${with_gtk_version:0:1}" == "x3"
        then
          check_for_3=$with_gtk_version
          check_for_2=false
        fi
        ;;
    esac
   ],
   [AC_MSG_RESULT([--with-gtk-version not specified])])

without_gtk=false
if test "x$check_for_3" != "xfalse"
then
  AM_PATH_GTK_3_0([$check_for_3],without_gtk=false,without_gtk=true)
fi

if test "x$without_gtk" == "xfalse" -a "x$check_for_2" != "xfalse"
then
  AM_PATH_GTK_2_0([$check_for_2],without_gtk=false,without_gtk=true)
fi

AM_CONDITIONAL(HAVE_GTK, test x$without_gtk != xtrue)
if test $without_gtk != true
then
  AC_DEFINE_UNQUOTED([HAVE_GTK], 1, [We have GTK])
else
  AC_MSG_NOTICE([Cannot find GTK: Is pkg-config in path?])
fi

CFLAGS="$CFLAGS $GTK_CFLAGS"
CPPFLAGS="$CPPFLAGS $GTK_CFLAGS"
LIBS="$LIBS $GTK_LIBS"


AC_ARG_WITH([gstreamer], AS_HELP_STRING([--with-gstreamer], [Build with the GStreamer plugin]), [], [with_gstreamer=yes])

have_gstreamer=no
have_gstreamer_pbutils=no
have_gstreamer_tag=no
have_gstreamer_app=no
AS_IF([test "x$with_gstreamer" = "xyes"], [
   PKG_CHECK_MODULES([GSTREAMER], [gstreamer-1.0 >= 0.11.93], [have_gstreamer=yes], [have_gstreamer=no])
   PKG_CHECK_MODULES([GSTREAMER_PBUTILS], [gstreamer-pbutils-1.0 >= 0.11.93], [have_gstreamer_pbutils=yes], [have_gstreamer_pbutils=no])
   PKG_CHECK_MODULES([GSTREAMER_TAG], [gstreamer-tag-1.0 >= 0.11.93], [have_gstreamer_tag=yes], [have_gstreamer_tag=no])
   PKG_CHECK_MODULES([GSTREAMER_APP], [gstreamer-app-1.0 >= 0.11.93], [have_gstreamer_app=yes], [have_gstreamer_app=no])
])

AM_CONDITIONAL(HAVE_GSTREAMER, test x$have_gstreamer = xyes -a x$have_gstreamer_pbutils = xyes -a x$have_gstreamer_tag = xyes -a x$have_gstreamer_app = xyes -a ! x$without_glib = xtrue)

ABI_GSF

AM_CONDITIONAL(HAVE_GSF, test "x$have_gsf" = "xtrue")

# produce new line
echo ""
AC_CHECK_LIB(gsf-1, gsf_init, AC_DEFINE(HAVE_GSF_INIT,1,[gsf_init supported]))

AC_CHECK_PROG([HAVE_ZZUF],[zzuf], 1, 0)
AM_CONDITIONAL(HAVE_ZZUF, test 0 != $HAVE_ZZUF)

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
  [     AC_MSG_RESULT(yes)
        ffmpeg_enabled=1])
if test x$ffmpeg_enabled = x1
then
  ffmpeg_enabled=0
  AC_CHECK_LIB(swscale, sws_getContext,
    ffmpeg_enabled=1)
  AC_CHECK_HEADERS([libavutil/avutil.h ffmpeg/avutil.h libavformat/avformat.h ffmpeg/avformat.h libavcodec/avcodec.h ffmpeg/avcodec.h libswscale/swscale.h ffmpeg/swscale.h])
fi
AM_CONDITIONAL(HAVE_FFMPEG, test x$ffmpeg_enabled != x0)


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


# TODO insert a proper check here
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
 LE_LIB_LDFLAGS="$LE_LIB_LDFLAGS -export-symbols-regex \"(EXTRACTOR|pl)_@<:@a-zA-Z0-9_@:>@*\""
 LE_PLUGIN_LDFLAGS="$LE_PLUGIN_LDFLAGS -export-symbols-regex \"(EXTRACTOR|pl)_@<:@a-zA-Z0-9_@:>@*_.......\""
fi

# restore LIBS
LIBS=$LIBSOLD

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

# gcov compilation
AC_MSG_CHECKING(whether to compile with support for code coverage analysis)
AC_ARG_ENABLE([coverage], 
              AS_HELP_STRING([--enable-coverage],
                             [compile the library with code coverage support]),
              [use_gcov=${enableval}], 
              [use_gcov=no])
AC_MSG_RESULT($use_gcov)
AM_CONDITIONAL([USE_COVERAGE], [test "x$use_gcov" = "xyes"])



AC_CONFIG_FILES([Makefile
po/Makefile.in 
m4/Makefile
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
 AC_MSG_NOTICE([NOTICE: zlib not found, zlib support disabled])
fi

if test "x$HAVE_BZ2_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libbz2 not found, bz2 support disabled])
fi

if test "x$HAVE_EXIV2_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libexiv2 not found, exiv2 disabled])
fi

if test "x$HAVE_TIFF_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libtiff not found, tiff disabled])
fi

if test "x$HAVE_JPEG_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libjpeg not found, jpeg disabled])
fi

if test "x$HAVE_GIF_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libgif not found, gif disabled])
fi

if test "x$have_gsf" != "xtrue"
then
 AC_MSG_NOTICE([NOTICE: libgsf not found, no OLE2 (MS Office) support])
fi

if test "x$ffmpeg_enabled" = "x0"
then
 AC_MSG_NOTICE([NOTICE: FFmpeg thumbnailer plugin disabled])
fi

if test "x$without_gtk" = "xtrue" 
then
 AC_MSG_NOTICE([NOTICE: libgtk2+ not found, gtk thumbnail support disabled])
fi

if test "x$HAVE_VORBISFILE_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libvorbis not found, vorbis support disabled])
fi

if test "x$HAVE_FLAC_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libflac not found, flac support disabled])
fi

if test "x$HAVE_SMF_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libsmf not found, midi support disabled])
fi

if test "x$HAVE_MPEG2_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libmpeg2 not found, mpeg2 support disabled])
fi

if test "x$HAVE_MP4V2_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libmp4v2 not found, mp4 support disabled])
fi

if test "x$HAVE_CXX" != "xyes"
then
 AC_MSG_NOTICE([NOTICE: no C++ compiler found (not compiling plugins that require C++)])
fi

if test x$have_gstreamer = xyes -a x$have_gstreamer_pbutils = xyes -a x$have_gstreamer_tag = xyes -a x$have_gstreamer_app = xyes -a ! x$without_glib = xtrue
then
if test x$enable_experimental = xyes
then
 AC_MSG_NOTICE([NOTICE: gstreamer enabled])
fi
else
 AC_MSG_NOTICE([NOTICE: gstreamer not found, gstreamer support disabled])
fi

if test $without_gtk != false
then
  AC_MSG_NOTICE([NOTICE: gtk+ not found, gtk support disabled])
fi
