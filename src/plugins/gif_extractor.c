/*
     This file is part of libextractor.
     Copyright (C) 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */
/**
 * @file plugins/gif_extractor.c
 * @brief plugin to support GIF files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <gif_lib.h>


/**
 * Callback invoked by libgif to read data.
 *
 * @param ft the file handle, including our extract context
 * @param bt where to write the data
 * @param arg number of bytes to read
 * @return -1 on error, otherwise number of bytes read
 */
static int
gif_read_func (GifFileType *ft,
	       GifByteType *bt,
	       int arg)
{
  struct EXTRACTOR_ExtractContext *ec = ft->UserData;
  void *data;
  ssize_t ret;

  ret = ec->read (ec->cls,
		  &data,
		  arg);
  if (-1 == ret)
    return -1;
  memcpy (bt, data, ret);
  return ret;
}


/**
 * Main entry method for the 'image/gif' extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_gif_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  GifFileType *gif_file;
  GifRecordType gif_type;
  GifByteType *ext;
  int et;
  char dims[128];
#if defined (GIF_LIB_VERSION) || GIFLIB_MAJOR <= 4
  if (NULL == (gif_file = DGifOpen (ec, &gif_read_func)))
    return; /* not a GIF */
#else
  int gif_error;

  gif_error = 0;
  gif_file = DGifOpen (ec, &gif_read_func, &gif_error);
  if (gif_file == NULL || gif_error != 0)
  {
    if (gif_file != NULL)
#if GIFLIB_MAJOR < 5 || GIFLIB_MINOR < 1
      EGifCloseFile (gif_file);
#else
      EGifCloseFile (gif_file, NULL);
#endif
    return; /* not a GIF */
  }
#endif
  if (0 !=
      ec->proc (ec->cls,
		"gif",
		EXTRACTOR_METATYPE_MIMETYPE,
		EXTRACTOR_METAFORMAT_UTF8,
		"text/plain",
		"image/gif",
		strlen ("image/gif") + 1))
    return;
  snprintf (dims,
	    sizeof (dims),
	    "%dx%d",
	    gif_file->SHeight,
	    gif_file->SWidth);
  if (0 !=
      ec->proc (ec->cls,
		"gif",
		EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		EXTRACTOR_METAFORMAT_UTF8,
		"text/plain",
		dims,
		strlen (dims) + 1))
    return;
  while (1)
    {
      if (GIF_OK !=
	  DGifGetRecordType (gif_file,
			     &gif_type))
	break;
      if (UNDEFINED_RECORD_TYPE == gif_type)
	break;
      if (EXTENSION_RECORD_TYPE != gif_type)
	continue;
      if (GIF_OK !=
	  DGifGetExtension (gif_file, &et, &ext))
	continue;
      if (NULL == ext)
        continue;
      if (COMMENT_EXT_FUNC_CODE == et)
	{
	  ec->proc (ec->cls,
		    "gif",
		    EXTRACTOR_METATYPE_COMMENT,
		    EXTRACTOR_METAFORMAT_C_STRING,
		    "text/plain",
		    (char*) &ext[1],
		    (uint8_t) ext[0]);
	  break;
	}
      while ( (GIF_ERROR !=
	       DGifGetExtensionNext(gif_file, &ext)) &&
	      (NULL != ext) ) ; /* keep going */
    }
#if defined (GIF_LIB_VERSION) || GIFLIB_MAJOR < 5 || GIFLIB_MINOR < 1
  DGifCloseFile (gif_file);
#else
  DGifCloseFile (gif_file, NULL);
#endif
}

/* end of gif_extractor.c */
