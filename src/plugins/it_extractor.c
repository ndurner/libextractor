/*
 * This file is part of libextractor.
 * Copyright (C) 2008 Toni Ruottu
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3, or (at your
 * option) any later version.
 *
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/**
 * @file plugins/xm_extractor.c
 * @brief plugin to support Impulse Tracker (IT) files
 * @author Toni Ruottu
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"


/**
 * Number of bytes in the full IT header and thus
 * the minimum size we're going to accept for an IT file.
 */
#define HEADER_SIZE  0xD0


/**
 * Header of an IT file.
 */
struct Header
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


/**
 * extract meta data from an Impulse Tracker module
 *
 * ITTECH.TXT as taken from IT 2.14p5 was used, while this piece of
 * software was originally written.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_it_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *data;
  char title[27];
  char itversion[8];
  const struct Header *head;

  if ((ssize_t) HEADER_SIZE >
      ec->read (ec->cls,
		&data,
		HEADER_SIZE))
    return;
  head = (struct Header *) data;
  /* Check "magic" id bytes */
  if (memcmp (head->magicid, "IMPM", 4))
    return;
  /* Mime-type */
  if (0 != ec->proc (ec->cls,
		     "it",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "audio/x-mod",
		     strlen ("audio/x-mod") + 1))
    return;

  /* Version of Tracker */
  snprintf (itversion,
	    sizeof (itversion),
	    "%d.%d",
	    (head->version[0] & 0x01),
	    head->version[1]);
  if (0 != ec->proc (ec->cls,
		     "it",
		     EXTRACTOR_METATYPE_FORMAT_VERSION,
		     EXTRACTOR_METAFORMAT_C_STRING,
		     "text/plain",
		     itversion,
		     strlen (itversion) + 1))
    return;

  /* Song title */
  memcpy (&title, head->title, 26);
  title[26] = '\0';
  if (0 != ec->proc (ec->cls,
		     "it",
		     EXTRACTOR_METATYPE_TITLE,
		     EXTRACTOR_METAFORMAT_C_STRING,
		     "text/plain",
		     title,
		     strlen (title) + 1))
    return;
}

/* end of it_extractor.c */
