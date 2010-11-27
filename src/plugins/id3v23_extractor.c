/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2006, 2007, 2009 Vidyut Samanta and Christian Grothoff

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
#define DEBUG_EXTRACT_ID3v23 0

#include "platform.h"
#include "extractor.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#ifndef MINGW
#include <sys/mman.h>
#endif

#include "convert.h"

enum Id3v23Fmt
  {
    T, /* simple, 0-terminated string, prefixed by encoding */
    U, /* 0-terminated ASCII string, no encoding */
    UL, /* unsync'ed lyrics */
    SL, /* sync'ed lyrics */
    L, /* string with language prefix */
    I /* image */
  };

typedef struct
{
  const char *text;
  enum EXTRACTOR_MetaType type;
  enum Id3v23Fmt fmt;
} Matches;

static Matches tmap[] = {
  {"TALB", EXTRACTOR_METATYPE_ALBUM, T},
  {"TBPM", EXTRACTOR_METATYPE_BEATS_PER_MINUTE, T},
  {"TCOM", EXTRACTOR_METATYPE_COMPOSER, T},
  {"TCON", EXTRACTOR_METATYPE_SONG_VERSION, T},
  {"TCOP", EXTRACTOR_METATYPE_COPYRIGHT, T},
  /* {"TDAT", EXTRACTOR_METATYPE_CREATION_DATE, T}, */
  /* TDLY */
  {"TENC", EXTRACTOR_METATYPE_ENCODED_BY, T},
  {"TEXT", EXTRACTOR_METATYPE_WRITER, T},  
  {"TFLT", EXTRACTOR_METATYPE_FORMAT_VERSION, T},
  /* TIME */
  {"TIT1", EXTRACTOR_METATYPE_SECTION, T},
  {"TIT2", EXTRACTOR_METATYPE_TITLE, T},
  {"TIT3", EXTRACTOR_METATYPE_SONG_VERSION, T},
  /* TKEY */
  {"TLAN", EXTRACTOR_METATYPE_LANGUAGE, T},
  {"TLEN", EXTRACTOR_METATYPE_DURATION, T}, /* FIXME: should append 'ms' as unit */
  {"TMED", EXTRACTOR_METATYPE_SOURCE, T}, 
  {"TOAL", EXTRACTOR_METATYPE_ORIGINAL_TITLE, T},
  {"TOFN", EXTRACTOR_METATYPE_ORIGINAL_ARTIST, T},
  {"TOLY", EXTRACTOR_METATYPE_ORIGINAL_WRITER, T},
  {"TOPE", EXTRACTOR_METATYPE_ORIGINAL_PERFORMER, T},
  {"TORY", EXTRACTOR_METATYPE_ORIGINAL_RELEASE_YEAR, T},
  {"TOWN", EXTRACTOR_METATYPE_LICENSEE, T},
  {"TPE1", EXTRACTOR_METATYPE_ARTIST, T},
  {"TPE2", EXTRACTOR_METATYPE_PERFORMER, T},
  {"TPE3", EXTRACTOR_METATYPE_CONDUCTOR, T},
  {"TPE4", EXTRACTOR_METATYPE_INTERPRETATION, T}, 
  {"TPOS", EXTRACTOR_METATYPE_DISC_NUMBER, T},
  {"TPUB", EXTRACTOR_METATYPE_PUBLISHER, T},
  {"TRCK", EXTRACTOR_METATYPE_TRACK_NUMBER, T},
  /* TRDA */
  {"TRSN", EXTRACTOR_METATYPE_NETWORK_NAME, T},
  /* TRSO */
  {"TSIZ", EXTRACTOR_METATYPE_EMBEDDED_FILE_SIZE, T},
  {"TSRC", EXTRACTOR_METATYPE_ISRC, T},
  /* TSSE */
  {"TYER", EXTRACTOR_METATYPE_PUBLICATION_YEAR, T},
  {"WCOM", EXTRACTOR_METATYPE_URL, U},
  {"WCOP", EXTRACTOR_METATYPE_URL, U},
  {"WOAF", EXTRACTOR_METATYPE_URL, U},
  {"WOAS", EXTRACTOR_METATYPE_URL, U},
  {"WORS", EXTRACTOR_METATYPE_URL, U},
  {"WPAY", EXTRACTOR_METATYPE_URL, U},
  {"WPUB", EXTRACTOR_METATYPE_URL, U},
  {"WXXX", EXTRACTOR_METATYPE_URL, T},
  {"IPLS", EXTRACTOR_METATYPE_CONTRIBUTOR_NAME, T},
  /* ... */
  {"USLT", EXTRACTOR_METATYPE_LYRICS, UL },
  {"SYLT", EXTRACTOR_METATYPE_LYRICS, SL },
  {"COMM", EXTRACTOR_METATYPE_COMMENT, L},
  /* ... */
  {"APIC", EXTRACTOR_METATYPE_PICTURE, I},
  /* ... */
  {"LINK", EXTRACTOR_METATYPE_URL, U},
  /* ... */
  {"USER", EXTRACTOR_METATYPE_LICENSE, T},
  /* ... */
  {NULL, 0, T}
};


