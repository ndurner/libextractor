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


     Some of this code is based on AVInfo 1.0 alpha 11
     (c) George Shuklin, gs]AT[shounen.ru, 2002-2004
     http://shounen.ru/soft/avinfo/

 */

#define DEBUG_EXTRACT_MP3 0

#include "platform.h"
#include "extractor.h"
#include "convert.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "extractor_plugins.h"

#if WINDOWS
#include <sys/param.h>          /* #define BYTE_ORDER */
#endif
#ifndef __BYTE_ORDER
#ifdef _BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#else
#ifdef BYTE_ORDER
#define __BYTE_ORDER BYTE_ORDER
#endif
#endif
#endif
#ifndef __BIG_ENDIAN
#ifdef _BIG_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#else
#ifdef BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#endif
#endif
#ifndef __LITTLE_ENDIAN
#ifdef _LITTLE_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#else
#ifdef LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#endif
#endif

#define LARGEST_FRAME_SIZE 8065

enum
{ MPEG_ERR = 0, MPEG_V1 = 1, MPEG_V2 = 2, MPEG_V25 = 3 };

enum
{ LAYER_ERR = 0, LAYER_1 = 1, LAYER_2 = 2, LAYER_3 = 3 };

#define MPA_SYNC_MASK          ((unsigned int) 0xFFE00000)
#if __BYTE_ORDER == __BIG_ENDIAN
#define MPA_SYNC_MASK_MEM      ((unsigned int) 0xFFE00000)
#else
#define MPA_SYNC_MASK_MEM      ((unsigned int) 0x0000E0FF)
#endif
#define MPA_LAST_SYNC_BIT_MASK ((unsigned int) 0x00100000)
#define MPA_VERSION_MASK       ((unsigned int) 0x00080000)
#define MPA_LAYER_MASK         ((unsigned int) 0x3)
#define MPA_LAYER_SHIFT        17
#define MPA_BITRATE_MASK       ((unsigned int) 0xF)
#define MPA_BITRATE_SHIFT      12
#define MPA_FREQ_MASK          ((unsigned int) 0x3)
#define MPA_FREQ_SHIFT         10
#define MPA_CHMODE_MASK        ((unsigned int) 0x3)
#define MPA_CHMODE_SHIFT       6
#define MPA_PADDING_SHIFT      9
#define MPA_COPYRIGHT_SHIFT    3
#define MPA_ORIGINAL_SHIFT     2

static const unsigned int bitrate_table[16][6] = {
  {0,   0,   0,   0,   0,   0},
  {32,  32,  32,  32,  8,   8},
  {64,  48,  40,  48,  16,  16},
  {96,  56,  48,  56,  24,  24},
  {128, 64,  56,  64,  32,  32},
  {160, 80,  64,  80,  40,  40},
  {192, 96,  80,  96,  48,  48},
  {224, 112, 96,  112, 56,  56},
  {256, 128, 112, 128, 64,  64},
  {288, 160, 128, 144, 80,  80},
  {320, 192, 160, 160, 96,  96},
  {352, 224, 192, 176, 112, 112},
  {384, 256, 224, 192, 128, 128},
  {416, 320, 256, 224, 144, 144},
  {448, 384, 320, 256, 160, 160},
  {-1, -1, -1, -1, -1, -1}
};
static const int freq_table[4][3] = {
  {44100, 22050, 11025},
  {48000, 24000, 12000},
  {32000, 16000, 8000}
};
static const char * const channel_modes[4] = {
  gettext_noop("stereo"),
  gettext_noop("joint stereo"),
  gettext_noop("dual channel"),
  gettext_noop("mono")
};
static const char * const mpeg_versions[3] = {
  gettext_noop("MPEG-1"),
  gettext_noop("MPEG-2"),
  gettext_noop("MPEG-2.5")
};
static const char * const layer_names[3] = {
  gettext_noop("Layer I"),
  gettext_noop("Layer II"),
  gettext_noop("Layer III")
};


#define OK         0
#define SYSERR     1
#define INVALID_ID3 2

#define ADDR(s,t) do { if (0 != proc (proc_cls, "mp3", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)

struct mp3_state
{
  int state;

  uint32_t header;
  int sample_rate;
  char mpeg_ver;
  char layer;
  char vbr_flag;
  int ch;
  char copyright_flag;
  char original_flag;
  int avg_bps;
  int bitrate;

  int64_t number_of_frames;
  int64_t number_of_valid_frames;
};

enum MP3State
{
  MP3_LOOKING_FOR_FRAME = 0,
  MP3_READING_FRAME = 1,
};

void
EXTRACTOR_mp3_init_state_method (struct EXTRACTOR_PluginList *plugin)
{
  struct mp3_state *state;
  state = plugin->state = malloc (sizeof (struct mp3_state));
  if (state == NULL)
    return;
  state->header = 0;
  state->sample_rate = 0;
  state->number_of_frames = 0;
  state->number_of_valid_frames = 0;
  state->mpeg_ver = 0;
  state->layer = 0;
  state->vbr_flag = 0;
  state->ch = 0;
  state->copyright_flag = 0;
  state->original_flag = 0;
  state->avg_bps = 0;
  state->bitrate = 0;
  state->state = 0;
}

void
EXTRACTOR_mp3_discard_state_method (struct EXTRACTOR_PluginList *plugin)
{
  if (plugin->state != NULL)
  {
    free (plugin->state);
  }
  plugin->state = NULL;
}

static int
calculate_frame_statistics_and_maybe_report_it (struct EXTRACTOR_PluginList *plugin,
    struct mp3_state *state, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int length;
  char format[512];

  if (((double) state->number_of_valid_frames / (double) state->number_of_frames) < 0.8 ||
      state->number_of_valid_frames <= 2)
    /* Unlikely to be an mp3 file */
    return 0;
  ADDR ("audio/mpeg", EXTRACTOR_METATYPE_MIMETYPE);
  state->avg_bps = state->avg_bps / state->number_of_valid_frames;
  if (state->sample_rate > 0)
    length = 1152 * state->number_of_valid_frames / state->sample_rate;
  else if (state->avg_bps > 0 || state->bitrate > 0)
    length = plugin->fsize / (state->avg_bps ? state->avg_bps : state->bitrate ? state->bitrate : 1) / 125;
  else
    length = 0;

  ADDR (mpeg_versions[state->mpeg_ver - 1], EXTRACTOR_METATYPE_FORMAT_VERSION);
  snprintf (format,
	    sizeof (format),
	    "%s %s audio, %d kbps (%s), %d Hz, %s, %s, %s",
            mpeg_versions[state->mpeg_ver - 1],
            layer_names[state->layer - 1],
            state->avg_bps,
            state->vbr_flag ? _("VBR") : _("CBR"),
            state->sample_rate,
            channel_modes[state->ch],
            state->copyright_flag ? _("copyright") : _("no copyright"),
            state->original_flag ? _("original") : _("copy") );

  ADDR (format, EXTRACTOR_METATYPE_RESOURCE_TYPE);
  snprintf (format,
	    sizeof (format), "%dm%02d",
            length / 60, length % 60);
  ADDR (format, EXTRACTOR_METATYPE_DURATION);
  return 0;
}

int
EXTRACTOR_mp3_extract_method (struct EXTRACTOR_PluginList *plugin,
                       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls)
{
  int64_t file_position;
  int64_t file_size;
  size_t offset = 0;
  size_t size;
  unsigned char *data;
  struct mp3_state *state;

  size_t frames_found_in_this_round = 0;
  int start_anew = 0;

  char mpeg_ver = 0;
  char layer = 0;
  int idx_num = 0;
  int bitrate = 0;              /*used for each frame */
  int copyright_flag = 0;
  int original_flag = 0;
  int sample_rate = 0;
  int ch = 0;
  int frame_size;

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
    case MP3_LOOKING_FOR_FRAME:
      /* Look for a frame header */
      while (offset + sizeof (state->header) < size && (((*((uint32_t *) &data[offset])) & MPA_SYNC_MASK_MEM) != MPA_SYNC_MASK_MEM))
        offset += 1;
      if (offset + sizeof (state->header) >= size)
      {
        /* Alternative: (frames_found_in_this_round < (size / LARGEST_FRAME_SIZE / 2)) is to generous */
        if ((file_position == 0 && (state->number_of_valid_frames > 2) && ((double) state->number_of_valid_frames / (double) state->number_of_frames) < 0.8) ||
            file_position + offset + sizeof (state->header) >= file_size)
        {
          calculate_frame_statistics_and_maybe_report_it (plugin, state, proc, proc_cls);
          return 1;
        }
        plugin->seek_request = file_position + offset;
        return 0;
      }
      state->header = (data[offset] << 24) | (data[offset + 1] << 16) |
               (data[offset + 2] << 8) | data[offset + 3];
      if ((state->header & MPA_SYNC_MASK) == MPA_SYNC_MASK)
      {
        state->state = MP3_READING_FRAME;
        break;
      }
      break;
    case MP3_READING_FRAME:
      state->number_of_frames += 1;
      start_anew = 0;
      switch (state->header & (MPA_LAST_SYNC_BIT_MASK | MPA_VERSION_MASK))
      {
      case (MPA_LAST_SYNC_BIT_MASK | MPA_VERSION_MASK):
        mpeg_ver = MPEG_V1;
        break;
      case (MPA_LAST_SYNC_BIT_MASK):
        mpeg_ver = MPEG_V2;
        break;
      case 0:
        mpeg_ver = MPEG_V25;
        break;
      case (MPA_VERSION_MASK):
      default:
        state->state = MP3_LOOKING_FOR_FRAME;
        offset += 1;
        start_anew = 1;
      }
      if (start_anew)
        break;
      switch (state->header & (MPA_LAYER_MASK << MPA_LAYER_SHIFT))
      {
      case (0x1 << MPA_LAYER_SHIFT):
        layer = LAYER_3;
        break;
      case (0x2 << MPA_LAYER_SHIFT):
        layer = LAYER_2;
        break;
      case (0x3 << MPA_LAYER_SHIFT):
        layer = LAYER_1;
        break;
      case 0x0:
      default:
        state->state = MP3_LOOKING_FOR_FRAME;
        offset += 1;
        start_anew = 1;
      }
      if (start_anew)
        break;
      if (mpeg_ver < MPEG_V25)
        idx_num = (mpeg_ver - 1) * 3 + layer - 1;
      else
        idx_num = 2 + layer;
      bitrate = 1000 * bitrate_table[(state->header >> MPA_BITRATE_SHIFT) &
                                     MPA_BITRATE_MASK][idx_num];
      if (bitrate < 0)
      {
        /*error in header */
        state->state = MP3_LOOKING_FOR_FRAME;
        offset += 1;
        break;
      }
      sample_rate = freq_table[(state->header >> MPA_FREQ_SHIFT) &
                               MPA_FREQ_MASK][mpeg_ver - 1];
      if (sample_rate <= 0)
      {
        /*error in header */
        state->state = MP3_LOOKING_FOR_FRAME;
        offset += 1;
        break;
      }
      ch = ((state->header >> MPA_CHMODE_SHIFT) & MPA_CHMODE_MASK);
      copyright_flag = (state->header >> MPA_COPYRIGHT_SHIFT) & 0x1;
      original_flag = (state->header >> MPA_ORIGINAL_SHIFT) & 0x1;
      if (layer == LAYER_1)
        frame_size = (12 * bitrate / sample_rate + ((state->header >> MPA_PADDING_SHIFT) & 0x1)) * 4;
      else
        frame_size = 144 * bitrate / sample_rate + ((state->header >> MPA_PADDING_SHIFT) & 0x1);
      if (frame_size < 8)
      {
        /*error in header */
        state->state = MP3_LOOKING_FOR_FRAME;
        offset += 1;
        break;
      }

      /* Only save data from valid frames in the state */
      state->avg_bps += bitrate / 1000;
      state->sample_rate = sample_rate;
      state->mpeg_ver = mpeg_ver;
      state->layer = layer;
      state->ch = ch;
      state->copyright_flag = copyright_flag;
      state->original_flag = original_flag;
      state->bitrate = bitrate;

      frames_found_in_this_round += 1;
      state->number_of_valid_frames += 1;
      if (state->avg_bps / state->number_of_valid_frames != bitrate / 1000)
        state->vbr_flag = 1;
      offset += frame_size;
      state->state = MP3_LOOKING_FOR_FRAME;
      break;
    }
  }
  return 1;
}

/* end of mp3_extractor.c */
