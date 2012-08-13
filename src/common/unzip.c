/*
     This file is part of libextractor.
     (C) 2004, 2008, 2012 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
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
 * @file common/unzip.c
 * @brief API to access ZIP archives
 * @author Christian Grothoff
 * 
 * This code is based in part on
 * unzip 1.00 Copyright 1998-2003 Gilles Vollant
 * http://www.winimage.com/zLibDll"
 */
#include "platform.h"
#include <ctype.h>
#include "extractor.h"
#include "unzip.h"

#define CASESENSITIVITY (0)
#define MAXFILENAME (256)

#ifndef UNZ_BUFSIZE
#define UNZ_BUFSIZE (16384)
#endif

#ifndef UNZ_MAXFILENAMEINZIP
#define UNZ_MAXFILENAMEINZIP (256)
#endif

#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)


/**
 * IO callbacks for access to the ZIP data.
 */
struct EXTRACTOR_UnzipFileFuncDefs
{
  /**
   * Callback for reading 'size' bytes from the ZIP archive into buf.
   */
  uLong  ( *zread_file)  (voidpf opaque, void* buf, uLong size);

  /**
   * Callback to obtain the current read offset in the ZIP archive.
   */
  long   ( *ztell_file)  (voidpf opaque);

  /**
   * Callback for seeking to a different position in the ZIP archive.
   */
  long   ( *zseek_file)  (voidpf opaque, uLong offset, int origin);

  /**
   * Opaque argument to pass to all IO functions.
   */
  voidpf              opaque;
};


/**
 * Macro to read using filefunc API.
 *
 * @param filefunc filefunc struct
 * @param buf where to write data
 * @param size number of bytes to read
 * @return number of bytes copied to buf
 */
#define ZREAD(filefunc,buf,size) ((*((filefunc).zread_file)) ((filefunc).opaque, buf, size))

/**
 * Macro to obtain current offset in file using filefunc API.
 *
 * @param filefunc filefunc struct
 * @return current offset in file
 */
#define ZTELL(filefunc) ((*((filefunc).ztell_file)) ((filefunc).opaque))

/**
 * Macro to seek using filefunc API.
 *
 * @param filefunc filefunc struct
 * @param pos position to seek
 * @param mode seek mode
 * @return 0 on success
 */
#define ZSEEK(filefunc,pos,mode) ((*((filefunc).zseek_file)) ((filefunc).opaque, pos, mode))


/**
 * Global data about the ZIPfile
 * These data comes from the end of central dir 
 */
struct GlobalInfo
{

  /**
   * total number of entries in
   * the central dir on this disk 
   */
  uLong number_entry;        
  
  /**
   * size of the global comment of the zipfile 
   */
  uLong size_comment;         
};


/**
 * internal info about a file in zipfile
 */
struct UnzipFileInfoInternal
{

  /**
   * relative offset of local header 4 bytes 
   */
  uLong offset_curfile;

};


/**
 * Information about a file in zipfile, when reading and
 * decompressing it
 */
struct FileInZipReadInfo
{
  /**
   * internal buffer for compressed data 
   */
  char *read_buffer;         

  /**
   * zLib stream structure for inflate 
   */
  z_stream stream;            
  
  /**
   * position in byte on the zipfile, for fseek
   */
  uLong pos_in_zipfile;       

  /**
   * flag set if stream structure is initialised
   */
  uLong stream_initialised;   
  
  /**
   * offset of the local extra field 
   */
  uLong offset_local_extrafield;

  /**
   * size of the local extra field 
   */
  uInt  size_local_extrafield;

  /**
   * position in the local extra field in read
   */
  uLong pos_local_extrafield;   
  
  /**
   * crc32 of all data uncompressed so far
   */
  uLong crc32;                

  /**
   * crc32 we must obtain after decompress all 
   */
  uLong crc32_wait;           

  /**
   * number of bytes to be decompressed 
   */
  uLong rest_read_compressed; 
  
  /**
   * number of bytes to be obtained after decomp
   */
  uLong rest_read_uncompressed;
  
  /**
   * IO functions. (FIXME: where is this assigned?)
   */ 
  struct EXTRACTOR_UnzipFileFuncDefs z_filefunc;

  /**
   * compression method (0==store) 
   */
  uLong compression_method;   

  /**
   * byte before the zipfile, (>0 for sfx)
   */
  uLong byte_before_the_zipfile;
};


/**
 * Handle for a ZIP archive.
 * contains internal information about the zipfile
 */
struct EXTRACTOR_UnzipFile
{
  /**
   * io structore of the zipfile 
   */
  struct EXTRACTOR_UnzipFileFuncDefs z_filefunc;

  /**
   * public global information 
   */
  struct GlobalInfo gi;       
  
  /**
   * byte before the zipfile, (>0 for sfx)
   */
  uLong byte_before_the_zipfile;
  
  /**
   * number of the current file in the zipfile
   */
  uLong num_file;             

  /**
   * pos of the current file in the central dir
   */
  uLong pos_in_central_dir;   
  
  /**
   * flag about the usability of the current file
   */
  uLong current_file_ok;      

