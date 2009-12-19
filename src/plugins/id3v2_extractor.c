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

#include "platform.h"
#include "extractor.h"
#ifndef MINGW
#include <sys/mman.h>
#endif
#include "convert.h"

#define DEBUG_EXTRACT_ID3v2 0

typedef struct
{
  const char *text;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tmap[] = {
  {"TAL", EXTRACTOR_METATYPE_TITLE},
  {"TT1", EXTRACTOR_METATYPE_GROUP},
  {"TT2", EXTRACTOR_METATYPE_TITLE},
  {"TT3", EXTRACTOR_METATYPE_TITLE},
  {"TXT", EXTRACTOR_METATYPE_DESCRIPTION},
  {"TPB", EXTRACTOR_METATYPE_PUBLISHER},
  {"WAF", EXTRACTOR_METATYPE_LOCATION},
  {"WAR", EXTRACTOR_METATYPE_LOCATION},
  {"WAS", EXTRACTOR_METATYPE_LOCATION},
  {"WCP", EXTRACTOR_METATYPE_COPYRIGHT},
  {"WAF", EXTRACTOR_METATYPE_LOCATION},
  {"WCM", EXTRACTOR_METATYPE_DISCLAIMER},
  {"TSS", EXTRACTOR_METATYPE_FORMAT},
  {"TYE", EXTRACTOR_METATYPE_DATE},
  {"TLA", EXTRACTOR_METATYPE_LANGUAGE},
  {"TP1", EXTRACTOR_METATYPE_ARTIST},
  {"TP2", EXTRACTOR_METATYPE_ARTIST},
  {"TP3", EXTRACTOR_METATYPE_CONDUCTOR},
  {"TP4", EXTRACTOR_METATYPE_INTERPRET},
  {"IPL", EXTRACTOR_METATYPE_CONTRIBUTOR},
  {"TOF", EXTRACTOR_METATYPE_FILENAME},
  {"TEN", EXTRACTOR_METATYPE_PRODUCER},
  {"TCO", EXTRACTOR_METATYPE_SUBJECT},
  {"TCR", EXTRACTOR_METATYPE_COPYRIGHT},
  {"SLT", EXTRACTOR_METATYPE_LYRICS},
  {"TOA", EXTRACTOR_METATYPE_ARTIST},
  {"TRC", EXTRACTOR_METATYPE_ISRC},
  {"TRK", EXTRACTOR_METATYPE_TRACK_NUMBER},
  {"TCM", EXTRACTOR_METATYPE_CREATOR},
  {"TOT", EXTRACTOR_METATYPE_ALBUM},
  {"TOL", EXTRACTOR_METATYPE_AUTHOR},
  {"COM", EXTRACTOR_METATYPE_COMMENT},
  {"", EXTRACTOR_METATYPE_KEYWORDS},
  {NULL, 0},
};


/* mimetype = audio/mpeg */
int 
EXTRACTOR_id3v2_extract (const unsigned char *data,
			 size_t size,
			 EXTRACTOR_MetaDataProcessor proc,
			 void *proc_cls,
			 const char *options)
{
  int unsync;
  unsigned int tsize;
  unsigned int pos;

  if ((size < 16) ||
      (data[0] != 0x49) ||
      (data[1] != 0x44) ||
      (data[2] != 0x33) || (data[3] != 0x02) || (data[4] != 0x00))
    return 0;
  unsync = (data[5] & 0x80) > 0;
  tsize = (((data[6] & 0x7F) << 21) |
           ((data[7] & 0x7F) << 14) |
           ((data[8] & 0x7F) << 07) | ((data[9] & 0x7F) << 00));

  if (tsize + 10 > size)
    return 0;
  pos = 10;
  while (pos < tsize)
    {
      size_t csize;
      int i;

      if (pos + 6 > tsize)
        return 0;
      csize = (data[pos + 3] << 16) + (data[pos + 4] << 8) + data[pos + 5];
      if ((pos + 6 + csize > tsize) || (csize > tsize) || (csize == 0))
        break;
      i = 0;
      while (tmap[i].text != NULL)
        {
          if (0 == strncmp (tmap[i].text, (const char *) &data[pos], 3))
            {
              char *word;
              /* this byte describes the encoding
                 try to convert strings to UTF-8
                 if it fails, then forget it */
              switch (data[pos + 6])
                {
                case 0x00:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 7],
                                        csize, "ISO-8859-1");
                  break;
                case 0x01:
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 7],
                                        csize, "UCS-2");
                  break;
                default:
                  /* bad encoding byte,
                     try to convert from iso-8859-1 */
                  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 7],
                                        csize, "ISO-8859-1");
                  break;
                }
              pos++;
              csize--;
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
      pos += 6 + csize;
    }
  return 0;
}

/* end of id3v2_extractor.c */
