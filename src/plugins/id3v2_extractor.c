/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2006, 2009 Vidyut Samanta and Christian Grothoff

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
#ifndef MINGW
#include <sys/mman.h>
#endif
#include "convert.h"

#include "extractor_plugins.h"

#define DEBUG_EXTRACT_ID3v2 0

enum Id3v2Fmt
  {
    T, /* simple, 0-terminated string, prefixed by encoding */
    U, /* 0-terminated ASCII string, no encoding */
    UL, /* unsync'ed lyrics */
    SL, /* sync'ed lyrics */
    L, /* string with language prefix */
    I /* image */
  };

typedef struct
{
  const char *text;
  enum EXTRACTOR_MetaType type;
  enum Id3v2Fmt fmt;
} Matches;

static Matches tmap[] = {
  /* skipping UFI */
  {"TT1 ", EXTRACTOR_METATYPE_SECTION, T},
  {"TT2 ", EXTRACTOR_METATYPE_TITLE, T},
  {"TT3 ", EXTRACTOR_METATYPE_SONG_VERSION, T},
  {"TP1 ", EXTRACTOR_METATYPE_ARTIST, T},
  {"TP2 ", EXTRACTOR_METATYPE_PERFORMER, T},
  {"TP3 ", EXTRACTOR_METATYPE_CONDUCTOR, T},
  {"TP4 ", EXTRACTOR_METATYPE_INTERPRETATION, T},
  {"TCM ", EXTRACTOR_METATYPE_COMPOSER, T},
  {"TXT ", EXTRACTOR_METATYPE_WRITER, T},
  {"TLA ", EXTRACTOR_METATYPE_LANGUAGE, T},
  {"TCO ", EXTRACTOR_METATYPE_GENRE, T},
  {"TAL ", EXTRACTOR_METATYPE_ALBUM, T},
  {"TPA ", EXTRACTOR_METATYPE_DISC_NUMBER, T},
  {"TRK ", EXTRACTOR_METATYPE_TRACK_NUMBER, T},
  {"TRC ", EXTRACTOR_METATYPE_ISRC, T},
  {"TYE ", EXTRACTOR_METATYPE_PUBLICATION_YEAR, T},
  /*
    FIXME: these two and TYE should be combined into
    the actual publication date (if TRD is missing)
  {"TDA ", EXTRACTOR_METATYPE_PUBLICATION_DATE},
  {"TIM ", EXTRACTOR_METATYPE_PUBLICATION_DATE},
  */
  {"TRD ", EXTRACTOR_METATYPE_CREATION_TIME, T},
  {"TMT ", EXTRACTOR_METATYPE_SOURCE, T},
  {"TFT ", EXTRACTOR_METATYPE_FORMAT_VERSION, T},
  {"TBP ", EXTRACTOR_METATYPE_BEATS_PER_MINUTE, T},
  {"TCR ", EXTRACTOR_METATYPE_COPYRIGHT, T},
  {"TPB ", EXTRACTOR_METATYPE_PUBLISHER, T},
  {"TEN ", EXTRACTOR_METATYPE_ENCODED_BY, T},
  {"TSS ", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE, T},
  {"TOF ", EXTRACTOR_METATYPE_FILENAME, T},
  {"TLE ", EXTRACTOR_METATYPE_DURATION, T}, /* FIXME: should append 'ms' as unit */
  {"TSI ", EXTRACTOR_METATYPE_EMBEDDED_FILE_SIZE, T},
  /* skipping TDY, TKE */
  {"TOT ", EXTRACTOR_METATYPE_ORIGINAL_TITLE, T},
  {"TOA ", EXTRACTOR_METATYPE_ORIGINAL_ARTIST, T},
  {"TOL ", EXTRACTOR_METATYPE_ORIGINAL_WRITER, T},
  {"TOR ", EXTRACTOR_METATYPE_ORIGINAL_RELEASE_YEAR, T},
  /* skipping TXX */

  {"WAF ", EXTRACTOR_METATYPE_URL, U},
  {"WAR ", EXTRACTOR_METATYPE_URL, U},
  {"WAS ", EXTRACTOR_METATYPE_URL, U},
  {"WCM ", EXTRACTOR_METATYPE_URL, U},
  {"WCP ", EXTRACTOR_METATYPE_RIGHTS, U},
  {"WCB ", EXTRACTOR_METATYPE_URL, U},
  /* skipping WXX */
  {"IPL ", EXTRACTOR_METATYPE_CONTRIBUTOR_NAME, T},
  /* skipping MCI */
  /* skipping ETC */
  /* skipping MLL */
  /* skipping STC */
  {"ULT ", EXTRACTOR_METATYPE_LYRICS, UL},
  {"SLT ", EXTRACTOR_METATYPE_LYRICS, SL},
  {"COM ", EXTRACTOR_METATYPE_COMMENT, L},
  /* skipping RVA */
  /* skipping EQU */
  /* skipping REV */
  {"PIC ", EXTRACTOR_METATYPE_PICTURE, I},
  /* skipping GEN */
  /* {"CNT ", EXTRACTOR_METATYPE_PLAY_COUNTER, XXX}, */
  /* {"POP ", EXTRACTOR_METATYPE_POPULARITY_METER, XXX}, */
  /* skipping BUF */
  /* skipping CRM */
  /* skipping CRA */
  /* {"LNK ", EXTRACTOR_METATYPE_URL, XXX}, */


  {"TALB", EXTRACTOR_METATYPE_ALBUM, T},
  {"TBPM", EXTRACTOR_METATYPE_BEATS_PER_MINUTE, T},
  {"TCOM", EXTRACTOR_METATYPE_COMPOSER, T},
  {"TCON", EXTRACTOR_METATYPE_SONG_VERSION, T},
  {"TCOP", EXTRACTOR_METATYPE_COPYRIGHT, T},
  {"TDAT", EXTRACTOR_METATYPE_CREATION_DATE, T}, /* idv23 only */
  /* TDLY */
  {"TENC", EXTRACTOR_METATYPE_ENCODED_BY, T},
  {"TEXT", EXTRACTOR_METATYPE_WRITER, T},  
  {"TFLT", EXTRACTOR_METATYPE_FORMAT_VERSION, T},
  /* TIME, idv23 only */
  {"TIT1", EXTRACTOR_METATYPE_SECTION, T},
  {"TIT2", EXTRACTOR_METATYPE_TITLE, T},
  {"TIT3", EXTRACTOR_METATYPE_SONG_VERSION, T},
  /* TKEY */
  {"TLAN", EXTRACTOR_METATYPE_LANGUAGE, T},
  {"TLEN", EXTRACTOR_METATYPE_DURATION, T}, /* FIXME: should append 'ms' as unit */
  {"TMED", EXTRACTOR_METATYPE_SOURCE, T}, 
  {"TOAL", EXTRACTOR_METATYPE_ORIGINAL_TITLE, T},
  {"TOFN", EXTRACTOR_METATYPE_ORIGINAL_ARTIST, T},
  {"TOLY", EXTRACTOR_METATYPE_ORIGINAL_WRITER, T},
  {"TOPE", EXTRACTOR_METATYPE_ORIGINAL_PERFORMER, T},
  {"TORY", EXTRACTOR_METATYPE_ORIGINAL_RELEASE_YEAR, T}, /* idv23 only */
  {"TOWN", EXTRACTOR_METATYPE_LICENSEE, T},
  {"TPE1", EXTRACTOR_METATYPE_ARTIST, T},
  {"TPE2", EXTRACTOR_METATYPE_PERFORMER, T},
  {"TPE3", EXTRACTOR_METATYPE_CONDUCTOR, T},
  {"TPE4", EXTRACTOR_METATYPE_INTERPRETATION, T}, 
  {"TPOS", EXTRACTOR_METATYPE_DISC_NUMBER, T},
  {"TPUB", EXTRACTOR_METATYPE_PUBLISHER, T},
  {"TRCK", EXTRACTOR_METATYPE_TRACK_NUMBER, T},
  /* TRDA, idv23 only */
  {"TRSN", EXTRACTOR_METATYPE_NETWORK_NAME, T},
  /* TRSO */
  {"TSIZ", EXTRACTOR_METATYPE_EMBEDDED_FILE_SIZE, T}, /* idv23 only */
  {"TSRC", EXTRACTOR_METATYPE_ISRC, T},
  /* TSSE */
  {"TYER", EXTRACTOR_METATYPE_PUBLICATION_YEAR, T}, /* idv23 only */
  {"WCOM", EXTRACTOR_METATYPE_URL, U},
  {"WCOP", EXTRACTOR_METATYPE_URL, U},
  {"WOAF", EXTRACTOR_METATYPE_URL, U},
  {"WOAS", EXTRACTOR_METATYPE_URL, U},
  {"WORS", EXTRACTOR_METATYPE_URL, U},
  {"WPAY", EXTRACTOR_METATYPE_URL, U},
  {"WPUB", EXTRACTOR_METATYPE_URL, U},
  {"WXXX", EXTRACTOR_METATYPE_URL, T},
  {"IPLS", EXTRACTOR_METATYPE_CONTRIBUTOR_NAME, T}, /* idv23 only */
  /* ... */
  {"USLT", EXTRACTOR_METATYPE_LYRICS, UL },
  {"SYLT", EXTRACTOR_METATYPE_LYRICS, SL },
  {"COMM", EXTRACTOR_METATYPE_COMMENT, L},
  /* ... */
  {"APIC", EXTRACTOR_METATYPE_PICTURE, I},
  /* ... */
  {"LINK", EXTRACTOR_METATYPE_URL, U},
  /* ... */
  {"USER", EXTRACTOR_METATYPE_LICENSE, T},
  /* ... */

  /* new frames in id3v24 */
  /* ASPI, EQU2, RVA2, SEEK, SIGN, TDEN */
  {"TDOR", EXTRACTOR_METATYPE_PUBLICATION_DATE, T},
  /* TDRC, TDRL, TDTG */
  {"TIPL", EXTRACTOR_METATYPE_CONTRIBUTOR_NAME, T},
  {"TMCL", EXTRACTOR_METATYPE_MUSICIAN_CREDITS_LIST, T},
  {"TMOO", EXTRACTOR_METATYPE_MOOD, T},
  {"TPRO", EXTRACTOR_METATYPE_COPYRIGHT, T},
  {"TSOA", EXTRACTOR_METATYPE_ALBUM, T},
  {"TSOP", EXTRACTOR_METATYPE_PERFORMER, T},
  {"TSOT", EXTRACTOR_METATYPE_TITLE, T},
  {"TSST", EXTRACTOR_METATYPE_SUBTITLE, T},

  {NULL, 0, T},
};