  /**
   * position of the beginning of the central dir
   */
  uLong central_pos;          
  
  /**
   * size of the central directory  
   */
  uLong size_central_dir;    

  /**
   * offset of start of central directory with respect to the starting
   * disk number
   */ 
  uLong offset_central_dir;  

  /**
   * public info about the current file in zip
   */
  struct EXTRACTOR_UnzipFileInfo cur_file_info;
  
  /**
   * private info about it
   */ 
  struct UnzipFileInfoInternal cur_file_info_internal; 

  /**
   * structure about the current file if we are decompressing it
   */
  struct FileInZipReadInfo* pfile_in_zip_read; 

  /**
   * Is the file encrypted?
   */
  int encrypted;
};


/**
 * Read a byte from a gz_stream; update next_in and avail_in. Return EOF
 * for end of file.
 * IN assertion: the stream s has been sucessfully opened for reading.
 */
static int 
unzlocal_getByte (const struct EXTRACTOR_UnzipFileFuncDefs* pzlib_filefunc_def,
		  int *pi)
{
  unsigned char c;

  if (1 != ZREAD (*pzlib_filefunc_def, &c, 1))
    return EXTRACTOR_UNZIP_EOF;
  *pi = (int) c;
  return EXTRACTOR_UNZIP_OK;
}


static int 
unzlocal_getShort (const struct EXTRACTOR_UnzipFileFuncDefs* pzlib_filefunc_def,
		   uLong *pX)
{
  uLong x;
  int i;
  int err;
  
  *pX = 0;
  if (EXTRACTOR_UNZIP_OK != (err = unzlocal_getByte (pzlib_filefunc_def, &i)))
    return err;
  x = (uLong) i;
  if (EXTRACTOR_UNZIP_OK != (err = unzlocal_getByte (pzlib_filefunc_def, &i)))
    return err;
  x += ((uLong) i) << 8;  
  *pX = x;
  return err;
}


static int 
unzlocal_getLong (const struct EXTRACTOR_UnzipFileFuncDefs* pzlib_filefunc_def,
		  uLong *pX)
{
  uLong x;
  int i;
  int err;
  
  *pX = 0;
  if (EXTRACTOR_UNZIP_OK != (err = unzlocal_getByte (pzlib_filefunc_def, &i)))
    return err;
  x = (uLong) i;
  if (EXTRACTOR_UNZIP_OK != (err = unzlocal_getByte (pzlib_filefunc_def, &i)))
    return err;
  x += ((uLong) i) << 8;
  if (EXTRACTOR_UNZIP_OK != (err = unzlocal_getByte (pzlib_filefunc_def, &i)))
    return err;
  x += ((uLong) i) << 16;
  if (EXTRACTOR_UNZIP_OK != (err = unzlocal_getByte (pzlib_filefunc_def, &i)))
    return err;
  x += ((uLong) i) << 24;
  *pX = x;
  return err;
}


#ifndef CASESENSITIVITYDEFAULT_NO
#if !defined(unix) && !defined(CASESENSITIVITYDEFAULT_YES)
#define CASESENSITIVITYDEFAULT_NO
#endif
#endif

#ifdef  CASESENSITIVITYDEFAULT_NO
#define CASESENSITIVITYDEFAULTVALUE 2
#else
#define CASESENSITIVITYDEFAULTVALUE 1
#endif


/**
 * Compare two filename (fileName1,fileName2).
 *
 * @param filename1 name of first file
 * @param filename2 name of second file
 * @param iCaseSensitivity, use 1 for case sensitivity (like strcmp);
 *        2 for no case sensitivity (like strcmpi or strcasecmp); or
 *        0 for defaut of your operating system (like 1 on Unix, 2 on Windows)
 * @return 0 if names are equal
 */
static int 
EXTRACTOR_common_unzip_string_file_name_compare (const char* fileName1,
						 const char* fileName2,
						 int iCaseSensitivity)
{
  if (0 == iCaseSensitivity)
    iCaseSensitivity = CASESENSITIVITYDEFAULTVALUE;
  if (1 == iCaseSensitivity)
    return strcmp(fileName1, fileName2);
  return strcasecmp (fileName1, fileName2);
}


#ifndef BUFREADCOMMENT
#define BUFREADCOMMENT (0x400)
#endif


/**
 *
 */
