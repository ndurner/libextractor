/*
     This file is part of libextractor.
     (C) 2008 Christian Grothoff (and other contributing authors)

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

#ifndef UNZIP_H_
#define UNZIP_H_

#include <zlib.h>

#define EXTRACTOR_UNZIP_OK                          (0)
#define EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE         (-100)
#define EXTRACTOR_UNZIP_ERRNO                       (Z_ERRNO)
#define EXTRACTOR_UNZIP_EOF                         (0)
#define EXTRACTOR_UNZIP_PARAMERROR                  (-102)
#define EXTRACTOR_UNZIP_BADZIPFILE                  (-103)
#define EXTRACTOR_UNZIP_INTERNALERROR               (-104)
#define EXTRACTOR_UNZIP_CRCERROR                    (-105)

typedef voidp EXTRACTOR_unzip_file;

typedef struct EXTRACTOR_unzip_filefunc_def_s
{
    voidpf ( *zopen_file) (voidpf opaque, const char* filename, int mode);
    uLong  ( *zread_file) (voidpf opaque, voidpf stream, void* buf, uLong size);
    uLong  ( *zwrite_file) (voidpf opaque, voidpf stream, const void* buf, uLong size);
    long   ( *ztell_file) (voidpf opaque, voidpf stream);
    long   ( *zseek_file) (voidpf opaque, voidpf stream, uLong offset, int origin);
    int    ( *zclose_file) (voidpf opaque, voidpf stream);
    int    ( *zerror_file) (voidpf opaque, voidpf stream);
    voidpf              opaque;
} EXTRACTOR_unzip_filefunc_def;

/* tm_unz contain date/time info */
typedef struct EXTRACTOR_unzip_tm_unz_s
{
    uInt tm_sec;            /* seconds after the minute - [0,59] */
    uInt tm_min;            /* minutes after the hour - [0,59] */
    uInt tm_hour;           /* hours since midnight - [0,23] */
    uInt tm_mday;           /* day of the month - [1,31] */
    uInt tm_mon;            /* months since January - [0,11] */
    uInt tm_year;           /* years - [1980..2044] */
} EXTRACTOR_unzip_tm_unz;

/* unz_file_info contain information about a file in the zipfile */
typedef struct EXTRACTOR_unzip_file_info_s
{
    uLong version;              /* version made by                 2 bytes */
    uLong version_needed;       /* version needed to extract       2 bytes */
    uLong flag;                 /* general purpose bit flag        2 bytes */
    uLong compression_method;   /* compression method              2 bytes */
    uLong dosDate;              /* last mod file date in Dos fmt   4 bytes */
    uLong crc;                  /* crc-32                          4 bytes */
    uLong compressed_size;      /* compressed size                 4 bytes */
    uLong uncompressed_size;    /* uncompressed size               4 bytes */
    uLong size_filename;        /* filename length                 2 bytes */
    uLong size_file_extra;      /* extra field length              2 bytes */
    uLong size_file_comment;    /* file comment length             2 bytes */

    uLong disk_num_start;       /* disk number start               2 bytes */
    uLong internal_fa;          /* internal file attributes        2 bytes */
    uLong external_fa;          /* external file attributes        4 bytes */

    EXTRACTOR_unzip_tm_unz tmu_date;
} EXTRACTOR_unzip_file_info;

int EXTRACTOR_common_unzip_string_file_name_compare(const char* fileName1,
    const char* fileName2, int iCaseSensitivity);

int EXTRACTOR_common_unzip_go_to_first_file(EXTRACTOR_unzip_file file);

EXTRACTOR_unzip_file EXTRACTOR_common_unzip_open2(const char *path,
    EXTRACTOR_unzip_filefunc_def* pzlib_filefunc_def);

int EXTRACTOR_common_unzip_close_current_file(EXTRACTOR_unzip_file file);

int EXTRACTOR_common_unzip_close(EXTRACTOR_unzip_file file);

int EXTRACTOR_common_unzip_get_current_file_info(EXTRACTOR_unzip_file file,
    EXTRACTOR_unzip_file_info *pfile_info, char *szFileName, uLong fileNameBufferSize,
    void *extraField, uLong extraFieldBufferSize, char *szComment,
    uLong commentBufferSize);

int EXTRACTOR_common_unzip_go_to_next_file(EXTRACTOR_unzip_file file);

int EXTRACTOR_common_unzip_local_file(EXTRACTOR_unzip_file file, const char *szFileName,
    int iCaseSensitivity);

int EXTRACTOR_common_unzip_read_current_file(EXTRACTOR_unzip_file file, voidp buf,
    unsigned len);

int EXTRACTOR_common_unzip_open_current_file3(EXTRACTOR_unzip_file file, int* method,
    int* level, int raw);

voidpf EXTRACTOR_common_unzip_zlib_open_file_func(voidpf opaque,
    const char* filename, int mode);

uLong EXTRACTOR_common_unzip_zlib_read_file_func(voidpf opaque, voidpf stream,
    void* buf, uLong size);

long EXTRACTOR_common_unzip_zlib_tell_file_func(voidpf opaque, voidpf stream);

long EXTRACTOR_common_unzip_zlib_seek_file_func(voidpf opaque, voidpf stream,
    uLong offset, int origin);

int EXTRACTOR_common_unzip_zlib_close_file_func(voidpf opaque, voidpf stream);

int EXTRACTOR_common_unzip_zlib_testerror_file_func(voidpf opaque,
    voidpf stream);

#endif /* UNZIP_H_ */
