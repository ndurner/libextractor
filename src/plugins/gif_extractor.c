/*
     This file is part of libextractor.
     (C) 2002, 2003, 2009 Vidyut Samanta and Christian Grothoff

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
#include "pack.h"

#define DEBUG_GIF 0
#if DEBUG_GIF
#define PRINT(a,b) fprintf(stderr,a,b)
#else
#define PRINT(a,b)
#endif

struct GifHeader
{
  char gif[3];
  char version[3];
  uint16_t screen_width;
  uint16_t screen_height;
  unsigned char flags;
#define HEADER_FLAGS__SIZE_OF_GLOBAL_COLOR_TABLE 0x07
#define HEADER_FLAGS__SORT_FLAG 0x08
#define HEADER_FLAGS__COLOR_RESOLUTION 0x70
#define HEADER_FLAGS__GLOBAL_COLOR_TABLE_FLAG 0x80
  unsigned char background_color_index;
  unsigned char pixel_aspect_ratio;
};

#define GIF_HEADER_SIZE 13
#define GIF_HEADER_SPEC "3b3bhhbbb"
#define GIF_HEADER_FIELDS(p) \
 &(p)->gif,\
 &(p)->version, \
 &(p)->screen_width, \
 &(p)->screen_height, \
 &(p)->flags, \
 &(p)->background_color_index, \
 &(p)->pixel_aspect_ratio

struct GifDescriptor
{
  unsigned char image_separator;
  uint16_t image_left;
  uint16_t image_top;
  uint16_t image_width;
  uint16_t image_height;
  unsigned char flags;
#define DESCRIPTOR_FLAGS__PIXEL_SIZE 0x07
#define DESCRIPTOR_FLAGS__RESERVED 0x18
#define DESCRIPTOR_FLAGS__SORT_FLAG 0x20
#define DESCRIPTOR_FLAGS__INTERLACE_FLAG 0x40
#define DESCRIPTOR_FLAGS__LOCAL_COLOR_TABLE_FLAG 0x80
};

#define GIF_DESCRIPTOR_SIZE 10
#define GIF_DESCRIPTOR_SPEC "chhhhc"
#define GIF_DESCRIPTOR_FIELDS(p) \
 &(p)->image_separator, \
 &(p)->image_left, \
 &(p)->image_top, \
 &(p)->image_width, \
 &(p)->image_height, \
 &(p)->flags

struct GifExtension
{
  unsigned char extension_introducer;
  unsigned char graphic_control_label;
};

/**
 * Skip a data block.
 * @return the position after the block
 */
static size_t
skipDataBlock (const unsigned char *data, size_t pos, const size_t size)
{
  while ((pos < size) && (data[pos] != 0))
    pos += data[pos] + 1;
  return pos + 1;
}

/**
 * skip an extention block
 * @return the position after the block
 */
static size_t
skipExtensionBlock (const unsigned char *data,
                    size_t pos, const size_t size, 
		    const struct GifExtension * ext)
{
  return skipDataBlock (data, pos + sizeof (struct GifExtension), size);
}

/**
 * @return the offset after the global color map
 */
static size_t
skipGlobalColorMap (const unsigned char *data,
                    const size_t size,
		    const struct GifHeader * header)
{
  size_t gct_size;

  if ((header->flags & HEADER_FLAGS__GLOBAL_COLOR_TABLE_FLAG) > 0)
    gct_size =
      3 *
      (1 << ((header->flags & HEADER_FLAGS__SIZE_OF_GLOBAL_COLOR_TABLE) + 1));
  else
    gct_size = 0;
  return GIF_HEADER_SIZE + gct_size;
}

/**
 * @return the offset after the local color map
 */