struct id3v2_state
{
  int state;
  unsigned int tsize;
  size_t csize;
  char id[4];
  int32_t ti;
  char ver;
  char extended_header;
  uint16_t frame_flags;
  char *mime;
};

enum ID3v2State
{
  ID3V2_INVALID = -1,
  ID3V2_READING_HEADER = 0,
  ID3V2_READING_FRAME_HEADER,
  ID3V23_READING_EXTENDED_HEADER,
  ID3V24_READING_EXTENDED_HEADER,
  ID3V2_READING_FRAME
};

void
EXTRACTOR_id3v2_init_state_method (struct EXTRACTOR_PluginList *plugin)
{
  struct id3v2_state *state;
  state = plugin->state = malloc (sizeof (struct id3v2_state));
  if (state == NULL)
    return;
  memset (state, 0, sizeof (struct id3v2_state));
  state->state = ID3V2_READING_HEADER;
  state->ti = -1;
  state->mime = NULL;
}

void
EXTRACTOR_id3v2_discard_state_method (struct EXTRACTOR_PluginList *plugin)
{
  struct id3v2_state *state = plugin->state;
  if (state != NULL)
  {
    if (state->mime != NULL)
      free (state->mime);
    free (state);
  }
  plugin->state = NULL;
}

static int
find_type (const char *id, size_t len)
{
  int i;
  for (i = 0; tmap[i].text != NULL; i++)
    if (0 == strncmp (tmap[i].text, id, len))
      return i;
  return -1;
}

