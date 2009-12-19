/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2006, 2009 Vidyut Samanta and Christian Grothoff

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

#define DEBUG_EXTRACT_ID3v24 0

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


static struct EXTRACTOR_Keywords *
addKeyword (EXTRACTOR_KeywordList * oldhead,
            char *phrase, EXTRACTOR_KeywordType type)
{
  EXTRACTOR_KeywordList *keyword;

  keyword = malloc (sizeof (EXTRACTOR_KeywordList));
  keyword->next = oldhead;
  keyword->keyword = phrase;
  keyword->keywordType = type;
  return keyword;
}

typedef struct
{
  char *text;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tmap[] = {
  {"COMM", EXTRACTOR_METATYPE_COMMENT},
  {"IPLS", EXTRACTOR_METATYPE_CONTRIBUTOR},
  {"TIPL", EXTRACTOR_METATYPE_CONTRIBUTOR},
  {"TMOO", EXTRACTOR_METATYPE_MOOD},
  {"TMCL", EXTRACTOR_METATYPE_MUSICIAN_CREDITS_LIST},
  {"LINK", EXTRACTOR_METATYPE_LINK},
  {"MCDI", EXTRACTOR_METATYPE_MUSIC_CD_IDENTIFIER},
  {"PCNT", EXTRACTOR_METATYPE_PLAY_COUNTER},
  {"POPM", EXTRACTOR_METATYPE_POPULARITY_METER},
  {"TCOP", EXTRACTOR_METATYPE_COPYRIGHT},
  {"TDRC", EXTRACTOR_METATYPE_DATE},
  {"TCON", EXTRACTOR_METATYPE_GENRE},
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
  {"TIME", EXTRACTOR_METATYPE_TIME},
  {"TMED", EXTRACTOR_METATYPE_MEDIA_TYPE},
  {"TCOM", EXTRACTOR_METATYPE_CREATOR},
  {"TOFN", EXTRACTOR_METATYPE_FILENAME},
  {"TOPE", EXTRACTOR_METATYPE_ARTIST},
  {"TPUB", EXTRACTOR_METATYPE_PUBLISHER},
  {"TRCK", EXTRACTOR_METATYPE_TRACK_NUMBER},
  {"TRSC", EXTRACTOR_METATYPE_ISRC},
  {"TRSN", EXTRACTOR_METATYPE_SOURCE},
  {"TRSO", EXTRACTOR_METATYPE_CREATED_FOR},
  {"TSRC", EXTRACTOR_METATYPE_RESOURCE_IDENTIFIER},
  {"TYER", EXTRACTOR_METATYPE_YEAR},
  {"TOAL", EXTRACTOR_METATYPE_ALBUM},
  {"TALB", EXTRACTOR_METATYPE_ALBUM},
  {"TLAN", EXTRACTOR_METATYPE_LANGUAGE},
  {"TIT2", EXTRACTOR_METATYPE_TITLE},
  {"TIT3", EXTRACTOR_METATYPE_DESCRIPTION},
  {"WCOM", EXTRACTOR_METATYPE_RELEASE},
  {"WCOP", EXTRACTOR_METATYPE_DISCLAIMER},
  {"", EXTRACTOR_METATYPE_KEYWORDS},
  {NULL, 0}
};


/* mimetype = audio/mpeg */
int 
EXTRACTOR_id3v24_extract (const unsigned char *data,
			  size_t size,
			  EXTRACTOR_MetaDataProcessor proc,
			  void *proc_cls,
			  const char *options)
{
  int unsync;
  int extendedHdr;
  int experimental;
  int footer;
  unsigned int tsize;
  unsigned int pos;
  unsigned int ehdrSize;
  unsigned int padding;

  if ((size < 16) ||
      (data[0] != 0x49) ||
      (data[1] != 0x44) ||
      (data[2] != 0x33) || (data[3] != 0x04) || (data[4] != 0x00))
    return prev;
  unsync = (data[5] & 0x80) > 0;
  extendedHdr = (data[5] & 0x40) > 0;
  experimental = (data[5] & 0x20) > 0;
  footer = (data[5] & 0x10) > 0;
  tsize = (((data[6] & 0x7F) << 21) |
           ((data[7] & 0x7F) << 14) |
           ((data[8] & 0x7F) << 7) | ((data[9] & 0x7F) << 0));
  if ((tsize + 10 > size) || (experimental))
    return prev;
  pos = 10;
  padding = 0;
  if (extendedHdr)
    {
      ehdrSize = (((data[10] & 0x7F) << 21) |
                  ((data[11] & 0x7F) << 14) |
                  ((data[12] & 0x7F) << 7) | ((data[13] & 0x7F) << 0));
      pos += ehdrSize;
    }


  while (pos < tsize)
    {
      size_t csize;
      int i;
      unsigned short flags;

      if (pos + 10 > tsize)
        return prev;

      csize = (((data[pos + 4] & 0x7F) << 21) |
               ((data[pos + 5] & 0x7F) << 14) |
               ((data[pos + 6] & 0x7F) << 7) | ((data[pos + 7] & 0x7F) << 0));

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

              /* this byte describes the encoding
                 try to convert strings to UTF-8
                 if it fails, then forget it */
              csize--;
              switch (data[pos + 10])
                {
                case 0x00:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
                                        csize, "ISO-8859-1");
                  break;
                case 0x01:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
                                        csize, "UTF-16");
                  break;
                case 0x02:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 11],
                                        csize, "UTF-16BE");
                  break;
                case 0x03:
                  word = malloc (csize + 1);
                  memcpy (word, &data[pos + 11], csize);
                  word[csize] = '\0';
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
                  prev = addKeyword (prev, word, tmap[i].type);
                }
              else
                {
                  free (word);
                }
              break;
            }
          i++;
        }
      pos += 10 + csize;
    }
  return prev;
}

/* end of id3v24_extractor.c */
