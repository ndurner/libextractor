/*
     This file is part of libextractor.
     (C) 2008, 2012 Christian Grothoff (and other contributing authors)

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
 * @file common/unzip.h
 * @brief API to access ZIP archives
 * @author Christian Grothoff
 * 
 * This code is based in part on
 * unzip 1.00 Copyright 1998-2003 Gilles Vollant
 * http://www.winimage.com/zLibDll"
 */
#ifndef LE_COMMON_UNZIP_H
#define LE_COMMON_UNZIP_H

#include <zlib.h>

/**
 * Operation was successful.
 */
#define EXTRACTOR_UNZIP_OK                          (0)

/**
 * Cannot move to next file, we are at the end
 */
#define EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE         (-100)

/**
 * IO error, see errno.
 */
#define EXTRACTOR_UNZIP_ERRNO                       (Z_ERRNO)

/**
 * Reached end of the file (NOTE: same as OK!)
 */
#define EXTRACTOR_UNZIP_EOF                         (0)

/**
 * Invalid arguments to call.
 */
#define EXTRACTOR_UNZIP_PARAMERROR                  (-102)

/**
 * Not a zip file (or malformed)
 */
#define EXTRACTOR_UNZIP_BADZIPFILE                  (-103)

/**
 * Internal error.
 */
#define EXTRACTOR_UNZIP_INTERNALERROR               (-104)

/**
 * Checksum failure.
 */
#define EXTRACTOR_UNZIP_CRCERROR                    (-105)

/**
 * Handle for a ZIP archive.
 */
struct EXTRACTOR_UnzipFile;


/**
 * date/time information
 */
struct EXTRACTOR_UnzipDateTimeInfo
{
  /**
   * seconds after the minute - [0,59] 
   */
  uInt tm_sec;            

  /**
   * minutes after the hour - [0,59] 
   */
  uInt tm_min;            
  
  /**
   * hours since midnight - [0,23] 
   */
  uInt tm_hour;           

  /**
   * day of the month - [1,31] 
   */
  uInt tm_mday;           
 
  /**
   * months since January - [0,11] 
   */
  uInt tm_mon;            

  /**
   * years - [1980..2044] 
   */
  uInt tm_year;           
};


/**
 * Information about a file in the zipfile 
 */
struct EXTRACTOR_UnzipFileInfo
{
  /**
   * version made by                 2 bytes 
   */
  uLong version;              

  /**
   * version needed to extract       2 bytes 
   */
  uLong version_needed;       

  /**
   * general purpose bit flag        2 bytes 
   */
  uLong flag;                 

  /**
   * compression method              2 bytes 
   */
  uLong compression_method;   

  /**
   * last mod file date in Dos fmt   4 bytes 
   */
  uLong dosDate;              

  /**
   * crc-32                          4 bytes 
   */
  uLong crc;                  

  /**
   * compressed size                 4 bytes
   */
  uLong compressed_size;      

  /**
   * uncompressed size               4 bytes 
   */
  uLong uncompressed_size;    

  /**
   * filename length                 2 bytes 
   */
  uLong size_filename;        

  /**
   * extra field length              2 bytes 
   */
  uLong size_file_extra;      

  /**
   * file comment length             2 bytes 
   */
  uLong size_file_comment;    
  
  /**
   * disk number start               2 bytes 
   */
  uLong disk_num_start;       

  /**
   * internal file attributes        2 bytes 
   */
  uLong internal_fa;          

  /**
   * external file attributes        4 bytes 
   */
  uLong external_fa;          
  
  /**
   * Time and date of last modification.
   */
  struct EXTRACTOR_UnzipDateTimeInfo tmu_date;
};


/**
 * Open a zip file for processing using the data access
 * functions from the extract context.
 *
 * @param ec extract context to use
 * @return handle to zip data, NULL on error
 */
struct EXTRACTOR_UnzipFile * 
EXTRACTOR_common_unzip_open (struct EXTRACTOR_ExtractContext *ec);


/**
 * Obtain the global comment from a ZIP file.
 *
 * @param file unzip file to inspect
 * @param comment where to copy the comment
 * @param comment_len maximum number of bytes available in comment
 * @return EXTRACTOR_UNZIP_OK on success
 */
int
EXTRACTOR_common_unzip_get_global_comment (struct EXTRACTOR_UnzipFile *file,
					   char *comment,
					   size_t comment_len);


/**
 * Close a ZipFile.
 *
 * @param file zip file to close
 * @return EXTRACTOR_UNZIP_OK if there is no problem. 
 */
int
EXTRACTOR_common_unzip_close (struct EXTRACTOR_UnzipFile *file);


/**
 * Set the current file of the zipfile to the first file.
 *
 * @param file zipfile to manipulate
 * @return UNZ_OK if there is no problem
 */
int 
EXTRACTOR_common_unzip_go_to_first_file (struct EXTRACTOR_UnzipFile *file);


/**
 * Set the current file of the zipfile to the next file.
 *
 * @param file zipfile to manipulate
 * @return EXTRACTOR_UNZIP_OK if there is no problem,
 *         EXTRACTOR_UNZIP_END_OF_LIST_OF_FILE if the actual file was the latest.
 */
int
EXTRACTOR_common_unzip_go_to_next_file (struct EXTRACTOR_UnzipFile *file);


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
					   int iCaseSensitivity);


/**
 * Write info about the ZipFile in the *pglobal_info structure.
 * No preparation of the structure is needed.
 *
 * @param file zipfile to manipulate
 * @param pfile_info file information to initialize
 * @param szFileName where to write the name of the current file
 * @param fileNameBufferSize number of bytes available in szFileName
 * @param extraField where to write extra data
 * @param extraFieldBufferSize number of bytes available in extraField 
 * @param szComment where to write the comment on the current file
 * @param commentBufferSize number of bytes available in szComment
 * @return EXTRACTOR_UNZIP_OK if there is no problem.
 */
int 
EXTRACTOR_common_unzip_get_current_file_info (struct EXTRACTOR_UnzipFile *file,
					      struct EXTRACTOR_UnzipFileInfo *pfile_info, 
					      char *szFileName,
					      uLong fileNameBufferSize,
					      void *extraField, 
					      uLong extraFieldBufferSize, 
					      char *szComment,
					      uLong commentBufferSize);


/**
 * Open for reading data the current file in the zipfile.
 *
 * @param file zipfile to manipulate
 * @return EXTRACTOR_UNZIP_OK on success
 */
int
EXTRACTOR_common_unzip_open_current_file (struct EXTRACTOR_UnzipFile *file);


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
					  size_t len);


/**
 * Close the file in zip opened with EXTRACTOR_common_unzip_open_current_file.
 *
 * @return EXTRACTOR_UNZIP_CRCERROR if all the file was read but the CRC is not good
 */
int 
EXTRACTOR_common_unzip_close_current_file (struct EXTRACTOR_UnzipFile *file);


#endif 
/* LE_COMMON_UNZIP_H */
