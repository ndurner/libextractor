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

enum Id3v2Fmt
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
  enum Id3v2Fmt fmt;
} Matches;

static Matches tmap[] = {
  /* skipping UFI */
  {"TT1", EXTRACTOR_METATYPE_SECTION, T},
  {"TT2", EXTRACTOR_METATYPE_TITLE, T},
  {"TT3", EXTRACTOR_METATYPE_SONG_VERSION, T},
  {"TP1", EXTRACTOR_METATYPE_ARTIST, T},
  {"TP2", EXTRACTOR_METATYPE_PERFORMER, T},
  {"TP3", EXTRACTOR_METATYPE_CONDUCTOR, T},
  {"TP4", EXTRACTOR_METATYPE_INTERPRETATION, T},
  {"TCM", EXTRACTOR_METATYPE_COMPOSER, T},
  {"TXT", EXTRACTOR_METATYPE_WRITER, T},
  {"TLA", EXTRACTOR_METATYPE_LANGUAGE, T},
  {"TCO", EXTRACTOR_METATYPE_GENRE, T},
  {"TAL", EXTRACTOR_METATYPE_ALBUM, T},
  {"TPA", EXTRACTOR_METATYPE_DISC_NUMBER, T},
  {"TRK", EXTRACTOR_METATYPE_TRACK_NUMBER, T},
  {"TRC", EXTRACTOR_METATYPE_ISRC, T},
  {"TYE", EXTRACTOR_METATYPE_PUBLICATION_YEAR, T},
  /*
    FIXME: these two and TYE should be combined into
    the actual publication date (if TRD is missing)
  {"TDA", EXTRACTOR_METATYPE_PUBLICATION_DATE},
  {"TIM", EXTRACTOR_METATYPE_PUBLICATION_DATE},
  */
  {"TRD", EXTRACTOR_METATYPE_CREATION_TIME, T},
  {"TMT", EXTRACTOR_METATYPE_SOURCE, T},
  {"TFT", EXTRACTOR_METATYPE_FORMAT_VERSION, T},
  {"TBP", EXTRACTOR_METATYPE_BEATS_PER_MINUTE, T},
  {"TCR", EXTRACTOR_METATYPE_COPYRIGHT, T},
  {"TPB", EXTRACTOR_METATYPE_PUBLISHER, T},
  {"TEN", EXTRACTOR_METATYPE_ENCODED_BY, T},
  {"TSS", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE, T},
  {"TOF", EXTRACTOR_METATYPE_FILENAME, T},
  {"TLE", EXTRACTOR_METATYPE_DURATION, T}, /* FIXME: should append 'ms' as unit */
  {"TSI", EXTRACTOR_METATYPE_EMBEDDED_FILE_SIZE, T},
  /* skipping TDY, TKE */
  {"TOT", EXTRACTOR_METATYPE_ORIGINAL_TITLE, T},
  {"TOA", EXTRACTOR_METATYPE_ORIGINAL_ARTIST, T},
  {"TOL", EXTRACTOR_METATYPE_ORIGINAL_WRITER, T},
  {"TOR", EXTRACTOR_METATYPE_ORIGINAL_RELEASE_YEAR, T},
  /* skipping TXX */

  {"WAF", EXTRACTOR_METATYPE_URL, U},
  {"WAR", EXTRACTOR_METATYPE_URL, U},
  {"WAS", EXTRACTOR_METATYPE_URL, U},
  {"WCM", EXTRACTOR_METATYPE_URL, U},
  {"WCP", EXTRACTOR_METATYPE_RIGHTS, U},
  {"WCB", EXTRACTOR_METATYPE_URL, U},
  /* skipping WXX */
  {"IPL", EXTRACTOR_METATYPE_CONTRIBUTOR_NAME, T},
  /* skipping MCI */
  /* skipping ETC */
  /* skipping MLL */
  /* skipping STC */
  {"ULT", EXTRACTOR_METATYPE_LYRICS, UL},
  {"SLT", EXTRACTOR_METATYPE_LYRICS, SL},
  {"COM", EXTRACTOR_METATYPE_COMMENT, L},
  /* skipping RVA */
  /* skipping EQU */
  /* skipping REV */
  {"PIC", EXTRACTOR_METATYPE_PICTURE, I},
  /* skipping GEN */
  /* {"CNT", EXTRACTOR_METATYPE_PLAY_COUNTER, XXX}, */
  /*  {"POP", EXTRACTOR_METATYPE_POPULARITY_METER, XXX}, */
  /* skipping BUF */
  /* skipping CRM */
  /* skipping CRA */
  /* {"LNK", EXTRACTOR_METATYPE_URL, XXX}, */
  {NULL, 0, T},
};


