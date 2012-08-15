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
 * @file plugins/tiff_extractor.c
 * @brief plugin to support TIFF files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <tiffio.h>


/**
 * Error handler for libtiff.  Does nothing.
 *
 * @param module where did the error arise?
 * @param fmt format string
 * @param ap arguments for fmt
 */
static void
error_cb (const char *module,
	  const char *fmt,
	  va_list ap)
{
  /* do nothing */
}


/**
 * Callback invoked by TIFF lib for reading.
 *
 * @param ctx the 'struct EXTRACTOR_ExtractContext'
 * @param data where to write data
 * @param size number of bytes to read
 * @return number of bytes read
 */
static tsize_t
read_cb (thandle_t ctx,
	 tdata_t data,
	 tsize_t size)
{
  struct EXTRACTOR_ExtractContext *ec = ctx;
  void *ptr;
  ssize_t ret;
  
  ret = ec->read (ec->cls, &ptr, size);
  if (ret > 0)
    memcpy (data, ptr, ret);
  return ret;
}


/**
 * Callback invoked by TIFF lib for writing.  Always fails.
 *
 * @param ctx the 'struct EXTRACTOR_ExtractContext'
 * @param data where to write data
 * @param size number of bytes to read
 * @return -1 (error)
 */
static tsize_t
write_cb (thandle_t ctx,
	  tdata_t data,
	  tsize_t size)
{
  return -1;
}


/**
 * Callback invoked by TIFF lib for seeking.
 *
 * @param ctx the 'struct EXTRACTOR_ExtractContext'
 * @param offset target offset
 * @param whence target is relative to where
 * @return new offset
 */
static toff_t
seek_cb (thandle_t ctx,
	 toff_t offset,
	 int whence)
{
  struct EXTRACTOR_ExtractContext *ec = ctx;

  return ec->seek (ec->cls, offset, whence);
}


/**
 * Callback invoked by TIFF lib for getting the file size.
 *
 * @param ctx the 'struct EXTRACTOR_ExtractContext'
 * @return file size
 */
static toff_t
size_cb (thandle_t ctx)
{
  struct EXTRACTOR_ExtractContext *ec = ctx;

  return ec->get_size (ec->cls);
}


/**
 * Callback invoked by TIFF lib for closing the file. Does nothing.
 *
 * @param ctx the 'struct EXTRACTOR_ExtractContext'
 */
static int
close_cb (thandle_t ctx)
{
  return 0; /* success */
}


/**
 * A mapping from TIFF Tag to extractor types.
 */
struct Matches
{
  /**
   * TIFF Tag.
   */
  ttag_t tag;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * Mapping of TIFF tags to LE types.
 * NULL-terminated.
 */
static struct Matches tmap[] = {
  { TIFFTAG_ARTIST, EXTRACTOR_METATYPE_ARTIST },
  { TIFFTAG_COPYRIGHT, EXTRACTOR_METATYPE_COPYRIGHT },
  { TIFFTAG_DATETIME, EXTRACTOR_METATYPE_CREATION_DATE },
  { TIFFTAG_DOCUMENTNAME, EXTRACTOR_METATYPE_TITLE },
  { TIFFTAG_HOSTCOMPUTER, EXTRACTOR_METATYPE_BUILDHOST },
  { TIFFTAG_IMAGEDESCRIPTION, EXTRACTOR_METATYPE_DESCRIPTION },
  { TIFFTAG_MAKE, EXTRACTOR_METATYPE_CAMERA_MAKE },
  { TIFFTAG_MODEL, EXTRACTOR_METATYPE_CAMERA_MODEL },
  { TIFFTAG_PAGENAME, EXTRACTOR_METATYPE_PAGE_RANGE },
  { TIFFTAG_SOFTWARE, EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  { TIFFTAG_TARGETPRINTER, EXTRACTOR_METATYPE_TARGET_ARCHITECTURE },
  { 0, 0 }
};


/**
 * Main entry method for the 'image/tiff' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void 
EXTRACTOR_tiff_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  TIFF *tiff;
  unsigned int i;
  char *meta;
  char format[128];
  uint32_t width;  
  uint32_t height;

  TIFFSetErrorHandler (&error_cb);
  TIFFSetWarningHandler (&error_cb);
  tiff = TIFFClientOpen ("<no filename>",
			 "rm", /* read-only, no mmap */
			 ec,
			 &read_cb,
			 &write_cb,
			 &seek_cb,
			 &close_cb,
			 &size_cb,
			 NULL, NULL);
  if (NULL == tiff)
    return;
  for (i = 0; 0 != tmap[i].tag; i++)
    if ( (1 ==
	  TIFFGetField (tiff, tmap[i].tag, &meta)) &&	
	 (0 != 
	  ec->proc (ec->cls, 
		    "tiff",
		    tmap[i].type,
		    EXTRACTOR_METAFORMAT_UTF8,
		    "text/plain",
		    meta,
		    strlen (meta) + 1)) )
      goto CLEANUP;      
  if ( (1 == 
	TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &width)) &&
       (1 == 
	TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &height)) )
    {
      snprintf (format,
		sizeof (format),
		"%ux%u",
		(unsigned int) width,
		(unsigned int) height);
      if (0 != 
	  ec->proc (ec->cls, 
		    "tiff",
		    EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		    EXTRACTOR_METAFORMAT_UTF8,
		    "text/plain",
		    format,
		    strlen (format) + 1)) 
	goto CLEANUP;
      if (0 != 
	  ec->proc (ec->cls, 
		    "tiff",
		    EXTRACTOR_METATYPE_MIMETYPE,
		    EXTRACTOR_METAFORMAT_UTF8,
		    "text/plain",
		    "image/tiff",
		    strlen ("image/tiff") + 1)) 
	goto CLEANUP;
    }
       
 CLEANUP:
  TIFFClose (tiff);
}

/* end of tiff_extractor.c */
