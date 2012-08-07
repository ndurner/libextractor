/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/mime_extractor.c
 * @brief plugin to determine mime types using libmagic (from 'file')
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <magic.h>


/**
 * Global handle to MAGIC data.
 */
static magic_t magic;

/**
 * Path we used for loading magic data, NULL is used for 'default'.
 */
static char *magic_path;
 

/**
 * Main entry method for the 'application/ogg' extraction plugin.  The
 * 'config' of the context can be used to specify an alternative magic
 * path.  If config is not given, the default magic path will be
 * used.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_mime_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *buf;
  ssize_t ret;
  const char *mime;

  ret = ec->read (ec->cls,
		  &buf,
		  16 * 1024);
  if (-1 == ret)
    return;
  if ( ( (NULL == magic_path) &&
	 (NULL != ec->config) ) ||
       ( (NULL != magic_path) &&
	 (NULL == ec->config) ) ||
       ( (NULL != magic_path) &&
	 (NULL != ec->config) &&
	 (0 != strcmp (magic_path,
		       ec->config) )) )
    {
      if (NULL != magic_path)
	free (magic_path);		
      magic_close (magic);
      magic = magic_open (MAGIC_MIME_TYPE);
      if (0 != magic_load (magic, ec->config))
	{
	  /* FIXME: report errors? */
	}
      if (NULL != ec->config)
	magic_path = strdup (ec->config);
      else
	magic_path = NULL;
    }
  if (NULL == (mime = magic_buffer (magic, buf, ret)))
    return;
  ec->proc (ec->cls,
	    "mime",
	    EXTRACTOR_METATYPE_MIMETYPE,
	    EXTRACTOR_METAFORMAT_UTF8,
	    "text/plain",
	    mime,
	    strlen (mime) + 1);
}


/**
 * Constructor for the library.  Loads the magic file.
 */
void __attribute__ ((constructor)) 
mime_ltdl_init () 
{
  magic = magic_open (MAGIC_MIME_TYPE);
  if (0 != magic_load (magic, magic_path))
    {
      /* FIXME: how to deal with errors? */
    }
}


/**
 * Destructor for the library, cleans up.
 */
void __attribute__ ((destructor)) 
mime_ltdl_fini () 
{
  if (NULL != magic)
    {
      magic_close (magic);
      magic = NULL;
    }
  if (NULL != magic_path)
    {
      free (magic_path);
      magic_path = NULL;
    }
}

/* end of mime_extractor.c */
