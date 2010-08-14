/*
     This file is part of libextractor.
     (C) 2004, 2008 Vidyut Samanta and Christian Grothoff

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

/* This code is based in part on
   unzip 1.00 Copyright 1998-2003 Gilles Vollant
   http://www.winimage.com/zLibDll"
*/


#include "platform.h"
#include <ctype.h>
#include "extractor.h"
#include "unzip.h"

#define CASESENSITIVITY (0)
#define MAXFILENAME (256)

/* *********************** IOAPI ***************** */

#define ZLIB_FILEFUNC_SEEK_CUR (1)
#define ZLIB_FILEFUNC_SEEK_END (2)
#define ZLIB_FILEFUNC_SEEK_SET (0)

#define ZLIB_FILEFUNC_MODE_READ      (1)
#define ZLIB_FILEFUNC_MODE_WRITE     (2)
#define ZLIB_FILEFUNC_MODE_READWRITEFILTER (3)
#define ZLIB_FILEFUNC_MODE_EXISTING (4)
#define ZLIB_FILEFUNC_MODE_CREATE   (8)


#define ZREAD(filefunc,filestream,buf,size) ((*((filefunc).zread_file))((filefunc).opaque,filestream,buf,size))
#define ZWRITE(filefunc,filestream,buf,size) ((*((filefunc).zwrite_file))((filefunc).opaque,filestream,buf,size))
#define ZTELL(filefunc,filestream) ((*((filefunc).ztell_file))((filefunc).opaque,filestream))
#define ZSEEK(filefunc,filestream,pos,mode) ((*((filefunc).zseek_file))((filefunc).opaque,filestream,pos,mode))
#define ZCLOSE(filefunc,filestream) ((*((filefunc).zclose_file))((filefunc).opaque,filestream))
#define ZERROR(filefunc,filestream) ((*((filefunc).zerror_file))((filefunc).opaque,filestream))


/* ******************* former crypt.h ********************* */

/* unz_global_info structure contain global data about the ZIPfile
   These data comes from the end of central dir */
typedef struct unz_global_info_s
{
    uLong number_entry;         /* total number of entries in
                       the central dir on this disk */
    uLong size_comment;         /* size of the global comment of the zipfile */
} unz_global_info;

/*
  Read extra field from the current file (opened by unzOpenCurrentFile)
  This is the local-header version of the extra field (sometimes, there is
    more info in the local-header version than in the central-header)

  if buf==NULL, it return the size of the local extra field

  if buf!=NULL, len is the size of the buffer, the extra header is copied in
    buf.
  the return value is the number of bytes copied in buf, or (if <0)
    the error code
*/

#ifndef CASESENSITIVITYDEFAULT_NO
#  if !defined(unix) && !defined(CASESENSITIVITYDEFAULT_YES)
#    define CASESENSITIVITYDEFAULT_NO
#  endif
#endif


#ifndef UNZ_BUFSIZE
#define UNZ_BUFSIZE (16384)
#endif

#ifndef UNZ_MAXFILENAMEINZIP
#define UNZ_MAXFILENAMEINZIP (256)
#endif

#ifndef ALLOC
# define ALLOC(size) (malloc(size))
#endif

#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)



/* EXTRACTOR_unzip_file_info_interntal contain internal info about a file in zipfile*/
typedef struct unz_file_info_internal_s
{
    uLong offset_curfile;/* relative offset of local header 4 bytes */
} unz_file_info_internal;


/* file_in_zip_read_info_s contain internal information about a file in zipfile,
    when reading and decompress it */
typedef struct
{
    char  *read_buffer;         /* internal buffer for compressed data */
    z_stream stream;            /* zLib stream structure for inflate */

    uLong pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
    uLong stream_initialised;   /* flag set if stream structure is initialised*/

    uLong offset_local_extrafield;/* offset of the local extra field */
    uInt  size_local_extrafield;/* size of the local extra field */
    uLong pos_local_extrafield;   /* position in the local extra field in read*/

    uLong crc32;                /* crc32 of all data uncompressed */
    uLong crc32_wait;           /* crc32 we must obtain after decompress all */
    uLong rest_read_compressed; /* number of byte to be decompressed */
    uLong rest_read_uncompressed;/*number of byte to be obtained after decomp*/
    EXTRACTOR_unzip_filefunc_def z_filefunc;
    voidpf filestream;        /* io structore of the zipfile */
    uLong compression_method;   /* compression method (0==store) */
    uLong byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
    int   raw;
} file_in_zip_read_info_s;


/* unz_s contain internal information about the zipfile
*/
typedef struct
{
    EXTRACTOR_unzip_filefunc_def z_filefunc;
    voidpf filestream;        /* io structore of the zipfile */
    unz_global_info gi;       /* public global information */
    uLong byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
    uLong num_file;             /* number of the current file in the zipfile*/
    uLong pos_in_central_dir;   /* pos of the current file in the central dir*/
    uLong current_file_ok;      /* flag about the usability of the current file*/
    uLong central_pos;          /* position of the beginning of the central dir*/

    uLong size_central_dir;     /* size of the central directory  */
    uLong offset_central_dir;   /* offset of start of central directory with
                                   respect to the starting disk number */

    EXTRACTOR_unzip_file_info cur_file_info; /* public info about the current file in zip*/
    unz_file_info_internal cur_file_info_internal; /* private info about it*/
    file_in_zip_read_info_s* pfile_in_zip_read; /* structure about the current
                                        file if we are decompressing it */
    int encrypted;
} unz_s;



