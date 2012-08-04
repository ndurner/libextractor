/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

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

#include "platform.h"
#include "extractor.h"
#include <magic.h>


/**
 * Global handle to MAGIC data.
 */
static magic_t magic;

 
/**
 * Main entry method for the 'application/ogg' extraction plugin.
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
  mime = magic_buffer (magic, buf, ret);
  if (NULL == mime)
    {
      magic_close (magic);
      return;
    }
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
  /* FIXME: hard-wiring this path might not be the 
     most sane thing to do; not sure what is a good
     portable way to find the 'magic' file though... */
  magic_load (magic, "/usr/share/misc/magic");
}


/**
 * Destructor for the library, cleans up.
 */
void __attribute__ ((destructor)) 
mime_ltdl_fini () 
{
  magic_close (magic);
  magic = NULL;
}

/* end of mime_extractor.c */
