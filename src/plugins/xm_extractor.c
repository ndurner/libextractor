/*
 * This file is part of libextractor.
 * (C) 2008, 2009 Toni Ruottu
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

#define ADD(s,t) do { if (0 != proc (proc_cls, "xm", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)


/* "extract" keyword from an Extended Module
 *
 * The XM module format description for XM files
 * version $0104 that was written by Mr.H of Triton
 * in 1994 was used, while this piece of software
 * was originally written.
 *
 */
int 
EXTRACTOR_xm_extract (const unsigned char *data,
		      size_t size,
		      EXTRACTOR_MetaDataProcessor proc,
		      void *proc_cls,
		      const char *options)
{
  char title[21];
  char tracker[21];
  char xmversion[8];
  const struct header *head;

  /* Check header size */
  if (size < HEADER_SIZE)
    return 0;
  head = (const struct header *) data;
  /* Check "magic" id bytes */
  if (memcmp (head->magicid, "Extended Module: ", 17))
    return 0;
  ADD("audio/x-xm", EXTRACTOR_METATYPE_MIMETYPE);
  /* Version of Tracker */
  snprintf (xmversion, 
	    sizeof(xmversion),
	    "%d.%d", 
	    head->version[1],
	    head->version[0]);
  ADD (xmversion, EXTRACTOR_METATYPE_FORMAT_VERSION);
  /* Song title */
  memcpy (&title, head->title, 20);
  title[20] = '\0';
  ADD (title, EXTRACTOR_METATYPE_TITLE);
  /* software used for creating the data */
  memcpy (&tracker, head->tracker, 20);
  tracker[20] = '\0';
  ADD (tracker, EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);
  return 0;
}
