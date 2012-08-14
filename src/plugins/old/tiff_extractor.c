/*
     This file is part of libextractor.
     (C) 2004, 2009 Vidyut Samanta and Christian Grothoff

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

#define DEBUG 0

static int
addKeyword (EXTRACTOR_MetaDataProcessor proc,
	    void *proc_cls,
            const char *keyword, 
	    enum EXTRACTOR_MetaType type)
{
  if (keyword == NULL)
    return 0;
  return proc (proc_cls,
	       "tiff",
	       type,
	       EXTRACTOR_METAFORMAT_UTF8,
	       "text/plain",
	       keyword,
	       strlen(keyword)+1);
}

typedef struct
{
  unsigned short byteorder;
  unsigned short fourty_two;
  unsigned int ifd_offset;
} TIFF_HEADER;
#define TIFF_HEADER_SIZE 8
#define TIFF_HEADER_FIELDS(p) \
  &(p)->byteorder,	      \
    &(p)->fourty_two,	      \
    &(p)->ifd_offset
static char *TIFF_HEADER_SPECS[] = {
  "hhw",
  "HHW",
};

typedef struct
{
  unsigned short tag;
  unsigned short type;
  unsigned int count;
  unsigned int value_or_offset;
} DIRECTORY_ENTRY;
#define DIRECTORY_ENTRY_SIZE 12
#define DIRECTORY_ENTRY_FIELDS(p)		\
  &(p)->tag,					\
    &(p)->type,					\
    &(p)->count,				\
    &(p)->value_or_offset
static char *DIRECTORY_ENTRY_SPECS[] = {
  "hhww",
  "HHWW"
};

#define TAG_LENGTH 0x101
#define TAG_WIDTH 0x100
#define TAG_SOFTWARE 0x131
#define TAG_DAYTIME 0x132
#define TAG_ARTIST 0x315
#define TAG_COPYRIGHT 0x8298
#define TAG_DESCRIPTION 0x10E
#define TAG_DOCUMENT_NAME 0x10D
#define TAG_HOST 0x13C
#define TAG_SCANNER 0x110
#define TAG_ORIENTATION 0x112

#define TYPE_BYTE 1
#define TYPE_ASCII 2
#define TYPE_SHORT 3
#define TYPE_LONG 4
#define TYPE_RATIONAL 5

static int
addASCII (EXTRACTOR_MetaDataProcessor proc,
	  void *proc_cls,
          const char *data,
          size_t size, DIRECTORY_ENTRY * entry,
	  enum EXTRACTOR_MetaType type)
{
  if (entry->count > size)
    return 0;                     /* invalid! */
  if (entry->type != TYPE_ASCII)
    return 0;                     /* huh? */
  if (entry->count + entry->value_or_offset > size)
    return 0;
  if (data[entry->value_or_offset + entry->count - 1] != 0)
    return 0;
  return addKeyword (proc, proc_cls,
		     &data[entry->value_or_offset], type);
}


int 
EXTRACTOR_tiff_extract (const char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  TIFF_HEADER hdr;
  int byteOrder;                /* 0: do not convert;
                                   1: do convert */
  unsigned int current_ifd;
  unsigned int length = -1;
  unsigned int width = -1;

  if (size < TIFF_HEADER_SIZE)
    return 0;                /*  can not be tiff */
  if ((data[0] == 0x49) && (data[1] == 0x49))
    byteOrder = 0;
  else if ((data[0] == 0x4D) && (data[1] == 0x4D))
    byteOrder = 1;
  else
    return 0;                /* can not be tiff */
#if __BYTE_ORDER == __BIG_ENDIAN
  byteOrder = 1 - byteOrder;
#endif
  EXTRACTOR_common_cat_unpack (data, TIFF_HEADER_SPECS[byteOrder], TIFF_HEADER_FIELDS (&hdr));
  if (hdr.fourty_two != 42)
    return 0;                /* can not be tiff */
  if (hdr.ifd_offset + 6 > size)
    return 0;                /* malformed tiff */
  if (0 != addKeyword (proc, proc_cls, "image/tiff", EXTRACTOR_METATYPE_MIMETYPE))
    return 1;
  current_ifd = hdr.ifd_offset;
  while (current_ifd != 0)
    {
      unsigned short len;
      unsigned int off;
      int i;
      if ( (current_ifd + 6 > size) ||
	   (current_ifd + 6 < current_ifd) )
        return 0;
      if (byteOrder == 0)
        len = data[current_ifd + 1] << 8 | data[current_ifd];
      else
        len = data[current_ifd] << 8 | data[current_ifd + 1];
      if (len * DIRECTORY_ENTRY_SIZE + 2 + 4 + current_ifd > size)
        {
#if DEBUG
          printf ("WARNING: malformed tiff\n");
#endif
          return 0;
        }
      for (i = 0; i < len; i++)
        {
          DIRECTORY_ENTRY entry;
          off = current_ifd + 2 + DIRECTORY_ENTRY_SIZE * i;

          EXTRACTOR_common_cat_unpack (&data[off],
                      DIRECTORY_ENTRY_SPECS[byteOrder],
                      DIRECTORY_ENTRY_FIELDS (&entry));
          switch (entry.tag)
            {
            case TAG_LENGTH:
              if ((entry.type == TYPE_SHORT) && (byteOrder == 1))
                {
                  length = entry.value_or_offset >> 16;
                }
              else
                {
                  length = entry.value_or_offset;
                }
              if (width != -1)
                {
                  char tmp[128];
                  snprintf (tmp, 
			    sizeof(tmp), "%ux%u",
			    width, length);
                  addKeyword (proc, 
			      proc_cls, 
			      tmp, 
			      EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
                }
              break;
            case TAG_WIDTH:
              if ((entry.type == TYPE_SHORT) && (byteOrder == 1))
                width = entry.value_or_offset >> 16;
              else
                width = entry.value_or_offset;
              if (length != -1)
                {
                  char tmp[128];
                  snprintf (tmp, 
			    sizeof(tmp), 
			    "%ux%u",
			    width, length);
                  addKeyword (proc, proc_cls, 
			      tmp, 
			      EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
                }
              break;
            case TAG_SOFTWARE:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE))
		return 1;
              break;
            case TAG_ARTIST:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_ARTIST))
		return 1;
              break;
            case TAG_DOCUMENT_NAME:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_TITLE))
		return 1;
              break;
            case TAG_COPYRIGHT:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_COPYRIGHT))
		return 1;
              break;
            case TAG_DESCRIPTION:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_DESCRIPTION))
		return 1;
              break;
            case TAG_HOST:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_BUILDHOST))
		return 1;
              break;
            case TAG_SCANNER:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_SOURCE))
		return 1;
              break;
            case TAG_DAYTIME:
              if (0 != addASCII (proc, proc_cls, data, size, &entry, EXTRACTOR_METATYPE_CREATION_DATE))
		return 1;
              break;
            }
        }

      off = current_ifd + 2 + DIRECTORY_ENTRY_SIZE * len;
      if (byteOrder == 0)
        current_ifd =
          data[off + 3] << 24 | data[off + 2] << 16 | 
	  data[off + 1] << 8  | data[off];
      else
        current_ifd =
          data[off] << 24 | data[off + 1] << 16 |
	  data[off + 2] << 8 | data[off + 3];
    }
  return 0;
}