/* mimetype = audio/mpeg */
int 
EXTRACTOR_id3v2_extract (const unsigned char *data,
			 size_t size,
			 EXTRACTOR_MetaDataProcessor proc,
			 void *proc_cls,
			 const char *options)
{
  unsigned int tsize;
  unsigned int pos;
  unsigned int off;
  enum EXTRACTOR_MetaType type;
  const char *mime;

  if ((size < 16) ||
      (data[0] != 0x49) ||
      (data[1] != 0x44) ||
      (data[2] != 0x33) || (data[3] != 0x02) || (data[4] != 0x00))
    return 0;
  /* unsync: (data[5] & 0x80) > 0;  */
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

      if (pos + 7 > tsize)
        return 0;
      csize = (data[pos + 3] << 16) + (data[pos + 4] << 8) + data[pos + 5];
      if ((pos + 7 + csize > tsize) || (csize > tsize) || (csize == 0))
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
	      switch (tmap[i].fmt)
		{
		case T:		  
		  switch (data[pos + 6])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 7],
							       csize - 1, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 7],
							       csize - 1, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 7],
							       csize - 1, "ISO-8859-1");
		      break;
		    }
		  break;
		case U:
		  word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 6],
							   csize, "ISO-8859-1");
		  break;
		case UL:
		  if (csize < 6)
		    return 0; /* malformed */
		  /* find end of description */
		  off = 10;
		  while ( (off < size) &&
			  (off - pos < csize) &&
			  (data[pos + off] == '\0') )
		    off++;
		  if ( (off >= csize) ||
		       (data[pos+off] != '\0') )
		    return 0; /* malformed */
		  off++;
		  switch (data[pos + 6])
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
		  switch (data[pos + 6])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 12],
							       csize - 6, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 12],
							       csize - 6, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 12],
							       csize - 6, "ISO-8859-1");
		      break;
		    }
		  break;
		case L:
		  if (csize < 5)
		    return 0; /* malformed */
		  /* find end of description */
		  switch (data[pos + 6])
		    {
		    case 0x00:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 10],
							       csize - 4, "ISO-8859-1");
		      break;
		    case 0x01:
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 10],
							       csize - 4, "UCS-2");
		      break;
		    default:
		      /* bad encoding byte,
			 try to convert from iso-8859-1 */
		      word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[pos + 10],
							       csize - 4, "ISO-8859-1");
		      break;
		    }
		  break;
		case I:
		  if (csize < 6)
		    return 0; /* malformed */
		  /* find end of description */
		  off = 12;
		  while ( (off < size) &&
			  (off - pos < csize) &&
			  (data[pos + off] == '\0') )
		    off++;
		  if ( (off >= csize) ||
		       (data[pos+off] != '\0') )
		    return 0; /* malformed */
		  off++;
		  switch (data[pos+11])
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
		  if (0 == strncasecmp ("PNG",
					(const char*) &data[pos + 7], 3))
		    mime = "image/png";
		  else if (0 == strncasecmp ("JPG",
					     (const char*) &data[pos + 7], 3))
		    mime = "image/jpeg";
		  else
		    mime = NULL;
		  if (0 == strncasecmp ("-->",
					(const char*) &data[pos + 7], 3))
		    {
		      /* not supported */
		    }
		  else
		    {
		      if (0 != proc (proc_cls,
				     "id3v2",
				     type,
				     EXTRACTOR_METAFORMAT_BINARY,
				     mime,
				     (const char*) &data[pos + off],
				     csize + 6 - off))			
			return 1;
		    }
		  word = NULL;
		  break;
		default:
		  return 0;
		}
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
	      if (word != NULL)
		free (word);
              break;
            }
          i++;
        }
      pos += 6 + csize;
    }
  return 0;
}

/* end of id3v2_extractor.c */
