/*
     This file is part of GNUnet.
     Copyright (C) 2001, 2002, 2003, 2004, 2005 Christian Grothoff (and other contributing authors)

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/

/**
 * @file include/platform.h
 * @brief plaform specifics
 *
 * @author Nils Durner
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "config.h"
#ifndef FRAMEWORK_BUILD
#include "gettext.h"
#define _(a) dgettext(PACKAGE, a)
#else
#include "libintlemu.h"
#define _(a) dgettext("org.gnunet.libextractor", a)
#endif

#include "plibc.h"

#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MINGW
 #include <sys/socket.h>
 #include <sys/mman.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
#else
 #include <winsock2.h>
#endif
#include <locale.h>
#if HAVE_ICONV_H
#include <iconv.h>
#endif
#include <langinfo.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#if DARWIN
#include <mach-o/dyld.h>
#include <mach-o/ldsyms.h>
#endif

#if !WINDOWS
#define ABORT() abort()
#else
#define ABORT() DebugBreak ()
#endif

#endif
