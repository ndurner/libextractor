# Process this file with autoconf to produce a configure script.
#
#  This file is part of GNU libextractor
#  Copyright (C) 2014, 2017 GNUnet e.V.
#
#  GNU libextractor is free software; you can redistribute it and/or modify it under the
#  terms of the GNU General Public License as published by the Free Software
#  Foundation; either version 3, or (at your option) any later version.
#
#  GNU libextractor is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
#  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License along with
#  GNU libextractor; see the file COPYING.  If not, If not, see <http://www.gnu.org/license>
#
#
AC_PREREQ(2.61)
AC_INIT([libextractor], [1.4], [bug-libextractor@gnu.org])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AH_TOP([#define _GNU_SOURCE  1])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CANONICAL_SYSTEM

LIB_VERSION_CURRENT=4
LIB_VERSION_REVISION=4
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
AC_PROG_LIBTOOL
# save LIBS, libtool does a AC_SEARCH_LIBS(dlopen, dl), but plugins
# need not have -ldl added
LT_INIT([disable-static dlopen win32-dll])
AC_SUBST(MKDIR_P)

AC_CHECK_DECLS([_stati64])

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
     AM_CFLAGS="-D_POSIX_PTHREAD_SEMANTICS $AM_CFLAGS"
     LIBEXT=.so
     ;;
darwin*)
     AC_DEFINE_UNQUOTED(DARWIN,1,[This is a Darwin system])
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(WINDOWS, false)
     AM_CFLAGS="-fno-common $AM_CFLAGS"
     LIBEXT=.so
     ;;
cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a CYGWIN system])
     AM_CONDITIONAL(SOMEBSD, false)
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     AM_CONDITIONAL(WINDOWS, false)
     LIBEXT=.dll
     ;;
mingw*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_LIB(intl, gettext)
     # Sufficiently new Windows XP
     AM_CFLAGS="-D__MSVCRT_VERSION__=0x0601 $AM_CFLAGS"
     AM_CPPFLAGS="-DFTRUNCATE_DEFINED=1 $AM_CPPFLAGS"

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
            AM_LDFLAGS="-L$with_plibc/lib $AM_LDFLAGS"
            AM_CPPFLAGS="-I$with_plibc/include $AM_CPPFLAGS"
            AC_CHECK_HEADERS([plibc.h],
              AC_CHECK_LIB([plibc], [plibc_init],
                EXT_LIB_PATH="-L$with_plibc/lib $EXT_LIB_PATH"
                plibc=1))
            ;;
        esac
       ],
       [AC_MSG_RESULT([--with-plibc not specified])
        AM_LDFLAGS="-L/usr/lib $AM_LDFLAGS"
        AM_CPPFLAGS="-I/usr/include $AM_CPPFLAGS"
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

     AM_LDFLAGS="$AM_LDFLAGS -Wl,--export-all-symbols"
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
 AM_CFLAGS="-fno-strict-aliasing $AM_CFLAGS"
fi

AM_CONDITIONAL(HAVE_CXX, test "x$HAVE_CXX" = "xyes")


# Adam shostack suggests the following for Windows:
# -D_FORTIFY_SOURCE=2 -fstack-protector-all
AC_ARG_ENABLE(gcc-hardening,
   AS_HELP_STRING(--enable-gcc-hardening, enable compiler security checks),
[if test x$enableval = xyes; then
    AM_CFLAGS="$AM_CFLAGS -D_FORTIFY_SOURCE=2 -fstack-protector-all"
    AM_CFLAGS="$AM_CFLAGS -fwrapv -fPIE -Wstack-protector"
    AM_CFLAGS="$AM_CFLAGS --param ssp-buffer-size=1"
    AM_LDFLAGS="$AM_LDFLAGS -pie"
fi])


# Linker hardening options
# Currently these options are ELF specific - you can't use this with MacOSX
AC_ARG_ENABLE(linker-hardening,
  AS_HELP_STRING(--enable-linker-hardening, enable linker security fixups),
[if test x$enableval = xyes; then
   AM_LDFLAGS="$AM_LDFLAGS -z relro -z now"
fi])


# Checks for libraries.

AC_CHECK_HEADERS(langinfo.h)

# Check for libltdl header (#2999)
ltdl=0
AC_MSG_CHECKING(for libltdl)
AC_ARG_WITH(ltdl,
   [  --with-ltdl=PFX    base of libltdl installation],
   [AC_MSG_RESULT([$with_ltdl])
    case $with_ltdl in
      no)
        ;;
      yes)
        AC_CHECK_HEADERS(ltdl.h,
          AC_CHECK_LIB([ltdl], [lt_dlopenext],
            ltdl=1))
        ;;
      *)
        OLD_LIBS="$LIBS"
        LIBS="-L$with_ltdl/lib $LIBS"
        AM_CPPFLAGS="-I$with_ltdl/include $AM_CPPFLAGS"
        AC_CHECK_HEADERS(ltdl.h,
          AC_CHECK_LIB([ltdl], [lt_dlopenext],
            EXT_LIB_PATH="-L$with_ltdl/lib $EXT_LIB_PATH"
            ltdl=1))
        LIBS="$OLD_LIBS"
        ;;
    esac
   ],
   [AC_MSG_RESULT([--with-ltdl not specified])
    AC_CHECK_HEADERS(ltdl.h,
     AC_CHECK_LIB([ltdl], [lt_dlopenext],
      ltdl=1))])
