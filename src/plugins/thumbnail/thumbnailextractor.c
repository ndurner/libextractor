/*
     This file is part of libextractor.
     (C) 2005 Vidyut Samanta and Christian Grothoff

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
#include <gdk-pixbuf/gdk-pixbuf.h>

#define THUMBSIZE 128


/* using libgobject, needs init! */
void __attribute__ ((constructor)) ole_gobject_init(void) {
  g_type_init();
}


static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
					  char * keyword,
					  EXTRACTOR_KeywordList * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}


/* which mime-types maybe subjected to
   the thumbnail extractor (ImageMagick
   crashes and/or prints errors for bad
   formats, so we need to be rather
   conservative here) */
static char * whitelist[] = {
  "image/jpeg",
  "image/gif",
  "image/miff",
  "image/mng",
  "image/png",
  "image/tiff",
  "image/x-bmp",
  "image/x-mng",
  "image/x-png",
  "image/x-xpm",
  "image/xcf",
  NULL,
};

struct EXTRACTOR_Keywords * libextractor_thumbnail_extract(const char * filename,
							   const char * data,
							   size_t size,
							   struct EXTRACTOR_Keywords * prev) {
  GdkPixbuf * in;
  GdkPixbuf * out;
  size_t length;
  char * thumb;
  unsigned long width;
  unsigned long height;
  char * binary;
  size_t pos;
  size_t end;
  size_t wpos;
  size_t i;
  unsigned int markers[8]; /* 256 bits */
  unsigned char marker;
  const char * mime;
  int j;
  char * format;
  GError *err = NULL;

  /* if the mime-type of the file is not whitelisted
     do not run the thumbnail extactor! */
  mime = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE,
			       prev);
  if (mime == NULL)
    return prev;
  j = 0;
  while (whitelist[j] != NULL) {
    if (0 == strcmp(whitelist[j], mime))
      break;
    j++;
  }
  if (whitelist[j] == NULL)
    return prev;

  in = gdk_pixbuf_new_from_file(filename,
				&err);
  if (in == NULL)
    return prev;
  height = gdk_pixbuf_get_height(in);
  width = gdk_pixbuf_get_width(in);
  format = malloc(64);
  snprintf(format,
	   64,
	   "%ux%u",
	   (unsigned int) width,
	   (unsigned int) height);
  prev
    = addKeyword(EXTRACTOR_SIZE,
		 format,
		 prev);
  if (height == 0)
    height = 1;
  if (width == 0)
    width = 1;
  if ( (height <= THUMBSIZE) &&
       (width <= THUMBSIZE) ) {
    g_object_unref(in);
    return prev;
  }
  if (height > THUMBSIZE) {
    width = width * THUMBSIZE / height;
    height = THUMBSIZE;
  }
  if (width > THUMBSIZE) {
    height = height * THUMBSIZE / width;
    width = THUMBSIZE;
  }
  out = gdk_pixbuf_scale_simple(in,
				width,
				height,
				GDK_INTERP_BILINEAR);
  g_object_unref(in);
  thumb = NULL;
  if (! gdk_pixbuf_save_to_buffer(out,
				  &thumb,
				  &length,
				  "png",
				  NULL,
				  NULL)) {
    g_object_unref(out);
    return prev;
  }
  g_object_unref(out);
  if (thumb == NULL)
    return prev;


  /* encode! */
  binary = malloc(2 + length + (length+256) / 254);
  if (binary == NULL)
    return prev;

  pos = 0;
  wpos = 0;
  while (pos < length) {
    /* find unused value between 1 and 255 in
       the next 254 bytes */
    end = pos + 254;
    if (end < pos)
      break; /* integer overflow! */
    if (end > length)
      end = length;
    memset(markers, 0, sizeof(markers));
    for (i=pos;i<end;i++)
      markers[thumb[i]&7] |= 1 << (thumb[i] >> 3);
    marker = 1;
    while (markers[marker&7] & (1 << (marker >> 3))) {
      marker++;
      if (marker == 0) {
	/* assertion failed... */
	free(binary);
	free(thumb);
	return prev;
      }
    }
    /* recode */
    binary[wpos++] = marker;
    for (i=pos;i<end;i++)
      binary[wpos++] = thumb[i] == 0 ? marker : thumb[i];
    pos = end;
  }
  binary[wpos++] = 0; /* 0-termination! */
  free(thumb);
  return addKeyword(EXTRACTOR_THUMBNAIL_DATA,
		    binary,
		    prev);
}

#if 0

/**
 * This function can be used to decode the binary data
 * stream produced by the thumbnailextractor.
 *
 * @param in 0-terminated string from the meta-data
 * @return 1 on error, 0 on success
 */
int decodeThumbnail(const unsigned char * in,
		    unsigned char ** out,
		    size_t * outSize) {
  unsigned char * buf;
  size_t pos;
  size_t wpos;
  unsigned char marker;
  size_t i;
  size_t end;
  size_t inSize;

  inSize = strlen(in);
  if (inSize == 0) {
    *out = NULL;
    *outSize = 0;
    return 1;
  }

  buf = malloc(inSize); /* slightly more than needed ;-) */
  *out = buf;

  pos = 0;
  wpos = 0;
  while (pos < inSize) {
    end = pos + 255; /* 255 here: count the marker! */
    if (end > inSize)
      end = inSize;
    marker = in[pos++];
    for (i=pos;i<end;i++)
      buf[wpos++] = (in[i] == marker) ? 0 : in[i];
    pos = end;
  }
  *outSize = wpos;
  return 0;
}


#endif