static uLong 
unzlocal_SearchCentralDir (const struct EXTRACTOR_UnzipFileFuncDefs* pzlib_filefunc_def)
{
  unsigned char *buf;
  uLong uSizeFile;
  uLong uBackRead;
  uLong uMaxBack = 0xffff; /* maximum size of global comment */
  uLong uPosFound = 0;
  
  if (0 != ZSEEK (*pzlib_filefunc_def, 0, SEEK_END))
    return 0;
  uSizeFile = ZTELL (*pzlib_filefunc_def);

  if (uMaxBack > uSizeFile)
    uMaxBack = uSizeFile;

  if (NULL == (buf = malloc(BUFREADCOMMENT + 4)))
    return 0;

  uBackRead = 4;
  while (uBackRead < uMaxBack)
    {
      uLong uReadSize;
      uLong uReadPos;
      int i;

      if (uBackRead + BUFREADCOMMENT > uMaxBack)
	uBackRead = uMaxBack;
      else
	uBackRead += BUFREADCOMMENT;
      uReadPos = uSizeFile - uBackRead;
      
      uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos)) 
	? (BUFREADCOMMENT + 4) 
	: (uSizeFile - uReadPos);
      if (0 != ZSEEK (*pzlib_filefunc_def, uReadPos, SEEK_SET))
	break;
      
      if (ZREAD (*pzlib_filefunc_def, buf, uReadSize) != uReadSize)
	break;
      
      i = (int) uReadSize - 3; 
      while (i-- > 0)
	if ( (0x50 == (*(buf+i))) &&
	     (0x4b == (*(buf+i+1))) &&
	     (0x05 == (*(buf+i+2))) && 
	     (0x06 == (*(buf+i+3))) )
	  {
	    uPosFound = uReadPos + i;
	    break;
	  }
      if (0 != uPosFound)
	break;
    }
  free (buf);
  return uPosFound;
}


/**
 * Translate date/time from Dos format to struct
 * EXTRACTOR_UnzipDateTimeInfo (readable more easilty)
 */
static void 
unzlocal_DosDateToTmuDate (uLong ulDosDate,
			   struct EXTRACTOR_UnzipDateTimeInfo* ptm)
{
  uLong uDate;

  uDate = (uLong) (ulDosDate >> 16);
  ptm->tm_mday = (uInt) (uDate & 0x1f);
  ptm->tm_mon =  (uInt) ((((uDate) & 0x1E0) / 0x20) - 1);
  ptm->tm_year = (uInt) (((uDate & 0x0FE00) / 0x0200) + 1980);
  ptm->tm_hour = (uInt) ((ulDosDate & 0xF800) / 0x800);
  ptm->tm_min =  (uInt) ((ulDosDate & 0x7E0) / 0x20);
  ptm->tm_sec =  (uInt) (2 * (ulDosDate & 0x1f));
}


