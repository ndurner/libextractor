/*
     This file is part of libextractor.
     (C) 2005, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/thumbnailgtk_extractor.c
 * @author Christian Grothoff
 * @brief this extractor produces a binary (!) encoded
 * thumbnail of images (using gdk pixbuf).  The bottom
 * of the file includes a decoder method that can be used
 * to reproduce the 128x128 PNG thumbnails.  We use
 * libmagic to test if the input data is actually an
 * image before trying to give it to gtk.
 */
#include "platform.h"
#include "extractor.h"
#include <magic.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/**
 * Target size for the thumbnails (width and height).
 */
#define THUMBSIZE 128

/**
 * Maximum image size supported (to avoid unreasonable
 * allocations)
 */
#define MAX_IMAGE_SIZE (32 * 1024 * 1024)


/**
 * Global handle to MAGIC data.
 */
static magic_t magic;


/**
 * Main method for the gtk-thumbnailer plugin.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_thumbnailgtk_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *in;
  GdkPixbuf *out;
  size_t length;
  char *thumb;
  unsigned long width;
  unsigned long height;
  char format[64];
  void *data;
  uint64_t size;
  size_t off;
  ssize_t iret;
  void *buf;
  const char *mime;

  if (-1 == (iret = ec->read (ec->cls,
			      &data,
			      16 * 1024)))
    return;
  if (NULL == (mime = magic_buffer (magic, data, iret)))
    return;
  if (0 != strncmp (mime,
		    "image/",
		    strlen ("image/")))
    return; /* not an image */

  /* read entire image into memory */
  size = ec->get_size (ec->cls);
  if (UINT64_MAX == size)
    size = MAX_IMAGE_SIZE; /* unknown size, cap at max */
  if (size > MAX_IMAGE_SIZE)
    return; /* FAR too big to be an image */
  if (NULL == (buf = malloc (size)))
    return; /* too big to fit into memory on this system */

  /* start with data already read */
  memcpy (buf, data, iret);
  off = iret;
  while (off < size)
    {
      iret = ec->read (ec->cls, &data, size - off);
      if (iret <= 0)
	{
	  /* io error */
	  free (buf);
	  return;
	}
      memcpy (buf + off, data, iret);
      off += iret;
    }

  loader = gdk_pixbuf_loader_new ();
  gdk_pixbuf_loader_write (loader, 
			   buf, 
			   size, NULL);
  free (buf);
  in = gdk_pixbuf_loader_get_pixbuf (loader);
  gdk_pixbuf_loader_close (loader, NULL);
  if (NULL == in)
    {
      g_object_unref (loader);
      return;
    }
  g_object_ref (in);
  g_object_unref (loader);
  height = gdk_pixbuf_get_height (in);
  width = gdk_pixbuf_get_width (in);
  snprintf (format, 
	    sizeof (format),
	    "%ux%u",
	    (unsigned int) width, 
	    (unsigned int) height);
  if (0 != ec->proc (ec->cls,
		     "thumbnailgtk",
		     EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     format,
		     strlen (format) + 1))
    {
      g_object_unref (in);
      return;
    }
  if ((height <= THUMBSIZE) && (width <= THUMBSIZE))
    {
      g_object_unref (in);
      return;
    }
  if (height > THUMBSIZE)
    {
      width = width * THUMBSIZE / height;
      height = THUMBSIZE;
    }
  if (width > THUMBSIZE)
    {
      height = height * THUMBSIZE / width;
      width = THUMBSIZE;
    }
  if ( (0 == height) || (0 == width) )
    {
      g_object_unref (in);
      return;
    }
  out = gdk_pixbuf_scale_simple (in, width, height,
				 GDK_INTERP_BILINEAR);
  g_object_unref (in);
  thumb = NULL;
  length = 0;
  if (NULL == out)
    return;
  if (! gdk_pixbuf_save_to_buffer (out, &thumb, 
				   &length, 
				   "png", NULL, 
				   "compression", "9", 
				   NULL))
    {
      g_object_unref (out);
      return;
    }
  g_object_unref (out);
  if (NULL == thumb)
    return;
  ec->proc (ec->cls,
	    "thumbnailgtk",
	    EXTRACTOR_METATYPE_THUMBNAIL,
	    EXTRACTOR_METAFORMAT_BINARY,
	    "image/png",
	    thumb, length);  
  free (thumb);
}


/**
 * This plugin sometimes is installed under the alias 'thumbnail'.
 * So we need to provide a second entry method.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_thumbnail_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  EXTRACTOR_thumbnailgtk_extract_method (ec);
}


/**
 * Initialize glib and load magic file.
 */
void __attribute__ ((constructor)) 
thumbnailgtk_gobject_init ()
{
  g_type_init ();
  magic = magic_open (MAGIC_MIME_TYPE);
  if (0 != magic_load (magic, NULL))
    {
      /* FIXME: how to deal with errors? */
    }
}


/**
 * Destructor for the library, cleans up.
 */
void __attribute__ ((destructor)) 
thumbnailgtk_ltdl_fini () 
{
  if (NULL != magic)
    {
      magic_close (magic);
      magic = NULL;
    }
}

/* end of thumbnailgtk_extractor.c */
