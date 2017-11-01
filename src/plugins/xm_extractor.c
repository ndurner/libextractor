/*
 * This file is part of libextractor.
 * Copyright (C) 2008, 2009 Toni Ruottu
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
 * @brief plugin to support XM files
 * @author Toni Ruottu
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"


/**
 * Header of an XM file.
 */
struct Header
{
  char magicid[17];
  char title[20];
  char something[1];
  char tracker[20];
  char version[2];
};


/**
 * Give meta data to LE.
 *
 * @param s utf-8 string meta data value
 * @param t type of the meta data
 */
#define ADD(s,t) do { if (0 != ec->proc (ec->cls, "xm", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return; } while (0)


/**
 * "extract" metadata from an Extended Module
 *
 * The XM module format description for XM files
 * version $0104 that was written by Mr.H of Triton
 * in 1994 was used, while this piece of software
 * was originally written.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_xm_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *data;
  const struct Header *head;
  char title[21];
  char tracker[21];
  char xmversion[8];
  size_t n;

  if ((ssize_t) sizeof (struct Header) >
      ec->read (ec->cls,
		&data,
		sizeof (struct Header)))
    return;
  head = data;
  /* Check "magic" id bytes */
  if (memcmp (head->magicid, "Extended Module: ", 17))
    return;
  ADD("audio/x-xm", EXTRACTOR_METATYPE_MIMETYPE);
  /* Version of Tracker */
  snprintf (xmversion,
	    sizeof (xmversion),
	    "%d.%d",
	    head->version[1],
	    head->version[0]);
  ADD (xmversion, EXTRACTOR_METATYPE_FORMAT_VERSION);
  /* Song title */
  memcpy (&title, head->title, 20);
  n = 19;
  while ( (n > 0) && isspace ((unsigned char) title[n]))
    n--;
  title[n + 1] = '\0';
  ADD (title, EXTRACTOR_METATYPE_TITLE);
  /* software used for creating the data */
  memcpy (&tracker, head->tracker, 20);
  n = 19;
  while ( (n > 0) && isspace ((unsigned char) tracker[n]))
    n--;
  tracker[n + 1] = '\0';
  ADD (tracker, EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);
  return;
}

/* end of xm_extractor.c */
