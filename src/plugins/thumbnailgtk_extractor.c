/*
     This file is part of libextractor.
     (C) 2005, 2009 Vidyut Samanta and Christian Grothoff

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

/**
 * @file thumbnailextractor.c
 * @author Christian Grothoff
 * @brief this extractor produces a binary (!) encoded
 * thumbnail of images (using gdk pixbuf).  The bottom
 * of the file includes a decoder method that can be used
 * to reproduce the 128x128 PNG thumbnails.
 */

#include "platform.h"
#include "extractor.h"
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define THUMBSIZE 128

/* using libgobject, needs init! */
void __attribute__ ((constructor)) ole_gobject_init ()
{
  g_type_init ();
}


const char *
EXTRACTOR_thumbnailgtk_options ()
{
  /* 
     Since the Gnome developers think that being unable to
     unload plugins is an 'acceptable' limitation, we
     require out-of-process execution for plugins depending
     on libgsf and other glib-based plugins.
     See also https://bugzilla.gnome.org/show_bug.cgi?id=374940 
  */
  return "oop-only"; 
}


int 
EXTRACTOR_thumbnailgtk_extract (const char *data,
				size_t size,
				EXTRACTOR_MetaDataProcessor proc,
				void *proc_cls,
				const char *options)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *in;
  GdkPixbuf *out;
  size_t length;
  char *thumb;
  unsigned long width;
  unsigned long height;
  char format[64];
  int ret;

  loader = gdk_pixbuf_loader_new ();
  gdk_pixbuf_loader_write (loader, 
			   (const unsigned char*) data, 
			   size, NULL);
  in = gdk_pixbuf_loader_get_pixbuf (loader);
  gdk_pixbuf_loader_close (loader, NULL);
  if (in == NULL)
    {
      g_object_unref (loader);
      return 0;
    }
  g_object_ref (in);
  g_object_unref (loader);
  height = gdk_pixbuf_get_height (in);
  width = gdk_pixbuf_get_width (in);
  snprintf (format, 
	    sizeof(format),
	    "%ux%u",
	    (unsigned int) width, 
	    (unsigned int) height);
  if (0 != proc (proc_cls,
		 "thumbnailgtk",
		 EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 format,
		 strlen (format) + 1))
    {
      g_object_unref (in);
      return 1;
    }
  if ((height <= THUMBSIZE) && (width <= THUMBSIZE))
    {
      g_object_unref (in);
      return 0;
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
  if ( (height == 0) || (width == 0) )
    {
      g_object_unref (in);
      return 0;
    }
  out = gdk_pixbuf_scale_simple (in, width, height, GDK_INTERP_BILINEAR);
  g_object_unref (in);
  thumb = NULL;
  length = 0;
  if (out == NULL)
    return 0;
  if (!gdk_pixbuf_save_to_buffer (out, &thumb, &length, "png", NULL, 
				  "compression", "9", NULL))
    {
      g_object_unref (out);
      return 0;
    }
  g_object_unref (out);
  if (thumb == NULL)
    return 0;
  ret = proc (proc_cls,
	      "thumbnailgtk",
	      EXTRACTOR_METATYPE_THUMBNAIL,
	      EXTRACTOR_METAFORMAT_BINARY,
	      "image/png",
	      thumb, length);  
  free (thumb);
  return ret;
}

int 
EXTRACTOR_thumbnail_extract (const char *data,
			     size_t size,
			     EXTRACTOR_MetaDataProcessor proc,
			     void *proc_cls,
			     const char *options)
{
  return EXTRACTOR_thumbnailgtk_extract (data, size, proc, proc_cls, options);
}


/* end of thumbnailgtk_extractor.c */
