/*
 * This file is part of libextractor.
 * Copyright (C) 2012 Vidyut Samanta and Christian Grothoff
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3, or (at your
 * option) any later version.
 * 
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */ 
/**
 * @file plugins/zip_extractor.c
 * @brief plugin to support ZIP files
 * @author Christian Grothoff
 */
#include "platform.h"
#include <ctype.h>
#include "extractor.h"
#include "unzip.h"

  
/**
 * Main entry method for the 'application/zip' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_zip_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  struct EXTRACTOR_UnzipFile *uf;
  struct EXTRACTOR_UnzipFileInfo fi;
  char fname[256];
  char fcomment[256];

  if (NULL == (uf = EXTRACTOR_common_unzip_open (ec)))
    return;
  if ( (EXTRACTOR_UNZIP_OK ==
	EXTRACTOR_common_unzip_go_find_local_file (uf,
						   "meta.xml",
						   2)) ||
       (EXTRACTOR_UNZIP_OK ==
	EXTRACTOR_common_unzip_go_find_local_file (uf,
						   "META-INF/MANIFEST.MF",
						   2)) )
    {
      /* not a normal zip, might be odf, jar, etc. */
      goto CLEANUP;
    }
  if (EXTRACTOR_UNZIP_OK !=
      EXTRACTOR_common_unzip_go_to_first_file (uf))
    { 
      /* zip malformed? */
      goto CLEANUP;
   }
  if (0 !=
      ec->proc (ec->cls, 
		"zip",
		EXTRACTOR_METATYPE_MIMETYPE,
		EXTRACTOR_METAFORMAT_UTF8,
		"text/plain",
		"application/zip",
		strlen ("application/zip") + 1))
    goto CLEANUP;
  if (EXTRACTOR_UNZIP_OK ==
      EXTRACTOR_common_unzip_get_global_comment (uf,
						 fcomment,
						 sizeof (fcomment)))
    {
      if ( (0 != strlen (fcomment)) &&
	   (0 !=
	    ec->proc (ec->cls, 
		      "zip",
		      EXTRACTOR_METATYPE_COMMENT,
		      EXTRACTOR_METAFORMAT_C_STRING,
		      "text/plain",
		      fcomment,
		      strlen (fcomment) + 1)))
	goto CLEANUP;
    }
  do
    {
      if (EXTRACTOR_UNZIP_OK ==
	  EXTRACTOR_common_unzip_get_current_file_info (uf,
							&fi,
							fname,
							sizeof (fname),
							NULL, 0,
							fcomment,
							sizeof (fcomment)))
	{
	  if ( (0 != strlen (fname)) &&
	       (0 !=
		ec->proc (ec->cls, 
			  "zip",
			  EXTRACTOR_METATYPE_FILENAME,
			  EXTRACTOR_METAFORMAT_C_STRING,
			  "text/plain",
			  fname,
			  strlen (fname) + 1)))
	    goto CLEANUP;
	  if ( (0 != strlen (fcomment)) &&
	       (0 !=
		ec->proc (ec->cls, 
			  "zip",
			  EXTRACTOR_METATYPE_COMMENT,
			  EXTRACTOR_METAFORMAT_C_STRING,
			  "text/plain",
			  fcomment,
			  strlen (fcomment) + 1)))
	    goto CLEANUP;
	}						    
    }
  while (EXTRACTOR_UNZIP_OK ==
	 EXTRACTOR_common_unzip_go_to_next_file (uf));
  
CLEANUP:
  (void) EXTRACTOR_common_unzip_close (uf);
}

/* end of zip_extractor.c */
