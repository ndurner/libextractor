/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2009 Vidyut Samanta and Christian Grothoff

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
 */

#include "platform.h"
#include "extractor.h"

#include "extractor_plugins.h"
#include "le_architecture.h"

/* Based upon ST 3.20 spec at http://16-bits.org/s3m/ */
/* Looks like the format was defined by the software implementation,
 * and that implementation was for little-endian platform, which means
 * that the format is little-endian.
 */

LE_NETWORK_STRUCT_BEGIN
struct S3MHeader
{
  char song_name[28];
  uint8_t byte_1A;
  uint8_t file_type; /* 0x10 == ST3 module */
  uint8_t unknown1[2];
  uint16_t number_of_orders; /* should be even */
  uint16_t number_of_instruments;
  uint16_t number_of_patterns;
  uint16_t flags;
  uint16_t created_with_version;
  uint16_t file_format_info;
  char SCRM[4];
  uint8_t global_volume;
  uint8_t initial_speed;
  uint8_t initial_tempo;
  uint8_t master_volume;
  uint8_t ultra_click_removal;
  uint8_t default_channel_positions;
  uint8_t unknown2[8];
  uint16_t special;
  uint8_t channel_settings[32];
};
LE_NETWORK_STRUCT_END

#define ADD(s,t) if (0 != proc (proc_cls, "s3m", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s) + 1)) return 1
#define ADDL(s,t,l) if (0 != proc (proc_cls, "s3m", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, l)) return 1

int
EXTRACTOR_s3m_extract_method (struct EXTRACTOR_PluginList *plugin,
    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int64_t offset;
  unsigned char *data;
  struct S3MHeader header;
  char song_name_NT[29];

  if (plugin == NULL)
    return 1;
  if (sizeof (header) != pl_read (plugin, &data, sizeof (header)))
    return 1;
  memcpy (&header, data, sizeof (header));
  if (header.byte_1A != 0x1A || memcmp (header.SCRM, "SCRM", 4) != 0)
    return 1;
  header.number_of_orders = LE_le16toh (header.number_of_orders);
  header.number_of_instruments = LE_le16toh (header.number_of_instruments);
  header.number_of_patterns = LE_le16toh (header.number_of_patterns);
  header.flags = LE_le16toh (header.flags);
  header.created_with_version = LE_le16toh (header.created_with_version);
  header.file_format_info = LE_le16toh (header.file_format_info);
  header.special = LE_le16toh (header.special);
  memcpy (song_name_NT, header.song_name, 28);
  song_name_NT[28] = '\0';

  ADD("audio/x-s3m", EXTRACTOR_METATYPE_MIMETYPE);
  ADD(song_name_NT, EXTRACTOR_METATYPE_TITLE);
  /* TODO: turn other header data into useful metadata (i.e. RESOURCE_TYPE).
   * Also, disabled instruments can be (and are) used to carry user-defined text.
   */
  return 1;
}