/* mimetype = audio/mpeg */
int 
EXTRACTOR_id3v23_extract (const unsigned char *data,
			  size_t size,
			  EXTRACTOR_MetaDataProcessor proc,
			  void *proc_cls,
			  const char *options)
{
  int unsync;
  int extendedHdr;
  int experimental;
  uint32_t tsize;
  uint32_t pos;
  uint32_t ehdrSize;
  uint32_t padding;
  uint32_t csize;
  int i;
  uint16_t flags;
  char *mime;
  enum EXTRACTOR_MetaType type;
  size_t off;
  int obo;

  if ((size < 16) ||
      (data[0] != 0x49) ||
      (data[1] != 0x44) ||
      (data[2] != 0x33) || (data[3] != 0x03) || (data[4] != 0x00))
    return 0;
  unsync = (data[5] & 0x80) > 0;
  if (unsync)
    return 0; /* not supported */
  extendedHdr = (data[5] & 0x40) > 0;
  experimental = (data[5] & 0x20) > 0;
  if (experimental)
    return 0;
  tsize = (((data[6] & 0x7F) << 21) |
           ((data[7] & 0x7F) << 14) |
           ((data[8] & 0x7F) << 7) | ((data[9] & 0x7F) << 0));
  if (tsize + 10 > size)
    return 0;
  pos = 10;
  padding = 0;
  if (extendedHdr)
    {
      ehdrSize = (((data[10]) << 24) |
                  ((data[11]) << 16) | ((data[12]) << 8) | ((data[12]) << 0));

      padding = (((data[15]) << 24) |
                 ((data[16]) << 16) | ((data[17]) << 8) | ((data[18]) << 0));
      pos += 4 + ehdrSize;
      if (padding < tsize)
        tsize -= padding;
      else
        return 0;
    }


  while (pos < tsize)
    {
      if (pos + 10 > tsize)
        return 0;
      csize =
        (data[pos + 4] << 24) + (data[pos + 5] << 16) + (data[pos + 6] << 8) +
        data[pos + 7];
      if ((pos + 10 + csize > tsize) || (csize > tsize) || (csize == 0) ||
	  (pos + 10 + csize <= pos + 10) || (pos + 10 <= pos))
        break;
      flags = (data[pos + 8] << 8) + data[pos + 9];
      if (((flags & 0x80) > 0) /* compressed, not yet supported */  ||
          ((flags & 0x40) > 0) /* encrypted, not supported */ )
        {
          pos += 10 + csize;
          continue;
        }
      i = 0;
      while (tmap[i].text != NULL)
        {
          if (0 == strncmp (tmap[i].text, (const char *) &data[pos], 4))
            {
              char *word;
              if ((flags & 0x20) > 0)
                {
                  /* "group" identifier, skip a byte */
                  pos++;
                  csize--;
                }
	      switch (tmap[i].fmt)
		{
		case T:
		  /* this byte describes the encoding
		     try to convert strings to UTF-8
		     if it fails, then forget it */
		  switch (data[pos + 10])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
							       csize - 1, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
							       csize - 1, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
							       csize - 1, "ISO-8859-1");
		      break;
		    }
		  break;
		case U:
		  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 10],
							   csize, "ISO-8859-1");
		  break;
		case UL:
		  if (csize < 6)
		    return 0; /* malformed */
		  /* find end of description */
		  off = 14;
		  while ( (off < size) &&
			  (off - pos < csize) &&
			  (data[pos + off] == '\0') )
		    off++;
		  if ( (off >= csize) ||
		       (data[pos+off] != '\0') )
		    return 0; /* malformed */
		  off++;
		  switch (data[pos + 10])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + off],
							       csize - off, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + off],
							       csize - off, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + off],
							       csize - off, "ISO-8859-1");
		      break;
		    }
		  break;
		case SL:
		  if (csize < 7)
		    return 0; /* malformed */
		  /* find end of description */
		  switch (data[pos + 10])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 16],
							       csize - 6, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 16],
							       csize - 6, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 16],
							       csize - 6, "ISO-8859-1");
		      break;
		    }
		  break;
		case L:
		  if (csize < 5)
		    return 0; /* malformed */
		  /* find end of description */
		  obo = data[pos + 14] == '\0' ? 1 : 0; /* someone put a \0 in front of comments... */
		  if (csize < 6)
		    obo = 0;
		  switch (data[pos + 10])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 14 + obo],
							       csize - 4 - obo, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 14 + obo],
							       csize - 4 - obo, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 14 + obo],
							       csize - 4 - obo, "ISO-8859-1");
		      break;
		    }
		  break;
		case I:
		  if (csize < 2)
		    return 0; /* malformed */
		  /* find end of mime type */
		  off = 11;
		  while ( (off < size) &&
			  (off - pos < csize) &&
			  (data[pos + off] == '\0') )
		    off++;
		  if ( (off >= csize) ||
		       (data[pos+off] != '\0') )
		    return 0; /* malformed */
		  off++;
		  mime = strdup ((const char*) &data[pos + 11]);
		  
		  switch (data[pos+off])
		    {
		    case 0x03:
		    case 0x04:
		      type = EXTRACTOR_METATYPE_COVER_PICTURE;
		      break;
		    case 0x07:
		    case 0x08:
		    case 0x09:
		    case 0x0A:
		    case 0x0B:
		    case 0x0C:
		      type = EXTRACTOR_METATYPE_CONTRIBUTOR_PICTURE;
		      break;
		    case 0x0D:
		    case 0x0E:
		    case 0x0F:
		      type = EXTRACTOR_METATYPE_EVENT_PICTURE;
		      break;
		    case 0x14:
		      type = EXTRACTOR_METATYPE_LOGO;
		      type = EXTRACTOR_METATYPE_LOGO;
		      break;
		    default:
		      type = EXTRACTOR_METATYPE_PICTURE;
		      break;
		    }
		  off++;

		  /* find end of description */
		  while ( (off < size) &&
			  (off - pos < csize) &&
			  (data[pos + off] == '\0') )
		    off++;
		  if ( (off >= csize) ||
		       (data[pos+off] != '\0') )
		    {
		      if (mime != NULL)
			free (mime);
		      return 0; /* malformed */
		    }
		  off++;
		  if ( (mime != NULL) &&
		       (0 == strcasecmp ("-->",
					 mime)) )
		    {
		      /* not supported */
		    }
		  else
		    {
		      if (0 != proc (proc_cls,
				     "id3v23",
				     type,
				     EXTRACTOR_METAFORMAT_BINARY,
				     mime,
				     (const char*) &data[pos + off],
				     csize + 6 - off))			
			{
			  if (mime != NULL)
			    free (mime);
			  return 1;
			}
		    }
		  if (mime != NULL)
		    free (mime);
		  word = NULL;
		  break;
		default:
		  return 0;
		}	      
              if ((word != NULL) && (strlen (word) > 0))
                {
		  if (0 != proc (proc_cls,
				 "id3v23",
				 tmap[i].type,
				 EXTRACTOR_METAFORMAT_UTF8,
				 "text/plain",
				 word,
				 strlen(word)+1))
		    {
		      free (word);
		      return 1;
		    }
                }
	      if (word != NULL)
		free (word);
              break;
            }
          i++;
        }
      pos += 10 + csize;
    }
  return 0;
}

/* end of id3v23_extractor.c */
