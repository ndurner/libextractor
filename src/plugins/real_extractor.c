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
#include <stdint.h>

#define UINT32 uint32_t
#define UINT16 uint16_t
#define UINT8 uint8_t

typedef struct
{
  UINT32 object_id;
  UINT32 size;
  UINT16 object_version;        /* must be 0 */
  UINT16 stream_number;
  UINT32 max_bit_rate;
  UINT32 avg_bit_rate;
  UINT32 max_packet_size;
  UINT32 avg_packet_size;
  UINT32 start_time;
  UINT32 preroll;
  UINT32 duration;
  UINT8 stream_name_size;
  UINT8 data[0];                /* variable length section */
  /*
     UINT8[stream_name_size]     stream_name;
     UINT8                       mime_type_size;
     UINT8[mime_type_size]       mime_type;
     UINT32                      type_specific_len;
     UINT8[type_specific_len]    type_specific_data;
   */
} Media_Properties;

typedef struct
{
  UINT32 object_id;
  UINT32 size;
  UINT16 object_version;        /* must be 0 */
  UINT16 title_len;
  UINT8 data[0];                /* variable length section */
  /*
     UINT8[title_len]  title;
     UINT16    author_len;
     UINT8[author_len]  author;
     UINT16    copyright_len;
     UINT8[copyright_len]  copyright;
     UINT16    comment_len;
     UINT8[comment_len]  comment;
   */
} Content_Description;
/* author, copyright and comment are supposed to be ASCII */

#define REAL_HEADER 0x2E524d46
#define MDPR_HEADER 0x4D445052
#define CONT_HEADER 0x434F4e54

#define RAFF4_HEADER 0x2E7261FD


static int
processMediaProperties (const Media_Properties * prop,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls)
{

  UINT8 mime_type_size;
  UINT32 prop_size;

  prop_size = ntohl (prop->size);
  if (prop_size <= sizeof (Media_Properties))
    return 0;
  if (0 != prop->object_version)
    return 0;
  if (prop_size <= prop->stream_name_size + sizeof (UINT8)
      + sizeof (Media_Properties))
    return 0;

  mime_type_size = prop->data[prop->stream_name_size];
  if (prop_size > prop->stream_name_size + sizeof (UINT8) +
      +mime_type_size + sizeof (Media_Properties))
    {
      char data[mime_type_size + 1];
      memcpy (data, &prop->data[prop->stream_name_size + 1], mime_type_size);
      data[mime_type_size] = '\0';
      
      return proc (proc_cls,
		   "real",
		   EXTRACTOR_METATYPE_MIMETYPE,
		   EXTRACTOR_METAFORMAT_UTF8,
		   "text/plain",
		   data,
		   strlen (data));
    }
  return 0;
}