/* ===========================================================================
     Read a byte from a gz_stream; update next_in and avail_in. Return EOF
   for end of file.
   IN assertion: the stream s has been sucessfully opened for reading.
*/


static int 
unzlocal_getByte(const EXTRACTOR_unzip_filefunc_def* pzlib_filefunc_def,
		 voidpf filestream,
		 int *pi)
{
    unsigned char c;
    int err = (int)ZREAD(*pzlib_filefunc_def,filestream,&c,1);
    if (err==1)
    {
        *pi = (int)c;
        return EXTRACTOR_UNZIP_OK;
    }
    else
    {
        if (ZERROR(*pzlib_filefunc_def,filestream))
            return EXTRACTOR_UNZIP_ERRNO;
        else
            return EXTRACTOR_UNZIP_EOF;
    }
}


static int 
unzlocal_getShort (const EXTRACTOR_unzip_filefunc_def* pzlib_filefunc_def,
		   voidpf filestream,
		   uLong *pX)
{
  uLong x;
  int i;
  int err;
  
  err = unzlocal_getByte(pzlib_filefunc_def, filestream, &i);
  *pX = 0;
  if (err != EXTRACTOR_UNZIP_OK)
    return err;
  x = (uLong)i;
  err = unzlocal_getByte(pzlib_filefunc_def, filestream, &i);
  if (err != EXTRACTOR_UNZIP_OK)
    return err;
  x += ((uLong)i)<<8;  
  *pX = x;
  return err;
}

static int 
unzlocal_getLong (const EXTRACTOR_unzip_filefunc_def* pzlib_filefunc_def,
		  voidpf filestream,
		  uLong *pX)
{
  uLong x ;
  int i;
  int err;
  
  *pX = 0;
  err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
  if (err != EXTRACTOR_UNZIP_OK)
    return err;
  x = (uLong)i;
  err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
  if (err != EXTRACTOR_UNZIP_OK)
    return err;
  x += ((uLong)i)<<8;
  err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
  if (err != EXTRACTOR_UNZIP_OK)
    return err;
  x += ((uLong)i)<<16;
  err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
  if (err != EXTRACTOR_UNZIP_OK)
    return err;
  x += ((uLong)i)<<24;
  *pX = x;
  return err;
}


/* My own strcmpi / strcasecmp */
static int 
strcmpcasenosensitive_internal (const char* fileName1,
				const char* fileName2)
{
  while (1)
    {
      char c1=*(fileName1++);
      char c2=*(fileName2++);
      if ((c1>='a') && (c1<='z'))
	c1 -= 0x20;
      if ((c2>='a') && (c2<='z'))
	c2 -= 0x20;
      if (c1=='\0')
	return ((c2=='\0') ? 0 : -1);
      if (c2=='\0')
	return 1;
      if (c1<c2)
	return -1;
      if (c1>c2)
	return 1;
    }
}


#ifdef  CASESENSITIVITYDEFAULT_NO
#define CASESENSITIVITYDEFAULTVALUE 2
#else
#define CASESENSITIVITYDEFAULTVALUE 1
#endif

#ifndef STRCMPCASENOSENTIVEFUNCTION
#define STRCMPCASENOSENTIVEFUNCTION strcmpcasenosensitive_internal
#endif

/*
   Compare two filename (fileName1,fileName2).
   If iCaseSenisivity = 1, comparision is case sensitivity (like strcmp)
   If iCaseSenisivity = 2, comparision is not case sensitivity (like strcmpi
                                                                or strcasecmp)
   If iCaseSenisivity = 0, case sensitivity is defaut of your operating system
        (like 1 on Unix, 2 on Windows)

*/
int 
EXTRACTOR_common_unzip_string_file_name_compare (const char* fileName1,
						 const char* fileName2,
						 int iCaseSensitivity)
{
  if (iCaseSensitivity==0)
    iCaseSensitivity=CASESENSITIVITYDEFAULTVALUE;
  
  if (iCaseSensitivity==1)
    return strcmp(fileName1,fileName2);
  
  return STRCMPCASENOSENTIVEFUNCTION(fileName1,fileName2);
}

#ifndef BUFREADCOMMENT
#define BUFREADCOMMENT (0x400)
#endif