static int 
unzlocal_GetCurrentFileInfoInternal (struct EXTRACTOR_UnzipFile *s,
				     struct EXTRACTOR_UnzipFileInfo *pfile_info,
				     struct UnzipFileInfoInternal *pfile_info_internal,
				     char *szFileName,
				     uLong fileNameBufferSize,
				     void *extraField,
				     uLong extraFieldBufferSize,
				     char *szComment,
				     uLong commentBufferSize)
{
  struct EXTRACTOR_UnzipFileInfo file_info;
  struct UnzipFileInfoInternal file_info_internal;
  int err = EXTRACTOR_UNZIP_OK;
  uLong uMagic;
  long lSeek = 0;
    
  if (s == NULL)
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (0 != ZSEEK (s->z_filefunc,
		  s->pos_in_central_dir + s->byte_before_the_zipfile,
		  SEEK_SET))
    err = EXTRACTOR_UNZIP_ERRNO;

  /* we check the magic */
  if (EXTRACTOR_UNZIP_OK == err) 
    {
      if (unzlocal_getLong(&s->z_filefunc, &uMagic) != EXTRACTOR_UNZIP_OK)
	err=EXTRACTOR_UNZIP_ERRNO;
      else if (uMagic!=0x02014b50)
	err=EXTRACTOR_UNZIP_BADZIPFILE;
    }
  if (unzlocal_getShort (&s->z_filefunc, &file_info.version) != EXTRACTOR_UNZIP_OK)
    err = EXTRACTOR_UNZIP_ERRNO;

  if (unzlocal_getShort(&s->z_filefunc, &file_info.version_needed) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getShort(&s->z_filefunc, &file_info.flag) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getShort(&s->z_filefunc, &file_info.compression_method) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getLong(&s->z_filefunc, &file_info.dosDate) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
    
  unzlocal_DosDateToTmuDate(file_info.dosDate,&file_info.tmu_date);
    
  if (unzlocal_getLong(&s->z_filefunc, &file_info.crc) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getLong(&s->z_filefunc, &file_info.compressed_size) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;

  if (unzlocal_getLong(&s->z_filefunc, &file_info.uncompressed_size) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getShort(&s->z_filefunc, &file_info.size_filename) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getShort(&s->z_filefunc, &file_info.size_file_extra) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getShort(&s->z_filefunc, &file_info.size_file_comment) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;

  if (unzlocal_getShort(&s->z_filefunc, &file_info.disk_num_start) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;

  if (unzlocal_getShort(&s->z_filefunc, &file_info.internal_fa) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (unzlocal_getLong(&s->z_filefunc, &file_info.external_fa) != EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;

  if (unzlocal_getLong (&s->z_filefunc,
			&file_info_internal.offset_curfile) != EXTRACTOR_UNZIP_OK)
    err = EXTRACTOR_UNZIP_ERRNO;

  lSeek += file_info.size_filename;
  if ((err==EXTRACTOR_UNZIP_OK) && (szFileName!=NULL))
    {
      uLong uSizeRead;
      if (file_info.size_filename < fileNameBufferSize)
        {
	  *(szFileName+file_info.size_filename) = '\0';
	  uSizeRead = file_info.size_filename;
        }
      else
	uSizeRead = fileNameBufferSize;
      
      if ((file_info.size_filename > 0) && (fileNameBufferSize > 0))
	if (ZREAD(s->z_filefunc, szFileName, uSizeRead) != uSizeRead)
	  err = EXTRACTOR_UNZIP_ERRNO;
      lSeek -= uSizeRead;
    }
  
  
  if ((err==EXTRACTOR_UNZIP_OK) && (extraField!=NULL))
    {
      uLong uSizeRead;
      if (file_info.size_file_extra<extraFieldBufferSize)
	uSizeRead = file_info.size_file_extra;
      else
	uSizeRead = extraFieldBufferSize;
      
      if (0 != lSeek) 
	{
	  if (0 == ZSEEK (s->z_filefunc, lSeek, SEEK_CUR))
	    lSeek = 0;
	  else
	    err = EXTRACTOR_UNZIP_ERRNO;
	}
      if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
	if (ZREAD(s->z_filefunc, extraField,uSizeRead)!=uSizeRead)
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

      if (lSeek!=0) 
	{
	  if (ZSEEK(s->z_filefunc, lSeek, SEEK_CUR)==0)
	    lSeek=0;
	  else
	    err=EXTRACTOR_UNZIP_ERRNO;
	}
      if ((file_info.size_file_comment>0) && (commentBufferSize>0))
	if (ZREAD(s->z_filefunc, szComment,uSizeRead)!=uSizeRead)
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


/**
 * Set the current file of the zipfile to the first file.
 *
 * @param file zipfile to manipulate
 * @return UNZ_OK if there is no problem
 */
int 
EXTRACTOR_common_unzip_go_to_first_file (struct EXTRACTOR_UnzipFile *file)
{
  int err;

  if (NULL == file)
    return EXTRACTOR_UNZIP_PARAMERROR;
  file->pos_in_central_dir = file->offset_central_dir;
  file->num_file = 0;
  err = unzlocal_GetCurrentFileInfoInternal (file,
					     &file->cur_file_info,
                                             &file->cur_file_info_internal,
                                             NULL, 0, NULL, 0, NULL, 0);
  file->current_file_ok = (err == EXTRACTOR_UNZIP_OK);
  return err;
}


/**
 * Open a Zip file.
 *
 * @param pzlib_filefunc_def IO functions
 * @return NULL on error
 */
static struct EXTRACTOR_UnzipFile * 
EXTRACTOR_common_unzip_open2 (struct EXTRACTOR_UnzipFileFuncDefs *pzlib_filefunc_def)
{
  struct EXTRACTOR_UnzipFile us;
  struct EXTRACTOR_UnzipFile *s;
  uLong central_pos;
  uLong uL;
  uLong number_disk;          /* number of the current dist, used for
				 spaning ZIP, unsupported, always 0*/
  uLong number_disk_with_CD;  /* number the the disk with central dir, used
				 for spaning ZIP, unsupported, always 0*/
  uLong number_entry_CD;      /* total number of entries in
				 the central dir
				 (same than number_entry on nospan) */
  
  int err = EXTRACTOR_UNZIP_OK;

  memset (&us, 0, sizeof(us));	
  us.z_filefunc = *pzlib_filefunc_def;
  
  central_pos = unzlocal_SearchCentralDir (&us.z_filefunc);
  if (central_pos==0)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if (ZSEEK(us.z_filefunc, 
	    central_pos, SEEK_SET)!=0)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* the signature, already checked */
  if (unzlocal_getLong(&us.z_filefunc, &uL)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* number of this disk */
  if (unzlocal_getShort(&us.z_filefunc, &number_disk)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* number of the disk with the start of the central directory */
  if (unzlocal_getShort(&us.z_filefunc, &number_disk_with_CD)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* total number of entries in the central dir on this disk */
  if (unzlocal_getShort(&us.z_filefunc, &us.gi.number_entry)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* total number of entries in the central dir */
  if (unzlocal_getShort(&us.z_filefunc, &number_entry_CD)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if ((number_entry_CD!=us.gi.number_entry) ||
      (number_disk_with_CD!=0) ||
      (number_disk!=0))
    err=EXTRACTOR_UNZIP_BADZIPFILE;
  
  /* size of the central directory */
  if (unzlocal_getLong(&us.z_filefunc, &us.size_central_dir)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* offset of start of central directory with respect to the
     starting disk number */
  if (unzlocal_getLong(&us.z_filefunc, &us.offset_central_dir)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  /* zipfile comment length */
  if (unzlocal_getShort(&us.z_filefunc, &us.gi.size_comment)!=EXTRACTOR_UNZIP_OK)
    err=EXTRACTOR_UNZIP_ERRNO;
  
  if ((central_pos<us.offset_central_dir+us.size_central_dir) &&
      (err==EXTRACTOR_UNZIP_OK))
    err=EXTRACTOR_UNZIP_BADZIPFILE;
  
  if (err!=EXTRACTOR_UNZIP_OK)
    {
      return NULL;
    }

  us.byte_before_the_zipfile = central_pos -
    (us.offset_central_dir+us.size_central_dir);
  us.central_pos = central_pos;
  us.pfile_in_zip_read = NULL;
  us.encrypted = 0;
  
  if (NULL == (s = malloc (sizeof(struct EXTRACTOR_UnzipFile))))
    return NULL;
  *s=us;
  EXTRACTOR_common_unzip_go_to_first_file(s);
  return s;
}


/**
 * Close the file in zip opened with EXTRACTOR_common_unzip_open_current_file.
 *
 * @return EXTRACTOR_UNZIP_CRCERROR if all the file was read but the CRC is not good
 */
int 
EXTRACTOR_common_unzip_close_current_file (struct EXTRACTOR_UnzipFile * file)
{
    int err=EXTRACTOR_UNZIP_OK;

    struct EXTRACTOR_UnzipFile* s;
    struct FileInZipReadInfo* pfile_in_zip_read_info;
    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(struct EXTRACTOR_UnzipFile*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

    if (pfile_in_zip_read_info==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;


    if (pfile_in_zip_read_info->rest_read_uncompressed == 0) 
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


/**
 * Close a ZipFile.  If there is files inside the .Zip opened with
 * EXTRACTOR_common_unzip_open_current_file, these files MUST be
 * closed with EXTRACTOR_common_unzip_close_current_file before
 * calling EXTRACTOR_common_unzip_close.
 *
 * @param file zip file to close
 * @return EXTRACTOR_UNZIP_OK if there is no problem. 
 */
int 
EXTRACTOR_common_unzip_close (struct EXTRACTOR_UnzipFile *file)
{
  struct EXTRACTOR_UnzipFile* s;
  
  if (file==NULL)
    return EXTRACTOR_UNZIP_PARAMERROR;
  s=(struct EXTRACTOR_UnzipFile*)file;
  
    if (s->pfile_in_zip_read!=NULL)
        EXTRACTOR_common_unzip_close_current_file(file);

    free(s);
    return EXTRACTOR_UNZIP_OK;
}


/**
 * Write info about the ZipFile in the *pglobal_info structure.
 * No preparation of the structure is needed.
 *
 * @param file zipfile to manipulate
 * @param szFileName where to write the name of the current file
 * @param fileNameBufferSize number of bytes available in szFileName
 * @param extraField where to write extra data
 * @param extraFieldBufferSize number of bytes available in extraField 
 * @param szComment where to write the comment on the current file
 * @param commentBufferSize number of bytes available in szComment
 * @return EXTRACTOR_UNZIP_OK if there is no problem.
 */
int
EXTRACTOR_common_unzip_get_current_file_info (struct EXTRACTOR_UnzipFile * file,
					      struct EXTRACTOR_UnzipFileInfo *pfile_info,
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


/**
 * Set the current file of the zipfile to the next file.
 *
 * @param file zipfile to manipulate
 * @return EXTRACTOR_UNZIP_OK if there is no problem,
 *         EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE if the actual file was the latest.
 */
int 
EXTRACTOR_common_unzip_go_to_next_file (struct EXTRACTOR_UnzipFile * file)
{
    struct EXTRACTOR_UnzipFile* s;
    int err;

    if (file==NULL)
        return EXTRACTOR_UNZIP_PARAMERROR;
    s=(struct EXTRACTOR_UnzipFile*)file;
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


/**
 * Try locate the file szFileName in the zipfile.
 * 
 * @param file zipfile to manipulate
 * @param szFileName name to find
 * @param iCaseSensitivity, use 1 for case sensitivity (like strcmp);
 *        2 for no case sensitivity (like strcmpi or strcasecmp); or
 *        0 for defaut of your operating system (like 1 on Unix, 2 on Windows)
 * @return EXTRACTOR_UNZIP_OK if the file is found. It becomes the current file.
 *         EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE if the file is not found
 */
int
EXTRACTOR_common_unzip_go_find_local_file (struct EXTRACTOR_UnzipFile *file, 
					   const char *szFileName,
					   int iCaseSensitivity)
{
  int err;
  /* We remember the 'current' position in the file so that we can jump
   * back there if we fail.
   */
  struct EXTRACTOR_UnzipFileInfo cur_file_infoSaved;
  struct UnzipFileInfoInternal cur_file_info_internalSaved;
  uLong num_fileSaved;
  uLong pos_in_central_dirSaved;
    
  if (NULL == file)
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (strlen (szFileName) >= UNZ_MAXFILENAMEINZIP)
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (! file->current_file_ok)
    return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE;
  
  /* Save the current state */
  num_fileSaved = file->num_file;
  pos_in_central_dirSaved = file->pos_in_central_dir;
  cur_file_infoSaved = file->cur_file_info;
  cur_file_info_internalSaved = file->cur_file_info_internal;
  err = EXTRACTOR_common_unzip_go_to_first_file (file);
  
  while (EXTRACTOR_UNZIP_OK == err)
    {
      char szCurrentFileName[UNZ_MAXFILENAMEINZIP+1];

      err = EXTRACTOR_common_unzip_get_current_file_info (file, NULL,
							  szCurrentFileName, sizeof (szCurrentFileName) - 1,
							  NULL, 0, NULL, 0);
      if (EXTRACTOR_UNZIP_OK == err)
        {
	  if (0 ==
	      EXTRACTOR_common_unzip_string_file_name_compare (szCurrentFileName,
							       szFileName,
							       iCaseSensitivity))
	    return EXTRACTOR_UNZIP_OK;
	  err = EXTRACTOR_common_unzip_go_to_next_file (file);
        }
    }
  
  /* We failed, so restore the state of the 'current file' to where we
   * were.
   */
  file->num_file = num_fileSaved;
  file->pos_in_central_dir = pos_in_central_dirSaved;
  file->cur_file_info = cur_file_infoSaved;
  file->cur_file_info_internal = cur_file_info_internalSaved;
  return err;
}


/**
 * Read bytes from the current file (must have been opened).
 *
 * @param buf contain buffer where data must be copied
 * @param len the size of buf.
 * @return the number of byte copied if somes bytes are copied
 *         0 if the end of file was reached
 *         <0 with error code if there is an error
 *        (EXTRACTOR_UNZIP_ERRNO for IO error, or zLib error for uncompress error)
 */
ssize_t 
EXTRACTOR_common_unzip_read_current_file (struct EXTRACTOR_UnzipFile *file,
					  void *buf,
					  size_t len)
{
  int err = EXTRACTOR_UNZIP_OK;
  uInt iRead = 0;
  struct FileInZipReadInfo* pfile_in_zip_read_info;

  if (NULL == file)
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (NULL == (pfile_in_zip_read_info = file->pfile_in_zip_read))
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (NULL == pfile_in_zip_read_info->read_buffer)
    return EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE;
  if (0 == len)
    return 0;

  pfile_in_zip_read_info->stream.next_out = (Bytef*) buf;
  pfile_in_zip_read_info->stream.avail_out = (uInt) len;
  if (len > pfile_in_zip_read_info->rest_read_uncompressed)
    pfile_in_zip_read_info->stream.avail_out =
      (uInt) pfile_in_zip_read_info->rest_read_uncompressed;
  
  while (pfile_in_zip_read_info->stream.avail_out > 0)
    {
      if ( (0 == pfile_in_zip_read_info->stream.avail_in) &&
	   (pfile_in_zip_read_info->rest_read_compressed>0) )
        {
	  uInt uReadThis = UNZ_BUFSIZE;
	  if (pfile_in_zip_read_info->rest_read_compressed<uReadThis)
	    uReadThis = (uInt) pfile_in_zip_read_info->rest_read_compressed;
	  if (0 == uReadThis)
	    return EXTRACTOR_UNZIP_EOF;
	  if (0  != 
	      ZSEEK (pfile_in_zip_read_info->z_filefunc,
		     pfile_in_zip_read_info->pos_in_zipfile +
		     pfile_in_zip_read_info->byte_before_the_zipfile,
		     SEEK_SET))
	    return EXTRACTOR_UNZIP_ERRNO;
	  if (ZREAD (pfile_in_zip_read_info->z_filefunc,
		     pfile_in_zip_read_info->read_buffer,
		     uReadThis) != uReadThis)
	    return EXTRACTOR_UNZIP_ERRNO;
	  
	  pfile_in_zip_read_info->pos_in_zipfile += uReadThis;	  
	  pfile_in_zip_read_info->rest_read_compressed-=uReadThis;
	  pfile_in_zip_read_info->stream.next_in =
	    (Bytef*)pfile_in_zip_read_info->read_buffer;
	  pfile_in_zip_read_info->stream.avail_in = (uInt)uReadThis;
        }
      
        if (pfile_in_zip_read_info->compression_method==0)
	  {
            uInt uDoCopy;
	    uInt i;
	    
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
            uLong uTotalOutBefore;
	    uLong uTotalOutAfter;
            const Bytef *bufBefore;
            uLong uOutThis;
            int flush = Z_SYNC_FLUSH;
	    
            uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
            bufBefore = pfile_in_zip_read_info->stream.next_out;
	    
            /*
	      if ((pfile_in_zip_read_info->rest_read_uncompressed ==
	      pfile_in_zip_read_info->stream.avail_out) &&
	      (pfile_in_zip_read_info->rest_read_compressed == 0))
	      flush = Z_FINISH;
            */
            err = inflate(&pfile_in_zip_read_info->stream,flush);
	    
            uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
            uOutThis = uTotalOutAfter-uTotalOutBefore;
	    
            pfile_in_zip_read_info->crc32 =
	      crc32(pfile_in_zip_read_info->crc32,bufBefore,
		    (uInt)(uOutThis));
	    
            pfile_in_zip_read_info->rest_read_uncompressed -=
	      uOutThis;
	    
            iRead += (uInt)(uTotalOutAfter - uTotalOutBefore);
	    
            if (Z_STREAM_END == err)
	      return (0 == iRead) ? EXTRACTOR_UNZIP_EOF : iRead;
            if (Z_OK != err)
	      break;
	  }
    }
  
  if (Z_OK == err)
    return iRead;
  return err;
}


/**
 * Read the local header of the current zipfile
 * Check the coherency of the local header and info in the end of central
 * directory about this file
 * store in *piSizeVar the size of extra info in local header
 * (filename and size of extra field data)
 */
static int 
unzlocal_CheckCurrentFileCoherencyHeader (struct EXTRACTOR_UnzipFile *file,
					  uInt *piSizeVar,
					  uLong *poffset_local_extrafield,
					  uInt *psize_local_extrafield)
{
  uLong uMagic;
  uLong uData;
  uLong uFlags;
  uLong size_filename;
  uLong size_extra_field;
  int err = EXTRACTOR_UNZIP_OK;

  *piSizeVar = 0;
  *poffset_local_extrafield = 0;
  *psize_local_extrafield = 0;
  
  if (0 != ZSEEK (file->z_filefunc, 
		  file->cur_file_info_internal.offset_curfile +
		  file->byte_before_the_zipfile, 
		  SEEK_SET))
    return EXTRACTOR_UNZIP_ERRNO;
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getLong (&file->z_filefunc,
			&uMagic))
    err = EXTRACTOR_UNZIP_ERRNO;
  else if (0x04034b50 != uMagic)
    err = EXTRACTOR_UNZIP_BADZIPFILE;   
  if ( (EXTRACTOR_UNZIP_OK !=
	unzlocal_getShort (&file->z_filefunc, &uData)) ||
       (EXTRACTOR_UNZIP_OK !=
	unzlocal_getShort (&file->z_filefunc, &uFlags)) )
    err = EXTRACTOR_UNZIP_ERRNO;
  
  if (EXTRACTOR_UNZIP_OK != unzlocal_getShort (&file->z_filefunc, &uData))
    err = EXTRACTOR_UNZIP_ERRNO;
  else if ((EXTRACTOR_UNZIP_OK == err) && 
	   (uData != file->cur_file_info.compression_method))
    err = EXTRACTOR_UNZIP_BADZIPFILE;

  if ( (EXTRACTOR_UNZIP_OK == err) &&
       (0 != file->cur_file_info.compression_method) &&
       (Z_DEFLATED != file->cur_file_info.compression_method) )
    err = EXTRACTOR_UNZIP_BADZIPFILE;
  
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getLong (&file->z_filefunc, &uData)) /* date/time */
    err = EXTRACTOR_UNZIP_ERRNO;

  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getLong (&file->z_filefunc, &uData)) /* crc */
    err = EXTRACTOR_UNZIP_ERRNO;
  else if ( (EXTRACTOR_UNZIP_OK == err) &&
	    (uData != file->cur_file_info.crc) &&
	    (0 == (uFlags & 8)) )
    err = EXTRACTOR_UNZIP_BADZIPFILE;
  
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getLong(&file->z_filefunc, &uData)) /* size compr */
    err = EXTRACTOR_UNZIP_ERRNO;
  else if ( (EXTRACTOR_UNZIP_OK == err) && 
	    (uData != file->cur_file_info.compressed_size) &&
	    (0 == (uFlags & 8)) )
    err = EXTRACTOR_UNZIP_BADZIPFILE;
  
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getLong (&file->z_filefunc,
			&uData)) /* size uncompr */
    err = EXTRACTOR_UNZIP_ERRNO;
  else if ( (EXTRACTOR_UNZIP_OK == err) && 
	    (uData != file->cur_file_info.uncompressed_size) &&
	    (0 == (uFlags & 8)))
    err = EXTRACTOR_UNZIP_BADZIPFILE;
 
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getShort (&file->z_filefunc, &size_filename))
    err = EXTRACTOR_UNZIP_ERRNO;
  else if ( (EXTRACTOR_UNZIP_OK == err) && 
	    (size_filename != file->cur_file_info.size_filename) )
    err = EXTRACTOR_UNZIP_BADZIPFILE;
  
  *piSizeVar += (uInt) size_filename;
  
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_getShort (&file->z_filefunc,
			 &size_extra_field))
    err = EXTRACTOR_UNZIP_ERRNO;
  *poffset_local_extrafield = file->cur_file_info_internal.offset_curfile +
    SIZEZIPLOCALHEADER + size_filename;
  *psize_local_extrafield = (uInt) size_extra_field;
  *piSizeVar += (uInt)size_extra_field;
  
  return err;
}


/**
 * Open for reading data the current file in the zipfile.
 *
 * @param file zipfile to manipulate
 * @return EXTRACTOR_UNZIP_OK on success
 */
int 
EXTRACTOR_common_unzip_open_current_file (struct EXTRACTOR_UnzipFile *file)
{
  int err = EXTRACTOR_UNZIP_OK;
  uInt iSizeVar;
  struct FileInZipReadInfo *pfile_in_zip_read_info;
  uLong offset_local_extrafield;  /* offset of the local extra field */
  uInt  size_local_extrafield;    /* size of the local extra field */
  
  if (NULL == file)
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (! file->current_file_ok)
    return EXTRACTOR_UNZIP_PARAMERROR;
  if (NULL != file->pfile_in_zip_read)
    EXTRACTOR_common_unzip_close_current_file (file);
  if (EXTRACTOR_UNZIP_OK !=
      unzlocal_CheckCurrentFileCoherencyHeader (file, 
						&iSizeVar,
						&offset_local_extrafield,
						&size_local_extrafield))
    return EXTRACTOR_UNZIP_BADZIPFILE;
  if (NULL == (pfile_in_zip_read_info = malloc(sizeof(struct FileInZipReadInfo))))
    return EXTRACTOR_UNZIP_INTERNALERROR;
  if (NULL == (pfile_in_zip_read_info->read_buffer = malloc(UNZ_BUFSIZE)))
    {
      free (pfile_in_zip_read_info);
      return EXTRACTOR_UNZIP_INTERNALERROR;
    }
  pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
  pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
  pfile_in_zip_read_info->pos_local_extrafield = 0;
  pfile_in_zip_read_info->stream_initialised = 0;

  if ( (0 != file->cur_file_info.compression_method) &&
       (Z_DEFLATED != file->cur_file_info.compression_method) )
    err = EXTRACTOR_UNZIP_BADZIPFILE;
  
  pfile_in_zip_read_info->crc32_wait = file->cur_file_info.crc;
  pfile_in_zip_read_info->crc32 = 0;
  pfile_in_zip_read_info->compression_method = file->cur_file_info.compression_method;
  pfile_in_zip_read_info->z_filefunc = file->z_filefunc;
  pfile_in_zip_read_info->byte_before_the_zipfile = file->byte_before_the_zipfile;
  pfile_in_zip_read_info->stream.total_out = 0;

  if (file->cur_file_info.compression_method==Z_DEFLATED)
    {
      pfile_in_zip_read_info->stream.zalloc = (alloc_func) NULL;
      pfile_in_zip_read_info->stream.zfree = (free_func) NULL;
      pfile_in_zip_read_info->stream.opaque = NULL;
      pfile_in_zip_read_info->stream.next_in = NULL;
      pfile_in_zip_read_info->stream.avail_in = 0;
      if (Z_OK != (err = inflateInit2 (&pfile_in_zip_read_info->stream, -MAX_WBITS)))
	{
	  free (pfile_in_zip_read_info->read_buffer);
	  free (pfile_in_zip_read_info);
	  return err;
	}
      pfile_in_zip_read_info->stream_initialised = 1;
      /* windowBits is passed < 0 to tell that there is no zlib header.
       * Note that in this case inflate *requires* an extra "dummy" byte
       * after the compressed stream in order to complete decompression and
       * return Z_STREAM_END.
       * In unzip, i don't wait absolutely Z_STREAM_END because I known the
       * size of both compressed and uncompressed data
       */
    }
  pfile_in_zip_read_info->rest_read_compressed = file->cur_file_info.compressed_size;
  pfile_in_zip_read_info->rest_read_uncompressed = file->cur_file_info.uncompressed_size;
  pfile_in_zip_read_info->pos_in_zipfile =
    file->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER +
    iSizeVar;
  pfile_in_zip_read_info->stream.avail_in = 0;
  file->pfile_in_zip_read = pfile_in_zip_read_info;
  
  return EXTRACTOR_UNZIP_OK;
}


static uLong 
ec_read_file_func (voidpf opaque,
		   void* buf,
		   uLong size) 
{
  struct EXTRACTOR_ExtractContext *ec = opaque;
  void *ptr;
  ssize_t ret;
  
  ret = ec->read (ec->cls,
		  &ptr,
		  size);
  if (ret > 0)
    memcpy (buf, ptr, ret);
  // FIXME: partial reads are not allowed, need to possibly read more
  return ret;
}


static long 
ec_tell_file_func (voidpf opaque)
{
  struct EXTRACTOR_ExtractContext *ec = opaque;

  return ec->seek (ec->cls, 0, SEEK_CUR);
}


static long 
ec_seek_file_func (voidpf opaque,					   
		   uLong offset,
		   int origin) 
{
  struct EXTRACTOR_ExtractContext *ec = opaque;
  
  if (-1 == ec->seek (ec->cls, offset, origin))
    return EXTRACTOR_UNZIP_INTERNALERROR;
  return EXTRACTOR_UNZIP_OK;
}


/**
 * Open a zip file for processing using the data access
 * functions from the extract context.
 *
 * @param ec extract context to use
 * @return handle to zip data, NULL on error
 */
struct EXTRACTOR_UnzipFile * 
EXTRACTOR_common_unzip_open (struct EXTRACTOR_ExtractContext *ec)
{
  struct EXTRACTOR_UnzipFileFuncDefs io;

  io.zread_file = &ec_read_file_func;
  io.ztell_file = &ec_tell_file_func;
  io.zseek_file = &ec_seek_file_func;
  io.opaque = ec;

  return EXTRACTOR_common_unzip_open2 (&io);
}

/* end of unzip.c */
