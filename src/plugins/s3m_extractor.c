/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 */
/**
 * @file plugins/s3m_extractor.c
 * @brief plugin to support Scream Tracker (S3M) files
 * @author Toni Ruottu
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include "le_architecture.h"


LE_NETWORK_STRUCT_BEGIN
struct S3MHeader
{
  char song_name[28];
  uint8_t byte_1A;
  uint8_t file_type; /* 0x10 == ST3 module */
  uint8_t unknown1[2];
  uint16_t number_of_orders LE_PACKED; /* should be even */
  uint16_t number_of_instruments LE_PACKED;
  uint16_t number_of_patterns LE_PACKED;
  uint16_t flags LE_PACKED;
  uint16_t created_with_version LE_PACKED;
  uint16_t file_format_info LE_PACKED;
  char SCRM[4];
  uint8_t global_volume;
  uint8_t initial_speed;
  uint8_t initial_tempo;
  uint8_t master_volume;
  uint8_t ultra_click_removal;
  uint8_t default_channel_positions;
  uint8_t unknown2[8];
  uint16_t special LE_PACKED;
  uint8_t channel_settings[32];
};
LE_NETWORK_STRUCT_END


/**
 * Give meta data to LE 'proc' callback using the given LE type and value.
 *
 * @param t LE meta data type
 * @param s meta data to add
 */
#define ADD(s, t) do { if (0 != ec->proc (ec->cls, "s3m", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return; } while (0)


/**
 * Extractor based upon Scream Tracker 3.20 spec at http://16-bits.org/s3m/
 *
 * Looks like the format was defined by the software implementation,
 * and that implementation was for little-endian platform, which means
 * that the format is little-endian.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_s3m_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *data;
  struct S3MHeader header;
  char song_name_NT[29];

  if ((ssize_t) sizeof (header) >
      ec->read (ec->cls,
		&data,
		sizeof (header)))
    return;
  memcpy (&header, data, sizeof (header));
  if ( (0x1A != header.byte_1A) ||
       (0 != memcmp (header.SCRM, "SCRM", 4)) )
    return;
  header.number_of_orders = LE_le16toh (header.number_of_orders);
  header.number_of_instruments = LE_le16toh (header.number_of_instruments);
  header.number_of_patterns = LE_le16toh (header.number_of_patterns);
  header.flags = LE_le16toh (header.flags);
  header.created_with_version = LE_le16toh (header.created_with_version);
  header.file_format_info = LE_le16toh (header.file_format_info);
  header.special = LE_le16toh (header.special);
  memcpy (song_name_NT, header.song_name, 28);
  song_name_NT[28] = '\0';
  ADD ("audio/x-s3m", EXTRACTOR_METATYPE_MIMETYPE);
  ADD (song_name_NT, EXTRACTOR_METATYPE_TITLE);
  /* TODO: turn other header data into useful metadata (i.e. RESOURCE_TYPE).
   * Also, disabled instruments can be (and are) used to carry user-defined text.
   */
}

/* end of s3m_extractor.c */