static uLong 
unzlocal_SearchCentralDir(const EXTRACTOR_unzip_filefunc_def* pzlib_filefunc_def,
			  voidpf filestream)
{
    unsigned char* buf;
    uLong uSizeFile;
    uLong uBackRead;
    uLong uMaxBack=0xffff; /* maximum size of global comment */
    uLong uPosFound=0;

    if (ZSEEK(*pzlib_filefunc_def,filestream,0,ZLIB_FILEFUNC_SEEK_END) != 0)
        return 0;


    uSizeFile = ZTELL(*pzlib_filefunc_def,filestream);

    if (uMaxBack>uSizeFile)
        uMaxBack = uSizeFile;

    buf = (unsigned char*)ALLOC(BUFREADCOMMENT+4);
    if (buf==NULL)
        return 0;

    uBackRead = 4;
    while (uBackRead<uMaxBack)
    {
        uLong uReadSize,uReadPos ;
        int i;
        if (uBackRead+BUFREADCOMMENT>uMaxBack)
            uBackRead = uMaxBack;
        else
            uBackRead+=BUFREADCOMMENT;
        uReadPos = uSizeFile-uBackRead ;

        uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ?
                     (BUFREADCOMMENT+4) : (uSizeFile-uReadPos);
        if (ZSEEK(*pzlib_filefunc_def,filestream,uReadPos,ZLIB_FILEFUNC_SEEK_SET)!=0)
            break;

        if (ZREAD(*pzlib_filefunc_def,filestream,buf,uReadSize)!=uReadSize)
            break;

        for (i=(int)uReadSize-3; (i--)>0;)
            if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) &&
                ((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06))
            {
                uPosFound = uReadPos+i;
                break;
            }

        if (uPosFound!=0)
            break;
    }
    free(buf);
    return uPosFound;
}

/*
   Translate date/time from Dos format to EXTRACTOR_unzip_tm_unz (readable more easilty)
*/
static void 
unzlocal_DosDateToTmuDate (uLong ulDosDate,
			   EXTRACTOR_unzip_tm_unz* ptm)
{
    uLong uDate;
    uDate = (uLong)(ulDosDate>>16);
    ptm->tm_mday = (uInt)(uDate&0x1f) ;
    ptm->tm_mon =  (uInt)((((uDate)&0x1E0)/0x20)-1) ;
    ptm->tm_year = (uInt)(((uDate&0x0FE00)/0x0200)+1980) ;

    ptm->tm_hour = (uInt) ((ulDosDate &0xF800)/0x800);
    ptm->tm_min =  (uInt) ((ulDosDate&0x7E0)/0x20) ;
    ptm->tm_sec =  (uInt) (2*(ulDosDate&0x1f)) ;
}



static int 
unzlocal_GetCurrentFileInfoInternal (EXTRACTOR_unzip_file file,
				     EXTRACTOR_unzip_file_info *pfile_info,
				     unz_file_info_internal *pfile_info_internal,
				     char *szFileName,
				     uLong fileNameBufferSize,
				     void *extraField,
				     uLong extraFieldBufferSize,
				     char *szComment,
				     uLong commentBufferSize)
{
    unz_s* s;
    EXTRACTOR_unzip_file_info file_info;
    unz_file_info_internal file_info_internal;
    int err=EXTRACTOR_UNZIP_OK;
    uLong uMagic;
    long lSeek=0;

    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;
    if (ZSEEK(s->z_filefunc, s->filestream,
              s->pos_in_central_dir+s->byte_before_the_zipfile,
              ZLIB_FILEFUNC_SEEK_SET)!=0)
        err=EXTRACTOR_UNZIP_ERRNO;


    /* we check the magic */
    if (err==EXTRACTOR_UNZIP_OK) {
        if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uMagic) != EXTRACTOR_UNZIP_OK)
            err=EXTRACTOR_UNZIP_ERRNO;
        else if (uMagic!=0x02014b50)
            err=EXTRACTOR_UNZIP_BADZIPFILE;
    }
    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.version) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.version_needed) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.flag) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.compression_method) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.dosDate) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    unzlocal_DosDateToTmuDate(file_info.dosDate,&file_info.tmu_date);

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.crc) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.compressed_size) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.uncompressed_size) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.size_filename) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.size_file_extra) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.size_file_comment) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.disk_num_start) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.internal_fa) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.external_fa) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info_internal.offset_curfile) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    lSeek+=file_info.size_filename;
    if ((err==EXTRACTOR_UNZIP_OK) && (szFileName!=NULL))
    {
        uLong uSizeRead ;
        if (file_info.size_filename<fileNameBufferSize)
        {
            *(szFileName+file_info.size_filename)='\0';
            uSizeRead = file_info.size_filename;
        }
        else
            uSizeRead = fileNameBufferSize;

        if ((file_info.size_filename>0) && (fileNameBufferSize>0))
            if (ZREAD(s->z_filefunc, s->filestream,szFileName,uSizeRead)!=uSizeRead)
                err=EXTRACTOR_UNZIP_ERRNO;
        lSeek -= uSizeRead;
    }


    if ((err==EXTRACTOR_UNZIP_OK) && (extraField!=NULL))
    {
        uLong uSizeRead ;
        if (file_info.size_file_extra<extraFieldBufferSize)
            uSizeRead = file_info.size_file_extra;
        else
            uSizeRead = extraFieldBufferSize;

        if (lSeek!=0) {
            if (ZSEEK(s->z_filefunc, s->filestream,lSeek,ZLIB_FILEFUNC_SEEK_CUR)==0)
                lSeek=0;
            else
                err=EXTRACTOR_UNZIP_ERRNO;
	}
        if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
            if (ZREAD(s->z_filefunc, s->filestream,extraField,uSizeRead)!=uSizeRead)
                err=EXTRACTOR_UNZIP_ERRNO;
        lSeek += file_info.size_file_extra - uSizeRead;
    }
    else
        lSeek+=file_info.size_file_extra;


    if ((err==EXTRACTOR_UNZIP_OK) && (szComment!=NULL))
    {
        uLong uSizeRead ;
        if (file_info.size_file_comment<commentBufferSize)
        {
            *(szComment+file_info.size_file_comment)='\0';
            uSizeRead = file_info.size_file_comment;
        }
        else
            uSizeRead = commentBufferSize;

        if (lSeek!=0) {
            if (ZSEEK(s->z_filefunc, s->filestream,lSeek,ZLIB_FILEFUNC_SEEK_CUR)==0)
                lSeek=0;
            else
                err=EXTRACTOR_UNZIP_ERRNO;
	}
        if ((file_info.size_file_comment>0) && (commentBufferSize>0))
            if (ZREAD(s->z_filefunc, s->filestream,szComment,uSizeRead)!=uSizeRead)
                err=EXTRACTOR_UNZIP_ERRNO;
        lSeek+=file_info.size_file_comment - uSizeRead;
    }
    else
        lSeek+=file_info.size_file_comment;

    if ((err==EXTRACTOR_UNZIP_OK) && (pfile_info!=NULL))
        *pfile_info=file_info;

    if ((err==EXTRACTOR_UNZIP_OK) && (pfile_info_internal!=NULL))
        *pfile_info_internal=file_info_internal;

    return err;
}