if test x$ltdl = x1
then
 AC_MSG_RESULT([libltdl found])
else
 AC_MSG_ERROR([libextractor requires libltdl (from GNU libtool), try installing libltdl-dev])
fi

# check for apparmor
AC_CHECK_LIB(apparmor, aa_change_profile,
        [AC_CHECK_HEADERS([sys/apparmor.h],
          AM_CONDITIONAL(HAVE_APPARMOR, true)
          AC_DEFINE(HAVE_APPARMOR,1,[Have apparmor]),
          AM_CONDITIONAL(HAVE_APPARMOR, false)
          AC_DEFINE(HAVE_APPARMOR,0,[lacking apparmor]))],
        AM_CONDITIONAL(HAVE_APPARMOR, false),
        -lapparmor)

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
SAVED_AM_LDFLAGS=$AM_LDFLAGS
AM_LDFLAGS="$AM_LDFLAGS -lexiv2"
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
AM_LDFLAGS=$SAVED_AM_LDFLAGS
AC_LANG_POP(C++)


AC_MSG_CHECKING(for DGifOpen -lgif)
SAVED_AM_LDFLAGS=$AM_LDFLAGS
AC_CHECK_LIB(gif, DGifOpen,
  [AC_CHECK_HEADERS([gif_lib.h],
   AM_CONDITIONAL(HAVE_GIF, true),
   AM_CONDITIONAL(HAVE_GIF, false))],
  AM_CONDITIONAL(HAVE_GIF, false))

AC_MSG_CHECKING(for magic_open -lmagic)
SAVED_AM_LDFLAGS=$AM_LDFLAGS
AC_CHECK_LIB(magic, magic_open,
  [AC_CHECK_HEADERS([magic.h],
   AM_CONDITIONAL(HAVE_MAGIC, true),
   AM_CONDITIONAL(HAVE_MAGIC, false))],
  AM_CONDITIONAL(HAVE_MAGIC, false))

AC_MSG_CHECKING(for tidyNodeGetValue -ltidy)
AC_LANG_PUSH(C++)
SAVED_LIBS=$LIBS
LIBS="$LIBS -ltidy"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([[#include <tidy/tidy.h>]],
    [[ Bool b = tidyNodeGetValue (NULL, NULL, NULL); ]])],
  [AC_MSG_RESULT(yes)
   AM_CONDITIONAL(HAVE_TIDY, true)
   AC_DEFINE(HAVE_TIDY,1,[Have tidyNodeGetValue in libtidy])],
  [AC_MSG_RESULT(no)
   AM_CONDITIONAL(HAVE_TIDY, false)])
LIBS=$SAVED_LIBS
AC_LANG_POP(C++)

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


