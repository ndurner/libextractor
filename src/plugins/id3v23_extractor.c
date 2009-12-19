/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2006, 2007 Vidyut Samanta and Christian Grothoff

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

typedef struct
{
  const char *text;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tmap[] = {
  {"COMM", EXTRACTOR_METATYPE_COMMENT},
  {"IPLS", EXTRACTOR_METATYPE_CONTRIBUTOR},
  {"LINK", EXTRACTOR_METATYPE_LINK},
  {"MCDI", EXTRACTOR_METATYPE_MUSIC_CD_IDENTIFIER},
  {"PCNT", EXTRACTOR_METATYPE_PLAY_COUNTER},
  {"POPM", EXTRACTOR_METATYPE_POPULARITY_METER},
  {"TCOP", EXTRACTOR_METATYPE_COPYRIGHT},
  {"TDAT", EXTRACTOR_METATYPE_DATE},
  {"TCON", EXTRACTOR_METATYPE_CONTENT_TYPE},
  {"TIT1", EXTRACTOR_METATYPE_GENRE},
  {"TENC", EXTRACTOR_METATYPE_ENCODED_BY},
  {"TEXT", EXTRACTOR_METATYPE_LYRICS},
  {"TOLY", EXTRACTOR_METATYPE_CONTRIBUTOR},
  {"TOPE", EXTRACTOR_METATYPE_CONTRIBUTOR},
  {"TOWN", EXTRACTOR_METATYPE_OWNER},
  {"TPE1", EXTRACTOR_METATYPE_ARTIST},
  {"TPE2", EXTRACTOR_METATYPE_ARTIST},
  {"TPE3", EXTRACTOR_METATYPE_CONDUCTOR},
  {"TPE4", EXTRACTOR_METATYPE_INTERPRET},
  {"TMED", EXTRACTOR_METATYPE_MEDIA_TYPE},
  {"TCOM", EXTRACTOR_METATYPE_CREATOR},
  {"TIME", EXTRACTOR_METATYPE_TIME},
  {"TOFN", EXTRACTOR_METATYPE_FILENAME},
  {"TOPE", EXTRACTOR_METATYPE_ARTIST},
  {"TPUB", EXTRACTOR_METATYPE_PUBLISHER},
  {"TRCK", EXTRACTOR_METATYPE_TRACK_NUMBER},
  {"TRSC", EXTRACTOR_METATYPE_ISRC},
  {"TRSN", EXTRACTOR_METATYPE_SOURCE},
  {"TRSO", EXTRACTOR_METATYPE_CREATED_FOR},
  {"TSRC", EXTRACTOR_METATYPE_RESOURCE_IDENTIFIER},
  {"TOAL", EXTRACTOR_METATYPE_ALBUM},
  {"TALB", EXTRACTOR_METATYPE_ALBUM},
  {"TLAN", EXTRACTOR_METATYPE_LANGUAGE},
  {"TYER", EXTRACTOR_METATYPE_YEAR},
  {"TLEN", EXTRACTOR_METATYPE_DURATION},
  {"TIT2", EXTRACTOR_METATYPE_TITLE},
  {"TIT3", EXTRACTOR_METATYPE_DESCRIPTION},
  {"WCOM", EXTRACTOR_METATYPE_RELEASE},
  {"WCOP", EXTRACTOR_METATYPE_DISCLAIMER},
  {"", EXTRACTOR_METATYPE_KEYWORDS},
  {NULL, 0}
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

  if ((size < 16) ||
      (data[0] != 0x49) ||
      (data[1] != 0x44) ||
      (data[2] != 0x33) || (data[3] != 0x03) || (data[4] != 0x00))
    return 0;
  unsync = (data[5] & 0x80) > 0;
  extendedHdr = (data[5] & 0x40) > 0;
  experimental = (data[5] & 0x20) > 0;
  tsize = (((data[6] & 0x7F) << 21) |
           ((data[7] & 0x7F) << 14) |
           ((data[8] & 0x7F) << 7) | ((data[9] & 0x7F) << 0));
  if ((tsize + 10 > size) || (experimental))
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
      if ((pos + 10 + csize > tsize) || (csize > tsize) || (csize == 0))
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
              csize--;
              /* this byte describes the encoding
                 try to convert strings to UTF-8
                 if it fails, then forget it */
              switch (data[pos + 10])
                {
                case 0x00:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
                                        csize, "ISO-8859-1");
                  break;
                case 0x01:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
                                        csize, "UCS-2");
                  break;
                default:
                  /* bad encoding byte,
                     try to convert from iso-8859-1 */
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
                                        csize, "ISO-8859-1");
                  break;
                }
              pos++;
              if ((word != NULL) && (strlen (word) > 0))
                {
		  if (0 != proc (proc_cls,
				 "id3v2",
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