/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
int 
EXTRACTOR_common_unzip_go_to_first_file (EXTRACTOR_unzip_file file)
{
    int err=EXTRACTOR_UNZIP_OK;
    unz_s* s;
    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;
    s->pos_in_central_dir=s->offset_central_dir;
    s->num_file=0;
    err=unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
                                             &s->cur_file_info_internal,
                                             NULL,0,NULL,0,NULL,0);
    s->current_file_ok = (err == EXTRACTOR_UNZIP_OK);
    return err;
}


/*
  Open a Zip file. path contain the full pathname (by example,
     on a Windows NT computer "c:\\test\\zlib114.zip" or on an Unix computer
     "zlib/zlib114.zip".
     If the zipfile cannot be opened (file doesn't exist or in not valid), the
       return value is NULL.
     Else, the return value is a EXTRACTOR_unzip_file Handle, usable with other function
       of this unzip package.
*/
EXTRACTOR_unzip_file 
EXTRACTOR_common_unzip_open2 (const char *path,
			      EXTRACTOR_unzip_filefunc_def* pzlib_filefunc_def)
{
    unz_s us;
    unz_s *s;
    uLong central_pos,uL;

    uLong number_disk;          /* number of the current dist, used for
                                   spaning ZIP, unsupported, always 0*/
    uLong number_disk_with_CD;  /* number the the disk with central dir, used
                                   for spaning ZIP, unsupported, always 0*/
    uLong number_entry_CD;      /* total number of entries in
                                   the central dir
                                   (same than number_entry on nospan) */

    int err=EXTRACTOR_UNZIP_OK;

    memset (&us, 0, sizeof(us));	
    us.z_filefunc = *pzlib_filefunc_def;

    us.filestream= (*(us.z_filefunc.zopen_file))(us.z_filefunc.opaque,
                                                 path,
                                                 ZLIB_FILEFUNC_MODE_READ |
                                                 ZLIB_FILEFUNC_MODE_EXISTING);
    if (us.filestream==NULL)
        return NULL;

    central_pos = unzlocal_SearchCentralDir(&us.z_filefunc,us.filestream);
    if (central_pos==0)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (ZSEEK(us.z_filefunc, us.filestream,
                                      central_pos,ZLIB_FILEFUNC_SEEK_SET)!=0)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* the signature, already checked */
    if (unzlocal_getLong(&us.z_filefunc, us.filestream,&uL)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* number of this disk */
    if (unzlocal_getShort(&us.z_filefunc, us.filestream,&number_disk)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* number of the disk with the start of the central directory */
    if (unzlocal_getShort(&us.z_filefunc, us.filestream,&number_disk_with_CD)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* total number of entries in the central dir on this disk */
    if (unzlocal_getShort(&us.z_filefunc, us.filestream,&us.gi.number_entry)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* total number of entries in the central dir */
    if (unzlocal_getShort(&us.z_filefunc, us.filestream,&number_entry_CD)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if ((number_entry_CD!=us.gi.number_entry) ||
        (number_disk_with_CD!=0) ||
        (number_disk!=0))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    /* size of the central directory */
    if (unzlocal_getLong(&us.z_filefunc, us.filestream,&us.size_central_dir)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* offset of start of central directory with respect to the
          starting disk number */
    if (unzlocal_getLong(&us.z_filefunc, us.filestream,&us.offset_central_dir)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    /* zipfile comment length */
    if (unzlocal_getShort(&us.z_filefunc, us.filestream,&us.gi.size_comment)!=EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if ((central_pos<us.offset_central_dir+us.size_central_dir) &&
        (err==EXTRACTOR_UNZIP_OK))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    if (err!=EXTRACTOR_UNZIP_OK)
    {
        ZCLOSE(us.z_filefunc, us.filestream);
        return NULL;
    }

    us.byte_before_the_zipfile = central_pos -
                            (us.offset_central_dir+us.size_central_dir);
    us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;
    us.encrypted = 0;


    s=(unz_s*)ALLOC(sizeof(unz_s));
    if (s == NULL) 
    {
       ZCLOSE(us.z_filefunc, us.filestream);
       return NULL;
    }
    *s=us;
    EXTRACTOR_common_unzip_go_to_first_file((EXTRACTOR_unzip_file)s);
    return (EXTRACTOR_unzip_file)s;
}

/*
  Close the file in zip opened with unzipOpenCurrentFile
  Return EXTRACTOR_UNZIP_CRCERROR if all the file was read but the CRC is not good
*/
int 
EXTRACTOR_common_unzip_close_current_file (EXTRACTOR_unzip_file file)
{
    int err=EXTRACTOR_UNZIP_OK;

    unz_s* s;
    file_in_zip_read_info_s* pfile_in_zip_read_info;
    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

    if (pfile_in_zip_read_info==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;


    if ((pfile_in_zip_read_info->rest_read_uncompressed == 0) &&
        (!pfile_in_zip_read_info->raw))
    {
        if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
            err=EXTRACTOR_UNZIP_CRCERROR;
    }


    if (NULL != pfile_in_zip_read_info->read_buffer)
      free(pfile_in_zip_read_info->read_buffer);
    pfile_in_zip_read_info->read_buffer = NULL;
    if (pfile_in_zip_read_info->stream_initialised)
        inflateEnd(&pfile_in_zip_read_info->stream);

    pfile_in_zip_read_info->stream_initialised = 0;
    free(pfile_in_zip_read_info);

    s->pfile_in_zip_read=NULL;

    return err;
}

/*
  Close a ZipFile opened with unzipOpen.
  If there is files inside the .Zip opened with unzipOpenCurrentFile (see later),
    these files MUST be closed with unzipCloseCurrentFile before call unzipClose.
  return EXTRACTOR_UNZIP_OK if there is no problem. */
int EXTRACTOR_common_unzip_close (EXTRACTOR_unzip_file file)
{
    unz_s* s;
    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;

    if (s->pfile_in_zip_read!=NULL)
        EXTRACTOR_common_unzip_close_current_file(file);

    ZCLOSE(s->z_filefunc, s->filestream);
    free(s);
    return EXTRACTOR_UNZIP_OK;
}


/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return EXTRACTOR_UNZIP_OK if there is no problem.
*/
int EXTRACTOR_common_unzip_get_current_file_info (EXTRACTOR_unzip_file file,
						  EXTRACTOR_unzip_file_info *pfile_info,
						  char *szFileName,
						  uLong fileNameBufferSize,
						  void *extraField,
						  uLong extraFieldBufferSize,
						  char *szComment,
						  uLong commentBufferSize)
{
    return unzlocal_GetCurrentFileInfoInternal(file,pfile_info,NULL,
                                                szFileName,fileNameBufferSize,
                                                extraField,extraFieldBufferSize,
                                                szComment,commentBufferSize);
}

/*
  Set the current file of the zipfile to the next file.
  return EXTRACTOR_UNZIP_OK if there is no problem
  return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
int EXTRACTOR_common_unzip_go_to_next_file (EXTRACTOR_unzip_file file)
{
    unz_s* s;
    int err;

    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;
    if (!s->current_file_ok)
        return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE;
    if (s->num_file+1==s->gi.number_entry)
        return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE;

    s->pos_in_central_dir += SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
            s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment ;
    s->num_file++;
    err = unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
                                               &s->cur_file_info_internal,
                                               NULL,0,NULL,0,NULL,0);
    s->current_file_ok = (err == EXTRACTOR_UNZIP_OK);
    return err;
}




/*
  Try locate the file szFileName in the zipfile.
  For the iCaseSensitivity signification, see unzipStringFileNameCompare

  return value :
  EXTRACTOR_UNZIP_OK if the file is found. It becomes the current file.
  EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE if the file is not found
*/
int EXTRACTOR_common_unzip_local_file (file, szFileName, iCaseSensitivity)
    EXTRACTOR_unzip_file file;
    const char *szFileName;
    int iCaseSensitivity;
{
    unz_s* s;
    int err;

    /* We remember the 'current' position in the file so that we can jump
     * back there if we fail.
     */
    EXTRACTOR_unzip_file_info cur_file_infoSaved;
    unz_file_info_internal cur_file_info_internalSaved;
    uLong num_fileSaved;
    uLong pos_in_central_dirSaved;


    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;

    if (strlen(szFileName)>=UNZ_MAXFILENAMEINZIP)
        return EXTRACTOR_UNZIP_PARAMERROR;

    s=(unz_s*)file;
    if (!s->current_file_ok)
        return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE;

    /* Save the current state */
    num_fileSaved = s->num_file;
    pos_in_central_dirSaved = s->pos_in_central_dir;
    cur_file_infoSaved = s->cur_file_info;
    cur_file_info_internalSaved = s->cur_file_info_internal;

    err = EXTRACTOR_common_unzip_go_to_first_file(file);

    while (err == EXTRACTOR_UNZIP_OK)
    {
        char szCurrentFileName[UNZ_MAXFILENAMEINZIP+1];
        err = EXTRACTOR_common_unzip_get_current_file_info(file,NULL,
                                    szCurrentFileName,sizeof(szCurrentFileName)-1,
                                    NULL,0,NULL,0);
        if (err == EXTRACTOR_UNZIP_OK)
        {
            if (EXTRACTOR_common_unzip_string_file_name_compare(szCurrentFileName,
                                            szFileName,iCaseSensitivity)==0)
                return EXTRACTOR_UNZIP_OK;
            err = EXTRACTOR_common_unzip_go_to_next_file(file);
        }
    }

    /* We failed, so restore the state of the 'current file' to where we
     * were.
     */
    s->num_file = num_fileSaved ;
    s->pos_in_central_dir = pos_in_central_dirSaved ;
    s->cur_file_info = cur_file_infoSaved;
    s->cur_file_info_internal = cur_file_info_internalSaved;
    return err;
}


/*
  Read bytes from the current file.
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
    (EXTRACTOR_UNZIP_ERRNO for IO error, or zLib error for uncompress error)
*/
int 
EXTRACTOR_common_unzip_read_current_file (EXTRACTOR_unzip_file file,
					  voidp buf,
					  unsigned len)
{
    int err=EXTRACTOR_UNZIP_OK;
    uInt iRead = 0;
    unz_s* s;
    file_in_zip_read_info_s* pfile_in_zip_read_info;
    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

    if (pfile_in_zip_read_info==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;


    if ((pfile_in_zip_read_info->read_buffer == NULL))
        return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE;
    if (len==0)
        return 0;

    pfile_in_zip_read_info->stream.next_out = (Bytef*)buf;

    pfile_in_zip_read_info->stream.avail_out = (uInt)len;

    if (len>pfile_in_zip_read_info->rest_read_uncompressed)
        pfile_in_zip_read_info->stream.avail_out =
          (uInt)pfile_in_zip_read_info->rest_read_uncompressed;

    while (pfile_in_zip_read_info->stream.avail_out>0)
    {
        if ((pfile_in_zip_read_info->stream.avail_in==0) &&
            (pfile_in_zip_read_info->rest_read_compressed>0))
        {
            uInt uReadThis = UNZ_BUFSIZE;
            if (pfile_in_zip_read_info->rest_read_compressed<uReadThis)
                uReadThis = (uInt)pfile_in_zip_read_info->rest_read_compressed;
            if (uReadThis == 0)
                return EXTRACTOR_UNZIP_EOF;
            if (ZSEEK(pfile_in_zip_read_info->z_filefunc,
                      pfile_in_zip_read_info->filestream,
                      pfile_in_zip_read_info->pos_in_zipfile +
                         pfile_in_zip_read_info->byte_before_the_zipfile,
                         ZLIB_FILEFUNC_SEEK_SET)!=0)
                return EXTRACTOR_UNZIP_ERRNO;
            if (ZREAD(pfile_in_zip_read_info->z_filefunc,
                      pfile_in_zip_read_info->filestream,
                      pfile_in_zip_read_info->read_buffer,
                      uReadThis)!=uReadThis)
                return EXTRACTOR_UNZIP_ERRNO;


            pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

            pfile_in_zip_read_info->rest_read_compressed-=uReadThis;

            pfile_in_zip_read_info->stream.next_in =
                (Bytef*)pfile_in_zip_read_info->read_buffer;
            pfile_in_zip_read_info->stream.avail_in = (uInt)uReadThis;
        }

        if ((pfile_in_zip_read_info->compression_method==0) || (pfile_in_zip_read_info->raw))
        {
            uInt uDoCopy,i ;

            if ((pfile_in_zip_read_info->stream.avail_in == 0) &&
                (pfile_in_zip_read_info->rest_read_compressed == 0))
                return (iRead==0) ? EXTRACTOR_UNZIP_EOF : iRead;

            if (pfile_in_zip_read_info->stream.avail_out <
                            pfile_in_zip_read_info->stream.avail_in)
                uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
            else
                uDoCopy = pfile_in_zip_read_info->stream.avail_in ;

            for (i=0;i<uDoCopy;i++)
                *(pfile_in_zip_read_info->stream.next_out+i) =
                        *(pfile_in_zip_read_info->stream.next_in+i);

            pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,
                                pfile_in_zip_read_info->stream.next_out,
                                uDoCopy);
            pfile_in_zip_read_info->rest_read_uncompressed-=uDoCopy;
            pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
            pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
            pfile_in_zip_read_info->stream.next_out += uDoCopy;
            pfile_in_zip_read_info->stream.next_in += uDoCopy;
            pfile_in_zip_read_info->stream.total_out += uDoCopy;
            iRead += uDoCopy;
        }
        else
        {
            uLong uTotalOutBefore,uTotalOutAfter;
            const Bytef *bufBefore;
            uLong uOutThis;
            int flush=Z_SYNC_FLUSH;

            uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
            bufBefore = pfile_in_zip_read_info->stream.next_out;

            /*
            if ((pfile_in_zip_read_info->rest_read_uncompressed ==
                     pfile_in_zip_read_info->stream.avail_out) &&
                (pfile_in_zip_read_info->rest_read_compressed == 0))
                flush = Z_FINISH;
            */
            err=inflate(&pfile_in_zip_read_info->stream,flush);

            uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
            uOutThis = uTotalOutAfter-uTotalOutBefore;

            pfile_in_zip_read_info->crc32 =
                crc32(pfile_in_zip_read_info->crc32,bufBefore,
                        (uInt)(uOutThis));

            pfile_in_zip_read_info->rest_read_uncompressed -=
                uOutThis;

            iRead += (uInt)(uTotalOutAfter - uTotalOutBefore);

            if (err==Z_STREAM_END)
                return (iRead==0) ? EXTRACTOR_UNZIP_EOF : iRead;
            if (err!=Z_OK)
                break;
        }
    }

    if (err==Z_OK)
        return iRead;
    return err;
}

/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
static int 
unzlocal_CheckCurrentFileCoherencyHeader (unz_s* s,
					  uInt* piSizeVar,
					  uLong *poffset_local_extrafield,
					  uInt  *psize_local_extrafield)
{
    uLong uMagic,uData,uFlags;
    uLong size_filename;
    uLong size_extra_field;
    int err = EXTRACTOR_UNZIP_OK;

    *piSizeVar = 0;
    *poffset_local_extrafield = 0;
    *psize_local_extrafield = 0;

    if (ZSEEK(s->z_filefunc, s->filestream,s->cur_file_info_internal.offset_curfile +
                                s->byte_before_the_zipfile,ZLIB_FILEFUNC_SEEK_SET)!=0)
      return EXTRACTOR_UNZIP_ERRNO;
    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uMagic) != EXTRACTOR_UNZIP_OK)
      err=EXTRACTOR_UNZIP_ERRNO;
    else if (uMagic!=0x04034b50)
      err=EXTRACTOR_UNZIP_BADZIPFILE;   
    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&uData) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;
    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&uFlags) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&uData) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;
    else if ((err==EXTRACTOR_UNZIP_OK) && (uData!=s->cur_file_info.compression_method))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    if ((err==EXTRACTOR_UNZIP_OK) && (s->cur_file_info.compression_method!=0) &&
                         (s->cur_file_info.compression_method!=Z_DEFLATED))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != EXTRACTOR_UNZIP_OK) /* date/time */
        err=EXTRACTOR_UNZIP_ERRNO;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != EXTRACTOR_UNZIP_OK) /* crc */
        err=EXTRACTOR_UNZIP_ERRNO;
    else if ((err==EXTRACTOR_UNZIP_OK) && (uData!=s->cur_file_info.crc) &&
                              ((uFlags & 8)==0))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != EXTRACTOR_UNZIP_OK) /* size compr */
        err=EXTRACTOR_UNZIP_ERRNO;
    else if ((err==EXTRACTOR_UNZIP_OK) && (uData!=s->cur_file_info.compressed_size) &&
                              ((uFlags & 8)==0))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != EXTRACTOR_UNZIP_OK) /* size uncompr */
        err=EXTRACTOR_UNZIP_ERRNO;
    else if ((err==EXTRACTOR_UNZIP_OK) && (uData!=s->cur_file_info.uncompressed_size) &&
                              ((uFlags & 8)==0))
        err=EXTRACTOR_UNZIP_BADZIPFILE;


    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&size_filename) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;
    else if ((err==EXTRACTOR_UNZIP_OK) && (size_filename!=s->cur_file_info.size_filename))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    *piSizeVar += (uInt)size_filename;

    if (unzlocal_getShort(&s->z_filefunc, s->filestream,&size_extra_field) != EXTRACTOR_UNZIP_OK)
        err=EXTRACTOR_UNZIP_ERRNO;
    *poffset_local_extrafield= s->cur_file_info_internal.offset_curfile +
                                    SIZEZIPLOCALHEADER + size_filename;
    *psize_local_extrafield = (uInt)size_extra_field;

    *piSizeVar += (uInt)size_extra_field;

    return err;
}



