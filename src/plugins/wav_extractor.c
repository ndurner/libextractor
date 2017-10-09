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

     This code was based on bitcollider 0.6.0
     (PD) 2004 The Bitzi Corporation
     http://bitzi.com/
     (PD) 2001 The Bitzi Corporation
     Please see file COPYING or http://bitzi.com/publicdomain
     for more info.
*/
/**
 * @file plugins/wav_extractor.c
 * @brief plugin to support WAV files
 * @author Christian Grothoff
 */

#include "platform.h"
#include "extractor.h"


#if BIG_ENDIAN_HOST
static uint16_t
little_endian_to_host16 (uint16_t in)
{
  unsigned char *ptr = (unsigned char *) &in;

  return ((ptr[1] & 0xFF) << 8) | (ptr[0] & 0xFF);
}


static uint32_t
little_endian_to_host32 (uint32_t in)
{
  unsigned char *ptr = (unsigned char *) &in;

  return ((ptr[3] & 0xFF) << 24) | ((ptr[2] & 0xFF) << 16) |
    ((ptr[1] & 0xFF) << 8) | (ptr[0] & 0xFF);
}
#endif


/**
 * Extract information from WAV files.
 *
 * @param ec extraction context
 *
 * @detail
 * A WAV header looks as follows:
 *
 * Offset  Value    meaning
 * 16      4 bytes  0x00000010     // Length of the fmt data (16 bytes)
 * 20      2 bytes  0x0001         // Format tag: 1 = PCM
 * 22      2 bytes  <channels>     // Channels: 1 = mono, 2 = stereo
 * 24      4 bytes  <sample rate>  // Samples per second: e.g., 44100
 */
void
EXTRACTOR_wav_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *data;
  const unsigned char *buf;
  uint16_t channels;
  uint16_t sample_size;
  uint32_t sample_rate;
  uint32_t data_len;
  uint32_t samples;
  char scratch[256];

  if (44 >
      ec->read (ec->cls,  &data, 44))
    return;
  buf = data;
  if ((buf[0] != 'R' || buf[1] != 'I' ||
       buf[2] != 'F' || buf[3] != 'F' ||
       buf[8] != 'W' || buf[9] != 'A' ||
       buf[10] != 'V' || buf[11] != 'E' ||
       buf[12] != 'f' || buf[13] != 'm' || buf[14] != 't' || buf[15] != ' '))
    return;                /* not a WAV file */

  channels = *((uint16_t *) &buf[22]);
  sample_rate = *((uint32_t *) &buf[24]);
  sample_size = *((uint16_t *) &buf[34]);
  data_len = *((uint32_t *) &buf[40]);

#if BIG_ENDIAN_HOST
  channels = little_endian_to_host16 (channels);
  sample_size = little_endian_to_host16 (sample_size);
  sample_rate = little_endian_to_host32 (sample_rate);
  data_len = little_endian_to_host32 (data_len);
#endif

  if ( (8 != sample_size) &&
       (16 != sample_size) )
    return;                /* invalid sample size found in wav file */
  if (0 == channels)
    return;                /* invalid channels value -- avoid division by 0! */
  if (0 == sample_rate)
    return;                /* invalid sample_rate */
  samples = data_len / (channels * (sample_size >> 3));

  snprintf (scratch,
            sizeof (scratch),
            "%u ms, %d Hz, %s",
            (samples < sample_rate)
            ? (samples * 1000 / sample_rate)
            : (samples / sample_rate) * 1000,
            sample_rate, (1 == channels) ? _("mono") : _("stereo"));
  if (0 != ec->proc (ec->cls,
		     "wav",
		     EXTRACTOR_METATYPE_RESOURCE_TYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     scratch,
		     strlen (scratch) + 1))
    return;
  if (0 != ec->proc (ec->cls,
		     "wav",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "audio/x-wav",
		     strlen ("audio/x-wav") +1 ))
    return;
}

/* end of wav_extractor.c */