dnl This is kind of tedious, but simple and straightforward
sockets=no
AC_MSG_CHECKING(for sockets)
AC_LANG_PUSH(C)
SAVED_LIBS="$LIBS"
SOCKET_LIBS=
AC_LINK_IFELSE(
  [
    AC_LANG_PROGRAM(
      [[#include <sys/socket.h>]],
      [[int s = socket (AF_INET, SOCK_STREAM, 6);]]
    )
  ],
  [
    sockets=yes
  ],
  [
    LIBS="$SAVED_LIBS -lsocket"
    AC_LINK_IFELSE(
      [
        AC_LANG_PROGRAM(
          [[#include <sys/socket.h>]],
          [[int s = socket (AF_INET, SOCK_STREAM, 6);]]
        )
      ],
      [
        sockets=yes
        SOCKET_LIBS="-lsocket"
      ],
      [
        AC_LINK_IFELSE(
          [
            AC_LANG_PROGRAM(
              [[#include <winsock2.h>]],
              [[int s = socket (AF_INET, SOCK_STREAM, 6);]]
            )
          ],
          [
            sockets=yes
          ],
          [
            LIBS="$SAVED_LIBS -lws2_32"
            AC_LINK_IFELSE(
              [
                AC_LANG_PROGRAM(
                  [[#include <winsock2.h>]],
                  [[int s = socket (AF_INET, SOCK_STREAM, 6);]]
                )
              ],
              [
                sockets=yes
                SOCKET_LIBS="-lws2_32"
              ],
              [
                sockets=no
              ]
            )
          ]
        )
      ]
    )
  ]
)
LIBS="$SAVED_LIBS $SOCKET_LIBS"
AC_LANG_POP(C)
AC_MSG_RESULT([$sockets])
if test "x$sockets" = "xno"
then
  AC_MSG_ERROR([libextractor requires some kind of socket library])
fi
AC_SUBST([SOCKET_LIBS],[$SOCKET_LIBS])

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
   esac],
  AC_MSG_RESULT(allowed))

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
CFLAGS_OLD="$CFLAGS"
CPPFLAGS_OLD="$CPPFLAGS"
CFLAGS="$AM_CFLAGS $GLIB_CFLAGS"
CPPFLAGS="$AM_CPPFLAGS $GLIB_CFLAGS"

AC_CHECK_LIB(smf, smf_load_from_memory,
        [AC_CHECK_HEADERS([smf.h],
           AM_CONDITIONAL(HAVE_SMF, true)
           AC_DEFINE(HAVE_MPEG2,1,[Have libsmf]),
         AM_CONDITIONAL(HAVE_SMF, false))],
         AM_CONDITIONAL(HAVE_SMF, false))

# check for gdk-pixbuf >= 2.4
AC_MSG_CHECKING(for gdk-pixbuf)
PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0 >= 2.4], [have_gdk_pixbuf=yes], [have_gdk_pixbuf=no])

AM_CONDITIONAL(HAVE_GDK_PIXBUF, test x$have_gdk_pixbuf != xno)
if test $have_gdk_pixbuf != no
then
  have_gdk_pixbuf=1
else
  have_gdk_pixbuf=0
  AC_MSG_NOTICE([Cannot find Gdk-pixbuf: Is pkg-config in path?])
fi
AC_DEFINE_UNQUOTED([HAVE_GDK_PIXBUF], [$have_gdk_pixbuf], [We have Gdk-pixbuf])

CFLAGS="$CFLAGS $GDK_PIXBUF_CFLAGS"
CPPFLAGS="$CPPFLAGS $GDK_PIXBUF_CFLAGS"
LIBS="$LIBS $GDK_PIXBUF_LIBS"


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
AC_CHECK_LIB(gsf-1, gsf_doc_meta_data_read_from_msole, AC_DEFINE(HAVE_GSF_DOC_META_DATA_READ_FROM_MSOLE,1,[gsf_doc_meta_data_read_from_msole supported]))


CFLAGS="$CFLAGS_OLD"
CPPFLAGS="$CPPFLAGS_OLD"


AC_CHECK_PROG([HAVE_ZZUF],[zzuf], 1, 0)
AM_CONDITIONAL(HAVE_ZZUF, test 0 != $HAVE_ZZUF)

AC_MSG_CHECKING([whether to enable the FFmpeg thumbnail extractor])
new_ffmpeg=0
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
  new_ffmpeg=0
  AC_CHECK_HEADERS([libavutil/avutil.h libavformat/avformat.h libavcodec/avcodec.h libavutil/frame.h],
    AC_CHECK_HEADERS([libavresample/avresample.h],
      AC_CHECK_LIB(avutil, av_audio_fifo_alloc,
        new_ffmpeg=1)))

  AC_CHECK_LIB(swscale, sws_getContext,
    AC_CHECK_LIB(avcodec, avcodec_alloc_context3,
      ffmpeg_enabled=1))
  AC_CHECK_HEADERS([libavutil/avutil.h ffmpeg/avutil.h libavformat/avformat.h ffmpeg/avformat.h libavcodec/avcodec.h ffmpeg/avcodec.h libswscale/swscale.h ffmpeg/swscale.h])
fi
AM_CONDITIONAL(HAVE_FFMPEG, test x$ffmpeg_enabled != x0)
AM_CONDITIONAL(HAVE_FFMPEG_NEW, test x$new_ffmpeg != x0)


LE_INTLINCL=""
LE_LIBINTL="$LTLIBINTL"
AC_ARG_ENABLE(framework, [  --enable-framework      enable Mac OS X framework build helpers],enable_framework_build=$enableval)
AM_CONDITIONAL(WANT_FRAMEWORK, test x$enable_framework_build = xyes)
if test x$enable_framework_build = xyes
then
  AC_DEFINE([FRAMEWORK_BUILD], 1, [Build a Mac OS X Framework])
  LE_INTLINCL='-I$(top_srcdir)/src/intlemu'
  LE_LIBINTL='$(top_builddir)/src/intlemu/libintlemu.la -framework CoreFoundation'
  AC_LIB_APPENDTOVAR([AM_CPPFLAGS], [$LE_INTLINCL])
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

if test "x$new_ffmpeg" = "x0"
then
 AC_MSG_NOTICE([NOTICE: FFmpeg/opus audio preview plugin disabled, It needs libav >= 10, or a FFmpeg with --enable-libavresample])
fi

if test "x$have_gdk_pixbuf" = "x0"
then
 AC_MSG_NOTICE([NOTICE: libgdk-pixbuf not found, gtk thumbnail support disabled])
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

if test "x$HAVE_APPARMOR_TRUE" = "x#"
then
 AC_MSG_NOTICE([NOTICE: libapparmor not found, apparmor support disabled])
fi