static size_t
skipLocalColorMap (const unsigned char *data,
                   size_t pos, const size_t size,
		   const struct GifDescriptor * descriptor)
{
  size_t lct_size;

  if (pos + GIF_DESCRIPTOR_SIZE > size)
    return size;
  if ((descriptor->flags & DESCRIPTOR_FLAGS__LOCAL_COLOR_TABLE_FLAG) > 0)
    lct_size =
      3 * (1 << ((descriptor->flags & DESCRIPTOR_FLAGS__PIXEL_SIZE) + 1));
  else
    lct_size = 0;
  return pos + GIF_DESCRIPTOR_SIZE + lct_size;
}

static int
parseComment (const unsigned char *data,
              size_t pos, const size_t size, 
	      EXTRACTOR_MetaDataProcessor proc,
	      void *proc_cls)
{
  size_t length;
  size_t off;
  size_t curr = pos;
  int ret;

  length = 0;
  while ( (curr < size) && 
	  (data[curr] != 0) )
    {
      length += data[curr];
      curr += data[curr] + 1;
      if (length > 65536)
	break;
    }
  if ( (length < 65536) &&
       (curr < size) )
    {
      char comment[length+1];

      curr = pos;
      off = 0;
      while ((data[curr] != 0) && (curr < size))
	{
	  if (off + data[curr] >= size)
	    break;
	  memcpy (&comment[off], 
		  &data[curr] + 1,
		  data[curr]);
	  off += data[curr];
	  curr += data[curr] + 1;
	}
      comment[off] = '\0';
      ret = proc (proc_cls, 
		  "gif",
		  EXTRACTOR_METATYPE_COMMENT,
		  EXTRACTOR_METAFORMAT_UTF8,
		  "text/plain",
		  comment,
		  length+1);
    }
  else
    {
      /* too big */
      ret = 0;
    }
  return ret;
}


int 
EXTRACTOR_gif_extract (const unsigned char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  size_t pos;
  struct GifHeader header;
  struct GifDescriptor gd;
  char tmp[128];

  if (size < GIF_HEADER_SIZE)
    return 0;
  EXTRACTOR_common_cat_unpack (data, GIF_HEADER_SPEC, GIF_HEADER_FIELDS (&header));
  if (0 != strncmp (&header.gif[0], "GIF", 3))
    return 0;
  if (0 != strncmp (&header.version[0], "89a", 3))
    return 0;                /* only 89a has support for comments */
  if (0 != proc (proc_cls, 
		 "gif",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "image/gif",
		 strlen ("image/gif")+1))
    return 1;
  snprintf (tmp, 
	    sizeof(tmp),
	    "%ux%u", 
	    header.screen_width, header.screen_height);
  if (0 != proc (proc_cls, 
		 "gif",
		 EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 tmp,
		 strlen (tmp)+1))
    return 1;
  pos = skipGlobalColorMap (data, size, &header);
  PRINT ("global color map ends at %d\n", pos);
  while (pos < size)
    {
      switch (data[pos])
        {
        case ',':              /* image descriptor block */
          PRINT ("skipping local color map %d\n", pos);
          EXTRACTOR_common_cat_unpack (&data[pos],
				       GIF_DESCRIPTOR_SPEC,
				       GIF_DESCRIPTOR_FIELDS (&gd));
          pos = skipLocalColorMap (data, pos, size, &gd);
          break;
        case '!':              /* extension block */
          PRINT ("skipping extension block %d\n", pos);
          if (data[pos + 1] == (unsigned char) 0xFE)
            {
              if (0 != parseComment (data, pos + 2, size, proc, proc_cls))
		return 1;
            }
          pos = skipExtensionBlock (data, pos, size,
                                    (const struct GifExtension *) & data[pos]);
          break;
        case ';':
          PRINT ("hit terminator at %d!\n", pos);
          return 0;        /* terminator! */
        default:               /* raster data block */
          PRINT ("skipping data block at %d\n", pos);
          pos = skipDataBlock (data, pos + 1, size);
          break;
        }
    }
  PRINT ("returning at %d\n", pos);
  return 0;
}
