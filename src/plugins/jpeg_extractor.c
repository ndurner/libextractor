/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/jpeg_extractor.c
 * @brief plugin to support JPEG files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#if WINDOWS
#define HAVE_BOOLEAN
#endif
#include <jpeglib.h>
#include <setjmp.h>


/**
 * Context for custom functions.
 */
struct Context
{
  /**
   * Environment for longjmp from within error_exit handler.
   */
  jmp_buf env;
};


/**
 * Function used to avoid having libjpeg write error messages to the console.
 */
static void
no_emit (j_common_ptr cinfo, int msg_level)
{
  /* do nothing */
}


/**
 * Function used to avoid having libjpeg write error messages to the console.
 */
static void
no_output (j_common_ptr cinfo)
{
  /* do nothing */
}


/**
 * Function used to avoid having libjpeg kill our process.
 */
static void
no_exit (j_common_ptr cinfo)
{
  struct Context *ctx = cinfo->client_data;

  /* we're not allowed to return (by API definition),
     and we don't want to abort/exit.  So we longjmp
     to our cleanup code instead. */
  longjmp (ctx->env, 1);
}


/**
 * Main entry method for the 'image/jpeg' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void 
EXTRACTOR_jpeg_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  struct jpeg_decompress_struct jds;
  struct jpeg_error_mgr em;
  void *buf;
  ssize_t size;
  int is_jpeg;
  unsigned int rounds;
  char format[128];
  struct jpeg_marker_struct *mptr;
  struct Context ctx;

  is_jpeg = 0;
  rounds = 0; /* used to avoid going on forever for non-jpeg files */
  jpeg_std_error (&em);
  em.emit_message = &no_emit;
  em.output_message = &no_output;
  em.error_exit = &no_exit;
  jds.client_data = &ctx;
  if (1 == setjmp (ctx.env)) 
    goto EXIT; /* we get here if libjpeg calls 'no_exit' because it wants to die */
  jds.err = &em;
  jpeg_create_decompress (&jds);
  jpeg_save_markers (&jds, JPEG_COM, 1024 * 8);
  while ( (1 == is_jpeg) || (rounds++ < 8) )
    {
      if (-1 == (size = ec->read (ec->cls,
				  &buf,
				  16 * 1024)))
	break;
      if (0 == size)
	break;
      jpeg_mem_src (&jds, buf, size);
      if (0 == is_jpeg)
	{      
	  if (JPEG_HEADER_OK == jpeg_read_header (&jds, 1))
	    is_jpeg = 1; /* ok, really a jpeg, keep going until the end */
	  continue;
	}
      jpeg_consume_input (&jds);
    }

  if (1 != is_jpeg)
    goto EXIT;
  if (0 !=
      ec->proc (ec->cls,
		"jpeg",
		EXTRACTOR_METATYPE_MIMETYPE,
		EXTRACTOR_METAFORMAT_UTF8,
		"text/plain",
		"image/jpeg",
		strlen ("image/jpeg") + 1))
    goto EXIT;
  snprintf (format,
	    sizeof (format),
	    "%ux%u",
	    (unsigned int) jds.image_width,
	    (unsigned int) jds.image_height);
  if (0 !=
      ec->proc (ec->cls,
		"jpeg",
		EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		EXTRACTOR_METAFORMAT_UTF8,
		"text/plain",
		format,
		strlen (format) + 1))
    goto EXIT;
  for (mptr = jds.marker_list; NULL != mptr; mptr = mptr->next)
    {
      size_t off;

      if (JPEG_COM != mptr->marker)
	continue;
      off = 0;
      while ( (off < mptr->data_length) &&
	      (isspace ((int) ((const char *)mptr->data)[mptr->data_length - 1 - off])) )
	off++;
      if (0 !=
	  ec->proc (ec->cls,
		    "jpeg",
		    EXTRACTOR_METATYPE_COMMENT,
		    EXTRACTOR_METAFORMAT_C_STRING,
		    "text/plain",
		    (const char *) mptr->data,
		    mptr->data_length - off))
	goto EXIT;
    }
  
 EXIT:
  jpeg_destroy_decompress (&jds);
}

/* end of jpeg_extractor.c */
