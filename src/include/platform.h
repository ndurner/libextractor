/*
     This file is part of GNUnet.
     (C) 2001 - 2005 Christian Grothoff (and other contributing authors)

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
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
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
#include "gettext.h"
#define _(a) dgettext(PACKAGE, a)

#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
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
 #include <pthread.h>
#endif

#ifndef MINGW
 #define DIR_SEPARATOR '/'
 
 #define CREAT(p, m) creat(p, m)
 #undef FOPEN
 #define FOPEN(f, m) fopen(f, m)
 #define OPENDIR(d) opendir(d)
 #define CHDIR(d) chdir(d)
 #define RMDIR(f) rmdir(f)
 #define ACCESS(p, m) access(p, m)
 #define CHMOD(f, p) chmod(f, p)
 #define FSTAT(h, b) fstat(h, b)
 #define PIPE(h) pipe(h)
 #define REMOVE(p) remove(p)
 #define RENAME(o, n) rename(o, n)
 #define STAT(p, b) stat(p, b)
 #define UNLINK(f) unlink(f)
 #define WRITE(f, b, n) write(f, b, n)
 #define READ(f, b, n) read(f, b, n)
 #define GN_FREAD(b, s, c, f) fread(b, s, c, f)
 #define GN_FWRITE(b, s, c, f) fwrite(b, s, c, f)
 #define MMAP(s, l, p, f, d, o) mmap(s, l, p, f, d, o)
 #define MUNMAP(s, l) munmap(s, l);
 #define STRERROR(i) strerror(i)
#else

 #include "winproc.h"


 #define DIR_SEPARATOR '\\'

 #define int64_t long long
 #define int32_t long

 #define CREAT(p, m) _win_creat(p, m)
 #define FOPEN(f, m) _win_fopen(f, m)
 #define OPENDIR(d) _win_opendir(d)
 #define CHDIR(d) _win_chdir(d)
 #define FSTAT(h, b) _win_fstat(h, b)
 #define RMDIR(f) _win_rmdir(f)
 #define ACCESS(p, m) _win_access(p, m)
 #define CHMOD(f, p) _win_chmod(f, p)
 #define PIPE(h) _win_pipe(h)
 #define REMOVE(p) _win_remove(p)
 #define RENAME(o, n) _win_rename(o, n)
 #define STAT(p, b) _win_stat(p, b)
 #define UNLINK(f) _win_unlink(f)
 #define WRITE(f, b, n) _win_write(f, b, n)
 #define READ(f, b, n) _win_read(f, b, n)
 #define GN_FREAD(b, s, c, f) _win_fread(b, s, c, f)
 #define GN_FWRITE(b, s, c, f) _win_fwrite(b, s, c, f)
 #define MMAP(s, l, p, f, d, o) _win_mmap(s, l, p, f, d, o)
 #define MUNMAP(s, l) _win_munmap(s, l)
 #define STRERROR(i) _win_strerror(i)
#endif

#ifdef OSX
 #define socklen_t unsigned int
#endif

#endif