int
EXTRACTOR_id3v2_extract_method (struct EXTRACTOR_PluginList *plugin,
    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int64_t file_position;
  int64_t file_size;
  int64_t offset = 0;
  int64_t size;
  struct id3v2_state *state;
  unsigned char *data;
  char *word = NULL;
  unsigned int off;
  enum EXTRACTOR_MetaType type;
  unsigned char picture_type;

  if (plugin == NULL || plugin->state == NULL)
    return 1;

  state = plugin->state;
  file_position = plugin->position;
  file_size = plugin->fsize;
  size = plugin->map_size;
  data = plugin->shm_ptr;

  if (plugin->seek_request < 0)
    return 1;
  if (file_position - plugin->seek_request > 0)
  {
    plugin->seek_request = -1;
    return 1;
  }
  if (plugin->seek_request - file_position < size)
    offset = plugin->seek_request - file_position;

  while (1)
  {
    switch (state->state)
    {
    case ID3V2_INVALID:
      plugin->seek_request = -1;
      return 1;
    case ID3V2_READING_HEADER:
      /* TODO: support id3v24 tags at the end of file. Here's a quote from id3 faq:
       * Q: Where is an ID3v2 tag located in an MP3 file?
       * A: It is most likely located at the beginning of the file. Look for the
       *    marker "ID3" in the first 3 bytes of the file. If it's not there, it
       *    could be at the end of the file (if the tag is ID3v2.4). Look for the
       *    marker "3DI" 10 bytes from the end of the file, or 10 bytes before the
       *    beginning of an ID3v1 tag. Finally it is possible to embed ID3v2 tags
       *    in the actual MPEG stream, on an MPEG frame boundry. Almost nobody does
       *    this.
       * Parsing of such tags will not be completely correct, because we can't
       * seek backwards. We will have to seek to file_size - chunk_size instead
       * (by the way, chunk size is theoretically unknown, LE is free to use any chunk
       * size, even though plugins often make assumptions about chunk size being large
       * enough to make one atomic read without seeking, if offset == 0) and search
       * for id3v1 at -128 offset, then look if there's a 3DI marker 10 bytes before
       *  it (or 10 bytes before the end of file, if id3v1 is not there; not sure
       * about APETAGs; we should probably just scan byte-by-byte from the end of file,
       * until we hit 3DI, or reach the offset == 0), and use it set offset to the
       * start of ID3v24 header, adjust the following file_position check and data
       * indices (use offset), and otherwise proceed as normal (maybe file size checks
       * along the way will have to be adjusted by -1, or made ">" instead of ">=";
       * these problems do not arise for tags at the beginning of the file, since
       * audio itself is usually at least 1-byte long; when the tag is at the end of
       * file, these checks will have to be 100% correct).
       * If there are two tags (at the beginning and at the end of the file),
       * a SEEK in the one at the beginning of the file can be used to seek to the
       * one at the end.
       */
      /* TODO: merge id3v1 and id3v2 parsers. There's an "update" flag in id3v2 that
       * tells the parser to augument id3v1 values with the values from id3v2 (if this
       * flag is not set, id3v2 parser must discard id3v1 data).
       * At the moment id3v1 and id3v2 are parsed separately, and update flag is ignored.
       */
      if (file_position != 0 || size < 10 || (data[0] != 0x49) || (data[1] != 0x44) || (data[2] != 0x33) || ((data[3] != 0x02) && (data[3] != 0x03) && (data[3] != 0x04))/* || (data[4] != 0x00) minor verisons are backward-compatible*/)
      {
        state->state = ID3V2_INVALID;
        break;
      }
      state->ver = data[3];
      if (state->ver == 0x02)
      {
        state->extended_header = 0;
      }
      else if ((state->ver == 0x03) || (state->ver == 0x04))
      {
        if ((data[5] & 0x80) > 0)
        {
          /* unsync is not supported in id3v23 or id3v24*/
          state->state = ID3V2_INVALID;
          break;
        }
        state->extended_header = (data[5] & 0x40) > 0;
        if ((data[5] & 0x20) > 0)
        {
          /* experimental is not supported in id3v23 or id3v24*/
          state->state = ID3V2_INVALID;
          break;
        }
      }
      state->tsize = (((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) | ((data[8] & 0x7F) << 07) | ((data[9] & 0x7F) << 00));
      if (state->tsize + 10 > file_size)
      {
        state->state = ID3V2_INVALID;
        break;
      }
      offset = 10;
      if (state->ver == 0x03 && state->extended_header)
        state->state = ID3V23_READING_EXTENDED_HEADER;
      else if (state->ver == 0x04 && state->extended_header)
        state->state = ID3V24_READING_EXTENDED_HEADER;
      else
        state->state = ID3V2_READING_FRAME_HEADER;
      break;
    case ID3V23_READING_EXTENDED_HEADER:
      if (offset + 9 >= size)
      { 
        if (offset == 0)
        {
          state->state = ID3V2_INVALID;
          break;
        }
        plugin->seek_request = file_position + offset;
        return 0;
      }
      if (state->ver == 0x03 && state->extended_header)
      {
        uint32_t padding, extended_header_size;
        extended_header_size = (((data[offset]) << 24) | ((data[offset + 1]) << 16) | ((data[offset + 2]) << 8) | ((data[offset + 3]) << 0));
        padding = (((data[offset + 6]) << 24) | ((data[offset + 7]) << 16) | ((data[offset + 8]) << 8) | ((data[offset + 9]) << 0));
        if (data[offset + 4] == 0 && data[offset + 5] == 0)
          /* Skip the CRC32 byte after extended header */
          offset += 1;
        offset += 4 + extended_header_size;
        if (padding < state->tsize)
          state->tsize -= padding;
        else
        {
          state->state = ID3V2_INVALID;
          break;
        }
      }
      break;
    case ID3V24_READING_EXTENDED_HEADER:
      if (offset + 6 >= size)
      { 
        if (offset == 0)
        {
          state->state = ID3V2_INVALID;
          break;
        }
        plugin->seek_request = file_position + offset;
        return 0;
      }
      if ( (state->ver == 0x04) && (state->extended_header))
      {
	uint32_t extended_header_size;

        extended_header_size = (((data[offset]) << 24) | 
				((data[offset + 1]) << 16) | 
				((data[offset + 2]) << 8) | 
				((data[offset + 3]) << 0));
        offset += 4 + extended_header_size;
      }
      break;
    case ID3V2_READING_FRAME_HEADER:
      if (file_position + offset > state->tsize ||
          ((state->ver == 0x02) && file_position + offset + 6 >= state->tsize) ||
          (((state->ver == 0x03) || (state->ver == 0x04))&& file_position + offset + 10 >= state->tsize))
      {
        state->state = ID3V2_INVALID;
        break;
      }
      if (((state->ver == 0x02) && (offset + 6 >= size)) ||
          (((state->ver == 0x03) || (state->ver == 0x04)) && (offset + 10 >= size)))
      {
        plugin->seek_request = file_position + offset;
        return 0;
      }
      if (state->ver == 0x02)
      {
        memcpy (state->id, &data[offset], 3);
        state->csize = (data[offset + 3] << 16) + (data[offset + 4] << 8) + data[offset + 5];
        if ((file_position + offset + 6 + state->csize > file_size) || (state->csize > file_size) || (state->csize == 0))
        {
          state->state = ID3V2_INVALID;
          break;
        }
        offset += 6;
        state->frame_flags = 0;
      }
      else if ((state->ver == 0x03) || (state->ver == 0x04))
      {
        memcpy (state->id, &data[offset], 4);
        if (state->ver == 0x03)
          state->csize = (data[offset + 4] << 24) + (data[offset + 5] << 16) + (data[offset + 6] << 8) + data[offset + 7];
        else if (state->ver == 0x04)
          state->csize = ((data[offset + 4] & 0x7F) << 21) | ((data[offset + 5] & 0x7F) << 14) | ((data[offset + 6] & 0x7F) << 07) | ((data[offset + 7] & 0x7F) << 00);
        if ((file_position + offset + 10 + state->csize > file_size) || (state->csize > file_size) || (state->csize == 0))
        {
          state->state = ID3V2_INVALID;
          break;
        }
        state->frame_flags = (data[offset + 8] << 8) + data[offset + 9];
        if (state->ver == 0x03)
        {
          if (((state->frame_flags & 0x80) > 0) /* compressed, not yet supported */ ||
              ((state->frame_flags & 0x40) > 0) /* encrypted, not supported */)
          {
            /* Skip to next frame header */
            offset += 10 + state->csize;
            break;
          }
        }
        else if (state->ver == 0x04)
        {
          if (((state->frame_flags & 0x08) > 0) /* compressed, not yet supported */ ||
              ((state->frame_flags & 0x04) > 0) /* encrypted, not supported */ ||
              ((state->frame_flags & 0x02) > 0) /* unsynchronization, not supported */)
          {
            /* Skip to next frame header */
            offset += 10 + state->csize;
            break;
          }
          if ((state->frame_flags & 0x01) > 0)
          {
            /* Skip data length indicator */
            state->csize -= 4;
            offset += 4;
          }
        }
        offset += 10;
      }

      state->ti = find_type ((const char *) state->id, (state->ver == 0x02) ? 3 : (((state->ver == 0x03) || (state->ver == 0x04)) ? 4 : 0));
      if (state->ti == -1)
      {
        offset += state->csize;
        break;
      }
      state->state = ID3V2_READING_FRAME;
      break;
    case ID3V2_READING_FRAME:
      if (offset == 0 && state->csize > size)
      {
        /* frame size is larger than the size of one data chunk we get at a time */
        offset += state->csize;
        state->state = ID3V2_READING_FRAME_HEADER;
        break;
      }
      if (offset + state->csize > size)
      {
        plugin->seek_request = file_position + offset;
        return 0;
      }
      word = NULL;
      if (((state->ver == 0x03) && ((state->frame_flags & 0x20) > 0)) ||
          ((state->ver == 0x04) && ((state->frame_flags & 0x40) > 0)))
      {
        /* "group" identifier, skip a byte */
        offset++;
        state->csize--;
      }
      switch (tmap[state->ti].fmt)
      {
      case T:
        if (data[offset] == 0x00)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 1],
              state->csize - 1, "ISO-8859-1");
        else if (data[offset] == 0x01)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 1],
              state->csize - 1, "UCS-2");
        else if ((state->ver == 0x04) && (data[offset] == 0x02))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 1],
              state->csize - 1, "UTF-16BE");
        else if ((state->ver == 0x04) && (data[offset] == 0x03))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 1],
              state->csize - 1, "UTF-8");
        else
          /* bad encoding byte, try to convert from iso-8859-1 */
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 1],
              state->csize - 1, "ISO-8859-1");
        break;
      case U:
        word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset],
            state->csize, "ISO-8859-1");
        break;
      case UL:
        if (state->csize < 6)
        {
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        /* find end of description */
        off = 4;
        while ((off < size) && (off < offset + state->csize) && (data[offset + off] != '\0'))
          off++;
        if ((off >= state->csize) || (data[offset + off] != '\0'))
        {
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        off++;
        if (data[offset] == 0x00)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "ISO-8859-1");
        else if (data[offset] == 0x01)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "UCS-2");
        else if ((state->ver == 0x04) && (data[offset] == 0x02))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "UTF-16BE");
        else if ((state->ver == 0x04) && (data[offset] == 0x03))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "UTF-8");
        else
          /* bad encoding byte, try to convert from iso-8859-1 */
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "ISO-8859-1");
        break;
      case SL:
        if (state->csize < 7)
        {
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        if (data[offset] == 0x00)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 6],
              state->csize - 6, "ISO-8859-1");
        else if (data[offset] == 0x01)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 6],
              state->csize - 6, "UCS-2");
        else if ((state->ver == 0x04) && (data[offset] == 0x02))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 6],
              state->csize - 6, "UTF-16BE");
        else if ((state->ver == 0x04) && (data[offset] == 0x03))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 6],
              state->csize - 6, "UTF-8");
        else
          /* bad encoding byte, try to convert from iso-8859-1 */
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + 6],
              state->csize - 6, "ISO-8859-1");
        break;
      case L:
        if (state->csize < 5)
        {
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        /* find end of description */
        off = 4;
        while ((off < size) && (off < offset + state->csize) && (data[offset + off] != '\0'))
          off++;
        if ((off >= state->csize) || (data[offset + off] != '\0'))
        {
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        off++;

        if (data[offset] == 0x00)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "ISO-8859-1");
        else if (data[offset] == 0x01)
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "UCS-2");
        else if ((state->ver == 0x04) && (data[offset] == 0x02))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "UTF-1offBE");
        else if ((state->ver == 0x04) && (data[offset] == 0x03))
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "UTF-8");
        else
          /* bad encoding byte, try to convert from iso-8859-1 */
          word = EXTRACTOR_common_convert_to_utf8 ((const char *) &data[offset + off],
              state->csize - off, "ISO-8859-1");
        break;
      case I:
        if ( ( (state->ver == 0x02) && 
	       (state->csize < 7) ) ||
	     ( ( (state->ver == 0x03) || 
		 (state->ver == 0x04)) && (state->csize < 5)) )
        {
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        if (state->mime != NULL)
          free (state->mime);
        state->mime = NULL;
        if (state->ver == 0x02)
        {
          off = 5;
          picture_type = data[offset + 5];
        }
        else if ((state->ver == 0x03) || (state->ver == 0x04))
        {
          off = 1;
          while ((off < size) && (off < offset + state->csize) && (data[offset + off] != '\0') )
            off++;
          if ((off >= state->csize) || (data[offset + off] != '\0'))
          {
            /* malformed */
            state->state = ID3V2_INVALID;
            break;
          }
          state->mime = malloc (off);
          memcpy (state->mime, &data[offset + 1], off - 1);
          state->mime[off - 1] = '\0';
          off += 1;
          picture_type = data[offset];
          off += 1;
        }
        /* find end of description */
        while ((off < size) && (off < offset + state->csize) && (data[offset + off] != '\0'))
          off++;
        if ((off >= state->csize) || (data[offset + off] != '\0'))
        {
          free (state->mime);
          state->mime = NULL;
          /* malformed */
          state->state = ID3V2_INVALID;
          break;
        }
        off++;
        switch (picture_type)
        {
        case 0x03:
        case 0x04:
          type = EXTRACTOR_METATYPE_COVER_PICTURE;
          break;
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
          type = EXTRACTOR_METATYPE_CONTRIBUTOR_PICTURE;
          break;
        case 0x0D:
        case 0x0E:
        case 0x0F:
          type = EXTRACTOR_METATYPE_EVENT_PICTURE;
          break;
        case 0x14:
          type = EXTRACTOR_METATYPE_LOGO;
          type = EXTRACTOR_METATYPE_LOGO;
          break;
        default:
          type = EXTRACTOR_METATYPE_PICTURE;
          break;
        }
        if (state->ver == 0x02)
        {
          if (0 == strncasecmp ("PNG", (const char *) &data[offset + 1], 3))
            state->mime = strdup ("image/png");
          else if (0 == strncasecmp ("JPG", (const char *) &data[offset + 1], 3))
            state->mime = strdup ("image/jpeg");
          else
            state->mime = NULL;
        }
        else if (((state->ver == 0x03) || (state->ver == 0x04)) && (strchr (state->mime, '/') == NULL))
        {
          size_t mime_len = strlen (state->mime);
          char *type_mime = malloc (mime_len + 6 + 1);
          snprintf (type_mime, mime_len + 6 + 1, "image/%s", state->mime);
          free (state->mime);
          state->mime = type_mime;
        }
        if ((state->mime != NULL) && (0 == strcmp (state->mime, "-->")))
        {
          /* not supported */
          free (state->mime);
          state->mime = NULL;
        }
        else
        {
          if (0 != proc (proc_cls, "id3v2", type, EXTRACTOR_METAFORMAT_BINARY, state->mime, (const char*) &data[offset + off], state->csize - off))
          {
            if (state->mime != NULL)
              free (state->mime);
            state->mime = NULL;
            return 1;
          }
          if (state->mime != NULL)
            free (state->mime);
          state->mime = NULL;
        }
        word = NULL;
        break;
      default:
        return 1;
      }
      if ((word != NULL) && (strlen (word) > 0))
      {
        if (0 != proc (proc_cls, "id3v2", tmap[state->ti].type, EXTRACTOR_METAFORMAT_UTF8, "text/plain", word, strlen (word) + 1))
        {
          free (word);
          return 1;
        }
      }
      if (word != NULL)
        free (word);
      offset = offset + state->csize;
      state->state = ID3V2_READING_FRAME_HEADER;
    break;
    }
  }
  return 1;
}

/* end of id3v2_extractor.c */