static int
processContentDescription (const Content_Description * prop,
			   EXTRACTOR_MetaDataProcessor proc,
			   void *proc_cls)
{
  UINT16 author_len;
  UINT16 copyright_len;
  UINT16 comment_len;
  UINT16 title_len;
  char *title;
  char *author;
  char *copyright;
  char *comment;
  UINT32 prop_size;
  int ret;

  prop_size = ntohl (prop->size);
  if (prop_size <= sizeof (Content_Description))
    return 0;
  if (0 != prop->object_version)
    return 0;
  title_len = ntohs (prop->title_len);
  if (prop_size <= title_len + sizeof (UINT16) + sizeof (Content_Description))
    return 0;
  author_len = ntohs (*(UINT16 *) & prop->data[title_len]);
  if (prop_size <= title_len + sizeof (UINT16)
      + author_len + sizeof (Content_Description))
    return 0;

  copyright_len = ntohs (*(UINT16 *) & prop->data[title_len +
                                                  author_len +
                                                  sizeof (UINT16)]);

  if (prop_size <= title_len + 2 * sizeof (UINT16)
      + author_len + copyright_len + sizeof (Content_Description))
    return 0;

  comment_len = ntohs (*(UINT16 *) & prop->data[title_len +
                                                author_len +
                                                copyright_len +
                                                2 * sizeof (UINT16)]);

  if (prop_size < title_len + 3 * sizeof (UINT16)
      + author_len + copyright_len + comment_len
      + sizeof (Content_Description))
    return 0;

  ret = 0;
  title = malloc (title_len + 1);
  if (title != NULL)
    {
      memcpy (title, &prop->data[0], title_len);
      title[title_len] = '\0';
      ret = proc (proc_cls,
		  "real",
		  EXTRACTOR_METATYPE_TITLE,
		  EXTRACTOR_METAFORMAT_UTF8,
		  "text/plain",
		  title,
		  strlen (title)+1);
      free (title);
    }
  if (ret != 0)
    return ret;

  author = malloc (author_len + 1);
  if (author != NULL)
    {
      memcpy (author, &prop->data[title_len + sizeof (UINT16)], author_len);
      author[author_len] = '\0';
      ret = proc (proc_cls,
		  "real",
		  EXTRACTOR_METATYPE_AUTHOR_NAME,
		  EXTRACTOR_METAFORMAT_UTF8,
		  "text/plain",
		  author,
		  strlen (author)+1);
      free (author);
    }
  if (ret != 0)
    return ret;

  copyright = malloc (copyright_len + 1);
  if (copyright != NULL)
    {
      memcpy (copyright,
	      &prop->data[title_len + sizeof (UINT16) * 2 + author_len],
	      copyright_len);
      copyright[copyright_len] = '\0';
      ret = proc (proc_cls,
		  "real",
		  EXTRACTOR_METATYPE_COPYRIGHT,
		  EXTRACTOR_METAFORMAT_UTF8,
		  "text/plain",
		  copyright,
		  strlen (copyright)+1);
      free (copyright);
    }
  if (ret != 0)
    return ret;

  comment = malloc (comment_len + 1);
  if (comment != NULL)
    {
      memcpy (comment,
	      &prop->data[title_len + sizeof (UINT16) * 3 + author_len +
			  copyright_len], comment_len);
      comment[comment_len] = '\0';
      ret = proc (proc_cls,
		  "real",
		  EXTRACTOR_METATYPE_COMMENT,
		  EXTRACTOR_METAFORMAT_UTF8,
		  "text/plain",
		  comment,
		  strlen (comment)+1);
      free (comment);
    }
  if (ret != 0)
    return ret;
  return 0;
}

typedef struct RAFF4_header
{
  unsigned short version;
  unsigned short revision;
  unsigned short header_length;
  unsigned short compression_type;
  unsigned int granularity;
  unsigned int total_bytes;
  unsigned int bytes_per_minute;
  unsigned int bytes_per_minute2;
  unsigned short interleave_factor;
  unsigned short interleave_block_size;
  unsigned int user_data;
  float sample_rate;
  unsigned short sample_size;
  unsigned short channels;
  unsigned char interleave_code[5];
  unsigned char compression_code[5];
  unsigned char is_interleaved;
  unsigned char copy_byte;
  unsigned char stream_type;
  /*
     unsigned char tlen;
     unsigned char title[tlen];
     unsigned char alen;
     unsigned char author[alen];
     unsigned char clen;
     unsigned char copyright[clen];
     unsigned char aplen;
     unsigned char app[aplen]; */
} RAFF4_header;

#define RAFF4_HDR_SIZE 53

static char *
stndup (const char *str, size_t n)
{
  char *tmp;
  tmp = malloc (n + 1);
  if (tmp == NULL)
    return NULL;
  tmp[n] = '\0';
  memcpy (tmp, str, n);
  return tmp;
}

