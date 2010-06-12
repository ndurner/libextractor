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

/* "extract" keyword from an Impulse Tracker module
 *
 * ITTECH.TXT as taken from IT 2.14p5 was used,
 * while this piece of software was originally
 * written.
 *
 */
int 
EXTRACTOR_it_extract (const char *data,
		      size_t size,
		      EXTRACTOR_MetaDataProcessor proc,
		      void *proc_cls,
		      const char *options)
{
  char title[27];
  char itversion[8];
  struct header *head;

  /* Check header size */
  if (size < HEADER_SIZE)    
    return 0;
  head = (struct header *) data;
  /* Check "magic" id bytes */
  if (memcmp (head->magicid, "IMPM", 4))
    return 0;
  /* Mime-type */
  if (0 != proc (proc_cls,
		 "it",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "audio/x-it",
		 strlen("audio/x-it")+1))
    return 1;

  /* Version of Tracker */
  snprintf (itversion, 
	    sizeof (itversion),
	    "%d.%d", 
	    (head->version[0]& 0x01),head->version[1]);
  if (0 != proc (proc_cls,
		 "it",
		 EXTRACTOR_METATYPE_FORMAT_VERSION,
		 EXTRACTOR_METAFORMAT_C_STRING,
		 "text/plain",
		 itversion,
		 strlen(itversion)+1))
    return 1;

  /* Song title */
  memcpy (&title, head->title, 26);
  title[26] = '\0';
  if (0 != proc (proc_cls,
		 "it",
		 EXTRACTOR_METATYPE_TITLE,
		 EXTRACTOR_METAFORMAT_C_STRING,
		 "text/plain",
		 title,
		 strlen(title)+1))
    return 1;
  return 0;
}