/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is EXTRACTOR_UNZIP_OK.
*/
int 
EXTRACTOR_common_unzip_open_current_file3 (EXTRACTOR_unzip_file file,
					   int* method,
					   int* level,
					   int raw)
{
    int err=EXTRACTOR_UNZIP_OK;
    uInt iSizeVar;
    unz_s* s;
    file_in_zip_read_info_s* pfile_in_zip_read_info;
    uLong offset_local_extrafield;  /* offset of the local extra field */
    uInt  size_local_extrafield;    /* size of the local extra field */

    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(unz_s*)file;
    if (!s->current_file_ok)
        return EXTRACTOR_UNZIP_PARAMERROR;

    if (s->pfile_in_zip_read != NULL)
        EXTRACTOR_common_unzip_close_current_file(file);
    if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar,
                &offset_local_extrafield,&size_local_extrafield)!=EXTRACTOR_UNZIP_OK)
        return EXTRACTOR_UNZIP_BADZIPFILE;

    pfile_in_zip_read_info = (file_in_zip_read_info_s*)
                                        ALLOC(sizeof(file_in_zip_read_info_s));
    if (pfile_in_zip_read_info==NULL)
        return EXTRACTOR_UNZIP_INTERNALERROR;

    pfile_in_zip_read_info->read_buffer=(char*)ALLOC(UNZ_BUFSIZE);
    pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
    pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
    pfile_in_zip_read_info->pos_local_extrafield=0;
    pfile_in_zip_read_info->raw=raw;

    if (pfile_in_zip_read_info->read_buffer==NULL)
      {
	free(pfile_in_zip_read_info);
        return EXTRACTOR_UNZIP_INTERNALERROR;
      }

    pfile_in_zip_read_info->stream_initialised=0;

    if (method!=NULL)
        *method = (int)s->cur_file_info.compression_method;

    if (level!=NULL)
    {
        *level = 6;
        switch (s->cur_file_info.flag & 0x06)
        {
          case 6 : *level = 1; break;
          case 4 : *level = 2; break;
          case 2 : *level = 9; break;
        }
    }

    if ((s->cur_file_info.compression_method!=0) &&
        (s->cur_file_info.compression_method!=Z_DEFLATED))
        err=EXTRACTOR_UNZIP_BADZIPFILE;

    pfile_in_zip_read_info->crc32_wait=s->cur_file_info.crc;
   pfile_in_zip_read_info->crc32=0;
    pfile_in_zip_read_info->compression_method =
            s->cur_file_info.compression_method;
   pfile_in_zip_read_info->filestream=s->filestream;
    pfile_in_zip_read_info->z_filefunc=s->z_filefunc;
    pfile_in_zip_read_info->byte_before_the_zipfile=s->byte_before_the_zipfile;

    pfile_in_zip_read_info->stream.total_out = 0;

    if ((s->cur_file_info.compression_method==Z_DEFLATED) &&
        (!raw))
    {
      pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
      pfile_in_zip_read_info->stream.zfree = (free_func)0;
      pfile_in_zip_read_info->stream.opaque = (voidpf)0;
      pfile_in_zip_read_info->stream.next_in = (voidpf)0;
      pfile_in_zip_read_info->stream.avail_in = 0;

      err=inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
      if (err == Z_OK)
	{
	  pfile_in_zip_read_info->stream_initialised=1;
	}
      else
	{
	  free (pfile_in_zip_read_info->read_buffer);
	  free (pfile_in_zip_read_info);
	  return err;
	}
        /* windowBits is passed < 0 to tell that there is no zlib header.
         * Note that in this case inflate *requires* an extra "dummy" byte
         * after the compressed stream in order to complete decompression and
         * return Z_STREAM_END.
         * In unzip, i don't wait absolutely Z_STREAM_END because I known the
         * size of both compressed and uncompressed data
         */
    }
    pfile_in_zip_read_info->rest_read_compressed =
            s->cur_file_info.compressed_size ;
    pfile_in_zip_read_info->rest_read_uncompressed =
            s->cur_file_info.uncompressed_size ;


    pfile_in_zip_read_info->pos_in_zipfile =
            s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER +
              iSizeVar;

    pfile_in_zip_read_info->stream.avail_in = (uInt)0;

    s->pfile_in_zip_read = pfile_in_zip_read_info;

    return EXTRACTOR_UNZIP_OK;
}

