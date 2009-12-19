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

#define ADD(s,t) do { if (0 != proc (proc_cls, "s3m", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)


/* "extract" keyword from a Scream Tracker 3 Module
 *
 * "Scream Tracker 3.01 BETA File Formats And Mixing Info"
 * was used, while this piece of software was originally
 * written.
 *
 */
int 
EXTRACTOR_s3m_extract (const unsigned char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  char title[29];
  const struct header *head;

  /* Check header size */

  if (size < HEADER_SIZE)    
    return 0;    
  head = (const struct header *) data;
  if (memcmp (head->magicid, "SCRM", 4))
    return 0;
  ADD ("audio/x-s3m", EXTRACTOR_METATYPE_MIMETYPE);

  memcpy (&title, head->title, 28);
  title[28] = '\0';
  ADD (title, EXTRACTOR_METATYPE_TITLE);
  return 0;
}
