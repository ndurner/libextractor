/*
     This file is part of libextractor.
     (C) 2004, 2009 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.

     This code was based on AVInfo 1.0 alpha 11
     (c) George Shuklin, gs]AT[shounen.ru, 2002-2004
     http://shounen.ru/soft/avinfo/

     and bitcollider 0.6.0
     (PD) 2004 The Bitzi Corporation
     http://bitzi.com/
 */

#include "platform.h"
#include "extractor.h"
#include <math.h>

/**
 * Read the specified number of bytes as a little-endian (least
 * significant byte first) integer.
 */
static unsigned int
fread_le (const char *data)
{
  int x;
  unsigned int result = 0;

  for (x = 0; x < 4; x++)
    result |= ((unsigned char) data[x]) << (x * 8);
  return result;
}

/* We implement our own rounding function, because the availability of
 * C99's round(), nearbyint(), rint(), etc. seems to be spotty, whereas
 * floor() is available in math.h on all C compilers.
 */
static double
round_double (double num)
{
  return floor (num + 0.5);
}

#define ADD(s,t) do { if (0 != (ret = proc (proc_cls, "riff", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto FINISH; } while (0)

/* video/x-msvideo */
int 
EXTRACTOR_riff_extract (const char *xdata,
			size_t xsize,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  unsigned int blockLen;
  unsigned int fps;
  unsigned int duration;
  size_t pos;
  unsigned int width;
  unsigned int height;
  char codec[5];
  char format[256];
  int ret;

  if (xsize < 32)
    return 0;
  if ((memcmp (&xdata[0],
               "RIFF", 4) != 0) || (memcmp (&xdata[8], "AVI ", 4) != 0))
    return 0;
  if (memcmp (&xdata[12], "LIST", 4) != 0)
    return 0;
  if (memcmp (&xdata[20], "hdrlavih", 8) != 0)
    return 0;

  blockLen = fread_le (&xdata[28]);

  /* begin of AVI header at 32 */
  fps = (unsigned int) round_double ((double) 1.0e6 / fread_le (&xdata[32]));
  duration = (unsigned int) round_double ((double) fread_le (&xdata[48])
                                          * 1000 / fps);
  width = fread_le (&xdata[64]);
  height = fread_le (&xdata[68]);
  /* pos: begin of video stream header */
  pos = blockLen + 32;

  if ((pos < blockLen) || (pos + 32 > xsize) || (pos > xsize))
    return 0;
  if (memcmp (&xdata[pos], "LIST", 4) != 0)
    return 0;
  blockLen = fread_le (&xdata[pos + 4]);
  if (memcmp (&xdata[pos + 8], "strlstrh", 8) != 0)
    return 0;
  if (memcmp (&xdata[pos + 20], "vids", 4) != 0)
    return 0;
  ret = 0;
  /* pos + 24: video stream header */
  memcpy (codec, &xdata[pos + 24], 4);
  codec[4] = '\0';
  snprintf (format,
	    sizeof(format),
	    _("codec: %s, %u fps, %u ms"), codec, fps, duration);
  ADD (format, EXTRACTOR_METATYPE_FORMAT);
  snprintf (format, 
	    sizeof(format), 
	    "%ux%u", width, height);
  ADD (format, EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
  ADD ("video/x-msvideo", EXTRACTOR_METATYPE_MIMETYPE);
 FINISH:
  return ret;
}