/* audio/vnd.rn-realaudio */
int 
EXTRACTOR_real_extract (const unsigned char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  const unsigned char *pos;
  const unsigned char *end;
  unsigned int length;
  const RAFF4_header *hdr;
  unsigned char tlen;
  unsigned char alen;
  unsigned char clen;
  unsigned char aplen;
  char *x;
  int ret;

  if (size <= 2 * sizeof (int))
    return 0;
  if (RAFF4_HEADER == ntohl (*(int *) data))
    {
      /* HELIX */
      if (size <= RAFF4_HDR_SIZE + 16 + 4)
        return 0;
      if (0 != proc (proc_cls,
		     "real",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "audio/vnd.rn-realaudio",
		     strlen ("audio/vnd.rn-realaudio")+1))
	return 1;
      hdr = (const RAFF4_header *) &data[16];
      if (ntohs (hdr->header_length) + 16 > size)
        return 0;
      tlen = data[16 + RAFF4_HDR_SIZE];
      if (tlen + RAFF4_HDR_SIZE + 20 > size)
        return 0;
      alen = data[17 + tlen + RAFF4_HDR_SIZE];
      if (tlen + alen + RAFF4_HDR_SIZE + 20 > size)
        return 0;
      clen = data[18 + tlen + alen + RAFF4_HDR_SIZE];
      if (tlen + alen + clen + RAFF4_HDR_SIZE + 20 > size)
        return 0;
      aplen = data[19 + tlen + clen + alen + RAFF4_HDR_SIZE];
      if (tlen + alen + clen + aplen + RAFF4_HDR_SIZE + 20 > size)
        return 0;
      ret = 0;
      if ( (tlen > 0) && (ret == 0) )
	{
	  x = stndup ((const char *) &data[17 + RAFF4_HDR_SIZE], tlen);
	  if (x != NULL)
	    {
	      ret = proc (proc_cls,
			  "real",
			  EXTRACTOR_METATYPE_MIMETYPE,
			  EXTRACTOR_METAFORMAT_UTF8,
			  "text/plain",
			  x,
			  strlen (x)+1);
	      free (x);
	    }
	}
      if ( (alen > 0) && (ret == 0) )
	{
	  x = stndup ((const char *) &data[18 + RAFF4_HDR_SIZE + tlen], alen);
	  if (x != NULL)
	    {
	      ret = proc (proc_cls,
			  "real",
			  EXTRACTOR_METATYPE_MIMETYPE,
			  EXTRACTOR_METAFORMAT_UTF8,
			  "text/plain",
			  x,
			  strlen (x)+1);
	      free (x);
	    }
	}
      if ( (clen > 0) && (ret == 0) )
	{
	  x = stndup ((const char *) &data[19 + RAFF4_HDR_SIZE + tlen + alen], clen);
	  if (x != NULL)
	    {
	      ret = proc (proc_cls,
			  "real",
			  EXTRACTOR_METATYPE_MIMETYPE,
			  EXTRACTOR_METAFORMAT_UTF8,
			  "text/plain",
			  x,
			  strlen (x)+1);
	      free (x);
	    }
	}
      if ( (aplen > 0) && (ret == 0) )
	{
	  x = stndup ((const char *) &data[20 + RAFF4_HDR_SIZE + tlen + alen + clen], aplen);
	  if (x != NULL)
	    {
	      ret = proc (proc_cls,
			  "real",
			  EXTRACTOR_METATYPE_MIMETYPE,
			  EXTRACTOR_METAFORMAT_UTF8,
			  "text/plain",
			  x,
			  strlen (x)+1);
	      free (x);
	    }
	}
      return ret;
    }
  if (REAL_HEADER == ntohl (*(int *) data))
    {
      /* old real */
      end = &data[size];
      pos = &data[0];
      ret = 0;
      while (0 == ret)
        {
          if ((pos + 8 >= end) || (pos + 8 < pos))
            break;
          length = ntohl (*(((unsigned int *) pos) + 1));
          if (length <= 0)
            break;
          if ((pos + length >= end) || (pos + length < pos))
            break;
          switch (ntohl (*((unsigned int *) pos)))
            {
            case MDPR_HEADER:
              ret = processMediaProperties ((Media_Properties *) pos,
                                               proc,
					       proc_cls);
              pos += length;
              break;
            case CONT_HEADER:
              ret = processContentDescription ((Content_Description *) pos,
					       proc,
					       proc_cls);
              pos += length;
              break;
            case REAL_HEADER:  /* treat like default */
            default:
              pos += length;
              break;
            }
        }
      return ret;
    }
  return 0;
}
