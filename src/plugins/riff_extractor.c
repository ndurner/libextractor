/*
     This file is part of libextractor.
     Copyright (C) 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.

     This code was based on AVInfo 1.0 alpha 11
     (c) George Shuklin, gs]AT[shounen.ru, 2002-2004
     http://shounen.ru/soft/avinfo/

     and bitcollider 0.6.0
     (PD) 2004 The Bitzi Corporation
     http://bitzi.com/
 */
/**
 * @file plugins/riff_extractor.c
 * @brief plugin to support RIFF files (ms-video)
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <math.h>


/**
 * Read an uint32_t as a little-endian (least
 * significant byte first) integer from 'data'
 *
 * @param data input data
 * @return integer read
 */
static uint32_t
fread_le (const char *data)
{
  unsigned int x;
  uint32_t result = 0;

  for (x = 0; x < 4; x++)
    result |= ((unsigned char) data[x]) << (x * 8);
  return result;
}


/**
 * We implement our own rounding function, because the availability of
 * C99's round(), nearbyint(), rint(), etc. seems to be spotty, whereas
 * floor() is available in math.h on all C compilers.
 *
 * @param num value to round
 * @return rounded-to-nearest value
 */
static double
round_double (double num)
{
  return floor (num + 0.5);
}


/**
 * Pass the given UTF-8 string to the 'proc' callback using
 * the given type.  Uses 'return' if 'proc' returns non-0.
 *
 * @param s 0-terminated UTF8 string value with the meta data
 * @param t libextractor type for the meta data
 */
#define ADD(s,t) do { if (0 != ec->proc (ec->cls, "riff", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return; } while (0)


/**
 * Main entry method for the 'video/x-msvideo' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void 
EXTRACTOR_riff_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  ssize_t xsize;
  void *data;
  char *xdata;
  uint32_t blockLen;
  unsigned int fps;
  unsigned int duration;
  uint64_t pos;
  uint32_t width;
  uint32_t height;
  char codec[5];
  char format[256];
  
  /* read header */
  if (72 > (xsize = ec->read (ec->cls, &data, 72)))
    return;
  xdata = data;
  
  /* check magic values */
  if ( (0 != memcmp (&xdata[0],
		     "RIFF", 4)) || 
       (0 != memcmp (&xdata[8], "AVI ", 4)) ||
       (0 != memcmp (&xdata[12], "LIST", 4)) ||
       (0 != memcmp (&xdata[20], "hdrlavih", 8)) )
    return;
  
  blockLen = fread_le (&xdata[28]);
  
  /* begin of AVI header at 32 */
  fps = (unsigned int) round_double ((double) 1.0e6 / fread_le (&xdata[32]));
  duration = (unsigned int) round_double ((double) fread_le (&xdata[48])
                                          * 1000 / fps);
  width = fread_le (&xdata[64]);
  height = fread_le (&xdata[68]);

  /* pos: begin of video stream header */
  pos = blockLen + 32;

  if (pos !=
      ec->seek (ec->cls, pos, SEEK_SET))
    return; 
  if (32 > ec->read (ec->cls, &data, 32))
    return;
  xdata = data;

  /* check magic */
  if ( (0 != memcmp (xdata, "LIST", 4)) ||
       (0 != memcmp (&xdata[8], "strlstrh", 8)) ||
       (0 != memcmp (&xdata[20], "vids", 4)) )
    return;

  /* pos + 24: video stream header with codec */
  memcpy (codec, &xdata[24], 4);
  codec[4] = '\0';
  snprintf (format,
	    sizeof (format),
	    _("codec: %s, %u fps, %u ms"), 
	    codec, fps, duration);
  ADD (format, EXTRACTOR_METATYPE_FORMAT);
  snprintf (format, 
	    sizeof (format), 
	    "%ux%u", 
	    (unsigned int) width,
	    (unsigned int) height);
  ADD (format, EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
  ADD ("video/x-msvideo", EXTRACTOR_METATYPE_MIMETYPE);
}

/* end of riff_extractor.c */
