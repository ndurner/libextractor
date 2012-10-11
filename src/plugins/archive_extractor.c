/*
     This file is part of libextractor.
     (C) 2012 Christian Grothoff

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
 * @file plugins/archive_extractor.c
 * @brief plugin to support archives (such as TAR)
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <archive.h>
#include <archive_entry.h>

/**
 * Callback for libarchive for 'reading'.
 *
 * @param a archive handle
 * @param client_data our 'struct EXTRACTOR_ExtractContext'
 * @param buff where to store data with pointer to data
 * @return number of bytes read
 */
static ssize_t
read_cb (struct archive *a, 
	 void *client_data, 
	 const void **buff)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;
  ssize_t ret;

  *buff = NULL;
  if (-1 == (ret = ec->read (ec->cls, (void **) buff, 16 * 1024)))
    return ARCHIVE_FATAL;
  return ret;
}


/**
 * Older versions of libarchive do not define __LA_INT64_T.
 */
#if ARCHIVE_VERSION_NUMBER < 2000000
#define __LA_INT64_T size_t
#else
#ifndef __LA_INT64_T
#define __LA_INT64_T int64_t
#endif
#endif


/**
 * Callback for libarchive for 'skipping'.
 *
 * @param a archive handle
 * @param client_data our 'struct EXTRACTOR_ExtractContext'
 * @param request number of bytes to skip
 * @return number of bytes skipped
 */
static __LA_INT64_T
skip_cb (struct archive *a, 
	 void *client_data,
	 __LA_INT64_T request)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;

  if (-1 == ec->seek (ec->cls, request, SEEK_CUR))
    return 0;
  return request;
}


/**
 * Main entry method for the ARCHIVE extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void 
EXTRACTOR_archive_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  struct archive *a;
  struct archive_entry *entry;
  const char *fname;
  const char *s;
  char *format;

  format = NULL;
  a = archive_read_new ();
  archive_read_support_compression_all (a);
  archive_read_support_format_all (a);
  if(archive_read_open2 (a, ec, NULL, &read_cb, &skip_cb, NULL)!= ARCHIVE_OK)
	return;
	
  while (ARCHIVE_OK == archive_read_next_header(a, &entry))
    {
      if ( (NULL == format) &&
	   (NULL != (fname = archive_format_name (a))) )
	format = strdup (fname);
      s = archive_entry_pathname (entry);
      if (0 != ec->proc (ec->cls, 
			 "tar", 
			 EXTRACTOR_METATYPE_FILENAME, 
			 EXTRACTOR_METAFORMAT_UTF8, 
			 "text/plain", 
			 s, strlen (s) + 1))
	break;
    }
  archive_read_finish (a);
  if (NULL != format)
    {
      if (0 != ec->proc (ec->cls, 
			 "tar",
			 EXTRACTOR_METATYPE_FORMAT,
			 EXTRACTOR_METAFORMAT_UTF8,
			 "text/plain", format, strlen (format) + 1))
	{
	  free (format);
	  return;
	} 
      free (format);
    }
}


/* end of tar_extractor.c */