typedef struct Ecls {
  char * data;
  size_t size;
  size_t pos;
} Ecls;

voidpf EXTRACTOR_common_unzip_zlib_open_file_func (voidpf opaque,
			       const char* filename,
			       int mode) {
  if (0 == strcmp(filename,
		  "ERROR"))
    return opaque;
  else
    return NULL;
}

uLong EXTRACTOR_common_unzip_zlib_read_file_func(voidpf opaque,
			     voidpf stream,
			     void* buf,
			     uLong size) {
  Ecls * e = opaque;
  uLong ret;

  ret = e->size - e->pos;
  if (ret > size)
    ret = size;
  memcpy(buf,
	 &e->data[e->pos],
	 ret);
  e->pos += ret;
  return ret;
}

long EXTRACTOR_common_unzip_zlib_tell_file_func(voidpf opaque,
			    voidpf stream) {
  Ecls * e = opaque;
  return e->pos;
}

long EXTRACTOR_common_unzip_zlib_seek_file_func(voidpf opaque,
			    voidpf stream,
			    uLong offset,
			    int origin) {
  Ecls * e = opaque;

  switch (origin) {
  case ZLIB_FILEFUNC_SEEK_SET:
    if (offset > e->size) 
      return -1;
    e->pos = offset;
    break;
  case ZLIB_FILEFUNC_SEEK_END:
    if (offset > e->size) 
      return -1;
    e->pos = e->size - offset;
    break;
  case ZLIB_FILEFUNC_SEEK_CUR:
    if ( (offset < - e->pos) ||
	 (offset > e->size - e->pos) )
      return -1;
    e->pos += offset;
    break;
  default:
    return -1;
  }
  return 0;
}

int EXTRACTOR_common_unzip_zlib_close_file_func(voidpf opaque,
			    voidpf stream) {
  return 0;
}

int EXTRACTOR_common_unzip_zlib_testerror_file_func(voidpf opaque,
				voidpf stream) {
  return 0;
}
