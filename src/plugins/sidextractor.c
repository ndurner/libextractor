/*
 * This file is part of libextractor.
 * (C) 2006, 2007 Toni Ruottu
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "platform.h"
#include "extractor.h"
#include "convert.h"


#define SID1_HEADER_SIZE 0x76
#define SID2_HEADER_SIZE 0x7c

/* flags */

#define MUSPLAYER_FLAG   0x01
#define PLAYSID_FLAG     0x02
#define PAL_FLAG         0x04
#define NTSC_FLAG        0x08
#define MOS6581_FLAG     0x10
#define MOS8580_FLAG     0x20

typedef char sidwrd[2];
typedef char sidlongwrd[4];

struct header
{
  char magicid[4];
  sidwrd sidversion;
  sidwrd dataoffset;
  sidwrd loadaddr;
  sidwrd initaddr;
  sidwrd playaddr;
  sidwrd songs;
  sidwrd firstsong;
  sidlongwrd speed;
  char title[32];
  char artist[32];
  char copyright[32];
  sidwrd flags;                 /* version 2 specific fields start */
  char startpage;
  char pagelength;
  sidwrd reserved;
};

int
sidword (sidwrd data)
{
  int value = (unsigned char) data[0] * 0x100 + (unsigned char) data[1];

  return (value);

}

static struct EXTRACTOR_Keywords *addkword
  (EXTRACTOR_KeywordList * oldhead,
   const char *phrase, EXTRACTOR_KeywordType type)
{
  EXTRACTOR_KeywordList *keyword;

  keyword = malloc (sizeof (EXTRACTOR_KeywordList));
  keyword->next = oldhead;
  keyword->keyword = strdup (phrase);
  keyword->keywordType = type;
  return (keyword);
}


/* "extract" keyword from a SID file
 *
 *  This plugin is based on the nsf extractor
 *
 */
struct EXTRACTOR_Keywords *libextractor_sid_extract
  (const char *filename,
   char *data, size_t size, struct EXTRACTOR_Keywords *prev)
{
  unsigned int flags;
  int version;
  char album[33];
  char artist[33];
  char copyright[33];
  char songs[32];
  char startingsong[32];
  char sidversion[32];
  struct header *head;

  /* Check header size */

  if (size < SID1_HEADER_SIZE)
    {
      return (prev);
    }

  head = (struct header *) data;

  /* Check "magic" id bytes */

  if (memcmp (head->magicid, "PSID", 4) && memcmp (head->magicid, "RSID", 4))
    {
      return (prev);
    }


  /* Mime-type */

  prev = addkword (prev, "audio/prs.sid", EXTRACTOR_MIMETYPE);


  /* Version of SID format */

  version = sidword (head->sidversion);
  sprintf (sidversion, "%d", version);
  prev = addkword (prev, sidversion, EXTRACTOR_FORMAT_VERSION);


  /* Get song count */

  sprintf (songs, "%d", sidword (head->songs));
  prev = addkword (prev, songs, EXTRACTOR_SONG_COUNT);


  /* Get number of the first song to be played */

  sprintf (startingsong, "%d", sidword (head->firstsong));
  prev = addkword (prev, startingsong, EXTRACTOR_STARTING_SONG);


  /* name, artist, copyright fields */

  memcpy (&album, head->title, 32);
  memcpy (&artist, head->artist, 32);
  memcpy (&copyright, head->copyright, 32);

  album[32] = '\0';
  artist[32] = '\0';
  copyright[32] = '\0';

  prev = addkword (prev, album, EXTRACTOR_ALBUM);
  prev = addkword (prev, artist, EXTRACTOR_ARTIST);
  prev = addkword (prev, copyright, EXTRACTOR_COPYRIGHT);


  if (version < 2 || size < SID2_HEADER_SIZE)
    {
      return (prev);
    }

  /* Version 2 specific options follow
   *
   * Note: Had some troubles understanding specification
   * on the flags in version 2. I hope this is correct.
   *
   */

  flags = sidword (head->flags);


  /* MUS data */

  if (flags & MUSPLAYER_FLAG)
    {
      prev = addkword (prev, "Compute!'s Sidplayer", EXTRACTOR_DEPENDENCY);
    }

  /* PlaySID data */

  if (flags & PLAYSID_FLAG)
    {
      prev = addkword (prev, "PlaySID", EXTRACTOR_DEPENDENCY);
    }


  /* PAL or NTSC */

  if (flags & PAL_FLAG)
    {
      if (flags & NTSC_FLAG)
        {
          prev = addkword (prev, "PAL/NTSC", EXTRACTOR_TELEVISION_SYSTEM);
        }
      else
        {
          prev = addkword (prev, "PAL", EXTRACTOR_TELEVISION_SYSTEM);
        }
    }
  else
    {
      if (flags & NTSC_FLAG)
        {
          prev = addkword (prev, "NTSC", EXTRACTOR_TELEVISION_SYSTEM);
        }
    }

  /* Detect SID Chips suitable for play the files */

  if (flags & MOS6581_FLAG)
    {
      if (flags & MOS8580_FLAG)
        {
          prev =
            addkword (prev, "MOS6581/MOS8580", EXTRACTOR_HARDWARE_DEPENDENCY);
        }
      else
        {
          prev = addkword (prev, "MOS6581", EXTRACTOR_HARDWARE_DEPENDENCY);
        }
    }
  else
    {
      if (flags & MOS8580_FLAG)
        {
          prev = addkword (prev, "MOS8580", EXTRACTOR_HARDWARE_DEPENDENCY);
        }
    }

  return (prev);
}
