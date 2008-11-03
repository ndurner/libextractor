/*
 * This file is part of libextractor.
 * (C) 2008 Toni Ruottu
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

#define HEADER_SIZE  0xD0

struct header
{
  char magicid[4];
  char title[26];
  char hilight[2];
  char orders[2];
  char instruments[2];
  char samples[2];
  char patterns[2];
  char version[2];
  char compatible[2];
  char flags[2];
  char special[2];
};


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


/* "extract" keyword from an Impulse Tracker module
 *
 * ITTECH.TXT as taken from IT 2.14p5 was used,
 * while this piece of software was originally
 * written.
 *
 */
struct EXTRACTOR_Keywords *libextractor_it_extract
  (const char *filename,
   char *data, size_t size, struct EXTRACTOR_Keywords *prev)
{
  char title[27];
  char itversion[8];
  struct header *head;

  /* Check header size */

  if (size < HEADER_SIZE)
    {
      return (prev);
    }

  head = (struct header *) data;

  /* Check "magic" id bytes */

  if (memcmp (head->magicid, "IMPM", 4))
    {
      return (prev);
    }

  /* Mime-type */

  prev = addkword (prev, "audio/x-it", EXTRACTOR_MIMETYPE);


  /* Version of Tracker */

  sprintf (itversion, "%d.%d", (head->version[0]& 0x01),head->version[1]);
  prev = addkword (prev, itversion, EXTRACTOR_FORMAT_VERSION);

  /* Song title */

  memcpy (&title, head->title, 26);
  title[26] = '\0';
  prev = addkword (prev, title, EXTRACTOR_TITLE);

  return (prev);

}
