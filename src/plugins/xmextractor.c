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

#define HEADER_SIZE  64

struct header
{
  char magicid[17];
  char title[20];
  char something[1];
  char tracker[20];
  char version[2];
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


/* "extract" keyword from an Extended Module
 *
 * The XM module format description for XM files
 * version $0104 that was written by Mr.H of Triton
 * in 1994 was used, while this piece of software
 * was originally written.
 *
 */
struct EXTRACTOR_Keywords *libextractor_xm_extract
  (const char *filename,
   char *data, size_t size, struct EXTRACTOR_Keywords *prev)
{
  char title[21];
  char tracker[21];
  char xmversion[8];
  struct header *head;

  /* Check header size */

  if (size < HEADER_SIZE)
    {
      return (prev);
    }

  head = (struct header *) data;

  /* Check "magic" id bytes */

  if (memcmp (head->magicid, "Extended Module: ", 17))
    {
      return (prev);
    }

  /* Mime-type */

  prev = addkword (prev, "audio/x-xm", EXTRACTOR_MIMETYPE);

  /* Version of Tracker */

  sprintf (xmversion, "%d.%d", head->version[1],head->version[0]);
  prev = addkword (prev, xmversion, EXTRACTOR_FORMAT_VERSION);

  /* Song title */

  memcpy (&title, head->title, 20);
  title[21] = '\0';
  prev = addkword (prev, title, EXTRACTOR_TITLE);

  /* software used for creating the data */

  memcpy (&tracker, head->tracker, 20);
  tracker[21] = '\0';
  prev = addkword (prev, tracker, EXTRACTOR_SOFTWARE);

  return (prev);

}
