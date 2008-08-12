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

#define HEADER_SIZE  0x70

struct header
{
  char title[28];
  char something[16];
  char magicid[4];
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


/* "extract" keyword from a Scream Tracker 3 Module
 *
 * "Scream Tracker 3.01 BETA File Formats And Mixing Info"
 * was used, while this piece of software was originally
 * written.
 *
 */
struct EXTRACTOR_Keywords *libextractor_s3m_extract
  (const char *filename,
   char *data, size_t size, struct EXTRACTOR_Keywords *prev)
{
  char title[29];
  struct header *head;

  /* Check header size */

  if (size < HEADER_SIZE)
    {
      return (prev);
    }

  head = (struct header *) data;

  /* Check "magic" id bytes */

  if (memcmp (head->magicid, "SCRM", 4))
    {
      return (prev);
    }

  /* Mime-type */

  prev = addkword (prev, "audio/x-s3m", EXTRACTOR_MIMETYPE);

  /* Song title */

  memcpy (&title, head->title, 28);
  title[29] = '\0';
  prev = addkword (prev, title, EXTRACTOR_TITLE);

  return (prev);

}
