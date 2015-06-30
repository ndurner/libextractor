/*
 * Copyright Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * $Id: demux_asf.c,v 1.1 2003/04/03 14:51:25 grothoff Exp $
 *
 * demultiplexer for asf streams
 *
 * based on ffmpeg's
 * ASF compatible encoder and decoder.
 * Copyright (c) 2000, 2001 Gerard Lantau.
 *
 * GUID list from avifile
 * some other ideas from MPlayer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "xine_internal.h"
#include "demux.h"
#include "xineutils.h"
#include "asfheader.h"
#include "xmlparser.h"

/*
#define LOG
*/

#define CODEC_TYPE_AUDIO       0
#define CODEC_TYPE_VIDEO       1
#define CODEC_TYPE_CONTROL     2
#define MAX_NUM_STREAMS       23

#define DEFRAG_BUFSIZE    65536

#define WRAP_THRESHOLD       5*90000

#define PTS_AUDIO 0
#define PTS_VIDEO 1

typedef struct
{
  int num;
  int seq;

  int frag_offset;
  int64_t timestamp;
  int ts_per_kbyte;
  int defrag;

  uint32_t buf_type;
  int stream_id;
  fifo_buffer_t *fifo;

  uint8_t *buffer;
} asf_stream_t;

typedef struct demux_asf_s
{
  demux_plugin_t demux_plugin;

  xine_stream_t *stream;

  fifo_buffer_t *audio_fifo;
  fifo_buffer_t *video_fifo;

  input_plugin_t *input;

  int keyframe_found;

  int seqno;
  uint32_t packet_size;
  uint8_t packet_flags;
  uint32_t data_size;

  asf_stream_t streams[MAX_NUM_STREAMS];
  uint32_t bitrates[MAX_NUM_STREAMS];
  int num_streams;
  int num_audio_streams;
  int num_video_streams;
  int audio_stream;
  int video_stream;
  int audio_stream_id;
  int video_stream_id;
  int control_stream_id;

  uint16_t wavex[1024];
  int wavex_size;

  uint16_t bih[1024];
  int bih_size;

  char title[512];
  char author[512];
  char copyright[512];
  char comment[512];

  uint32_t length, rate;

  /* packet filling */
  int packet_size_left;

  /* frame rate calculations, discontinuity detection */

  int64_t last_pts[2];
  int32_t frame_duration;
  int send_newpts;
  int64_t last_frame_pts;

  /* only for reading */
  uint32_t packet_padsize;
  int nb_frames;
  uint8_t frame_flag;
  uint8_t segtype;
  int frame;

  int status;

  /* byte reordering from audio streams */
  int reorder_h;
  int reorder_w;
  int reorder_b;

  off_t header_size;
  int buf_flag_seek;

  /* first packet position */
  int64_t first_packet_pos;

  int reference_mode;
} demux_asf_t;

typedef struct
{

  demux_class_t demux_class;

  /* class-wide, global variables here */

  xine_t *xine;
  config_values_t *config;
} demux_asf_class_t;


static uint8_t
get_byte (demux_asf_t * this)
{

  uint8_t buf;
  int i;

  i = this->input->read (this->input, &buf, 1);

  /* printf ("%02x ", buf); */

  if (i != 1)
    {
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: end of data\n");
      this->status = DEMUX_FINISHED;
    }

  return buf;
}

static uint16_t
get_le16 (demux_asf_t * this)
{

  uint8_t buf[2];
  int i;

  i = this->input->read (this->input, buf, 2);

  /* printf (" [%02x %02x] ", buf[0], buf[1]); */

  if (i != 2)
    {
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: end of data\n");
      this->status = DEMUX_FINISHED;
    }

  return buf[0] | (buf[1] << 8);
}

static uint32_t
get_le32 (demux_asf_t * this)
{

  uint8_t buf[4];
  int i;

  i = this->input->read (this->input, buf, 4);

  /* printf ("%02x %02x %02x %02x ", buf[0], buf[1], buf[2], buf[3]); */

  if (i != 4)
    {
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: end of data\n");
      this->status = DEMUX_FINISHED;
    }

  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static uint64_t
get_le64 (demux_asf_t * this)
{

  uint8_t buf[8];
  int i;

  i = this->input->read (this->input, buf, 8);

  if (i != 8)
    {
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: end of data\n");
      this->status = DEMUX_FINISHED;
    }

  return (uint64_t) buf[0]
    | ((uint64_t) buf[1] << 8)
    | ((uint64_t) buf[2] << 16)
    | ((uint64_t) buf[3] << 24)
    | ((uint64_t) buf[4] << 32)
    | ((uint64_t) buf[5] << 40)
    | ((uint64_t) buf[6] << 48) | ((uint64_t) buf[7] << 54);
}

static int
get_guid (demux_asf_t * this)
{
  int i;
  GUID g;

  g.v1 = get_le32 (this);
  g.v2 = get_le16 (this);
  g.v3 = get_le16 (this);
  for (i = 0; i < 8; i++)
    {
      g.v4[i] = get_byte (this);
    }

  for (i = 1; i < GUID_END; i++)
    {
      if (!memcmp (&g, &guids[i].guid, sizeof (GUID)))
        {
#ifdef LOG
          printf ("demux_asf: GUID: %s\n", guids[i].name);
#endif
          return i;
        }
    }

  if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
    printf ("demux_asf: unknown GUID: 0x%x, 0x%x, 0x%x, "
            "{ 0x%hx, 0x%hx, 0x%hx, 0x%hx, 0x%hx, 0x%hx, 0x%hx, 0x%hx }\n",
            g.v1, g.v2, g.v3,
            g.v4[0], g.v4[1], g.v4[2], g.v4[3], g.v4[4], g.v4[5], g.v4[6],
            g.v4[7]);
  return GUID_ERROR;
}

static void
get_str16_nolen (demux_asf_t * this, int len, char *buf, int buf_size)
{

  int c;
  char *q;

  q = buf;
  while (len > 0)
    {
      c = get_le16 (this);
      if ((q - buf) < buf_size - 1)
        *q++ = c;
      len -= 2;
    }
  *q = '\0';
}

static void
asf_send_audio_header (demux_asf_t * this, int stream)
{

  buf_element_t *buf;
  xine_waveformatex *wavex = (xine_waveformatex *) this->wavex;

  if (!this->audio_fifo)
    return;

  buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
  memcpy (buf->content, this->wavex, this->wavex_size);

  this->stream->stream_info[XINE_STREAM_INFO_AUDIO_FOURCC] =
    wavex->wFormatTag;

#ifdef LOG
  printf ("demux_asf: wavex header is %d bytes long\n", this->wavex_size);
#endif

  buf->size = this->wavex_size;
  buf->type = this->streams[stream].buf_type;
  buf->decoder_flags = BUF_FLAG_HEADER;
  buf->decoder_info[1] = wavex->nSamplesPerSec;
  buf->decoder_info[2] = wavex->wBitsPerSample;
  buf->decoder_info[3] = wavex->nChannels;

  this->audio_fifo->put (this->audio_fifo, buf);
}

static unsigned long
str2ulong (unsigned char *str)
{
  return (str[0] | (str[1] << 8) | (str[2] << 16) | (str[3] << 24));
}

static void
asf_send_video_header (demux_asf_t * this, int stream)
{

  buf_element_t *buf;
  xine_bmiheader *bih = (xine_bmiheader *) this->bih;

  this->stream->stream_info[XINE_STREAM_INFO_VIDEO_FOURCC] =
    bih->biCompression;

  buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
  buf->decoder_flags = BUF_FLAG_HEADER;
  buf->decoder_info[1] = 3000;  /* FIXME ? */
  memcpy (buf->content, &this->bih, this->bih_size);
  buf->size = this->bih_size;
  buf->type = this->streams[stream].buf_type;

  this->video_fifo->put (this->video_fifo, buf);

}

static int
asf_read_header (demux_asf_t * this)
{

  int guid;
  uint64_t gsize;

  guid = get_guid (this);
  if (guid != GUID_ASF_HEADER)
    {
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: file doesn't start with an asf header\n");
      return 0;
    }
  get_le64 (this);
  get_le32 (this);
  get_byte (this);
  get_byte (this);

  while (this->status != DEMUX_FINISHED)
    {
      guid = get_guid (this);
      gsize = get_le64 (this);

      if (gsize < 24)
        goto fail;

      switch (guid)
        {
        case GUID_ASF_FILE_PROPERTIES:
          {
            uint64_t start_time, end_time;

            guid = get_guid (this);
            get_le64 (this);    /* file size */
            get_le64 (this);    /* file time */
            get_le64 (this);    /* nb_packets */

            end_time = get_le64 (this);

            this->length = get_le64 (this) / 10000;
            if (this->length)
              this->rate =
                this->input->get_length (this->input) / (this->length / 1000);
            else
              this->rate = 0;

            this->stream->stream_info[XINE_STREAM_INFO_BITRATE] =
              this->rate * 8;

            start_time = get_le32 (this);       /* start timestamp in 1/1000 s */

            get_le32 (this);    /* unknown */
            get_le32 (this);    /* min size */
            this->packet_size = get_le32 (this);        /* max size */
            get_le32 (this);    /* max bitrate */
            get_le32 (this);
          }
          break;

        case (GUID_ASF_STREAM_PROPERTIES):
          {
            int type;
            uint32_t total_size, stream_data_size;
            uint16_t stream_id;
            uint64_t pos1, pos2;
            xine_bmiheader *bih = (xine_bmiheader *) this->bih;
            xine_waveformatex *wavex = (xine_waveformatex *) this->wavex;

            pos1 = this->input->get_current_pos (this->input);

            guid = get_guid (this);
            switch (guid)
              {
              case GUID_ASF_AUDIO_MEDIA:
                type = CODEC_TYPE_AUDIO;
                break;

              case GUID_ASF_VIDEO_MEDIA:
                type = CODEC_TYPE_VIDEO;
                break;

              case GUID_ASF_COMMAND_MEDIA:
                type = CODEC_TYPE_CONTROL;
                break;

              default:
                goto fail;
              }

            guid = get_guid (this);
            get_le64 (this);
            total_size = get_le32 (this);
            stream_data_size = get_le32 (this);
            stream_id = get_le16 (this);        /* stream id */
            get_le32 (this);

            if (type == CODEC_TYPE_AUDIO)
              {
                uint8_t buffer[6];

                this->input->read (this->input, (uint8_t *) this->wavex,
                                   total_size);
                xine_waveformatex_le2me ((xine_waveformatex *) this->wavex);

                /*
                   printf ("total size: %d bytes\n", total_size);
                 */

                /*
                   this->input->read (this->input, (uint8_t *) &this->wavex[9], this->wavex[8]);
                 */
                if (guid == GUID_ASF_AUDIO_SPREAD)
                  {
                    this->input->read (this->input, buffer, 6);
                    this->reorder_h = buffer[0];
                    this->reorder_w = (buffer[2] << 8) | buffer[1];
                    this->reorder_b = (buffer[4] << 8) | buffer[3];
                    this->reorder_w /= this->reorder_b;
                    if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
                      printf
                        ("demux_asf: audio conceal interleave detected (%d x %d x %d)\n",
                         this->reorder_w, this->reorder_h, this->reorder_b);
                  }
                else
                  {
                    this->reorder_b = this->reorder_h = this->reorder_w = 1;
                  }

                this->wavex_size = total_size;  /* 18 + this->wavex[8]; */

                this->streams[this->num_streams].buf_type =
                  formattag_to_buf_audio (wavex->wFormatTag);
                if (!this->streams[this->num_streams].buf_type)
                  {
                    printf ("demux_asf: unknown audio type 0x%x\n",
                            wavex->wFormatTag);
                    this->streams[this->num_streams].buf_type =
                      BUF_AUDIO_UNKNOWN;
                  }

                this->streams[this->num_streams].fifo = this->audio_fifo;
                this->streams[this->num_streams].stream_id = stream_id;
                this->streams[this->num_streams].frag_offset = 0;
                this->streams[this->num_streams].seq = 0;
                if (this->reorder_h > 1 && this->reorder_w > 1)
                  {
                    if (!this->streams[this->num_streams].buffer)
                      this->streams[this->num_streams].buffer =
                        malloc (DEFRAG_BUFSIZE);
                    this->streams[this->num_streams].defrag = 1;
                  }
                else
                  this->streams[this->num_streams].defrag = 0;

#ifdef LOG
                printf ("demux_asf: found an audio stream id=%d \n",
                        stream_id);
#endif
                this->num_audio_streams++;
              }
            else if (type == CODEC_TYPE_VIDEO)
              {

                uint16_t i;

                get_le32 (this);        /* width */
                get_le32 (this);        /* height */
                get_byte (this);

                i = get_le16 (this);    /* size */
                if (i > 0 && i < sizeof (this->bih))
                  {
                    this->bih_size = i;
                    this->input->read (this->input, (uint8_t *) this->bih,
                                       this->bih_size);
                    xine_bmiheader_le2me ((xine_bmiheader *) this->bih);

                    this->streams[this->num_streams].buf_type =
                      fourcc_to_buf_video (bih->biCompression);
                    if (!this->streams[this->num_streams].buf_type)
                      {
                        printf ("demux_asf: unknown video format %.4s\n",
                                (char *) &bih->biCompression);

                        this->streams[this->num_streams].buf_type =
                          BUF_VIDEO_UNKNOWN;
                      }

                    this->streams[this->num_streams].fifo = this->video_fifo;
                    this->streams[this->num_streams].stream_id = stream_id;
                    this->streams[this->num_streams].frag_offset = 0;
                    this->streams[this->num_streams].defrag = 0;

                  }
                else
                  printf
                    ("demux_asf: invalid bih_size received (%d), v_stream ignored.\n",
                     i);

#ifdef LOG
                printf
                  ("demux_asf: found a video stream id=%d, buf_type=%08x \n",
                   stream_id, this->streams[this->num_streams].buf_type);
#endif
                this->num_video_streams++;
              }
            else if (type == CODEC_TYPE_CONTROL)
              {
                this->streams[this->num_streams].stream_id = stream_id;
                this->control_stream_id = stream_id;

                /* This code does'nt work
                   while (get_byte(this) != 0) {while (get_byte(this) != 0) {}}
                   while (get_byte(this) != 0) {while (get_byte(this) != 0) {}}
                 */
#ifdef LOG
                printf ("demux_asf: found a control stream id=%d \n",
                        stream_id);
#endif
              }

            this->num_streams++;
            pos2 = this->input->get_current_pos (this->input);
            this->input->seek (this->input, gsize - (pos2 - pos1 + 24),
                               SEEK_CUR);
          }
          break;

        case GUID_ASF_DATA:
#ifdef LOG
          printf ("demux_asf: found data\n");
#endif
          goto headers_ok;
          break;
        case GUID_ASF_CONTENT_DESCRIPTION:
          {
            uint16_t len1, len2, len3, len4, len5;

            len1 = get_le16 (this);
            len2 = get_le16 (this);
            len3 = get_le16 (this);
            len4 = get_le16 (this);
            len5 = get_le16 (this);
            get_str16_nolen (this, len1, this->title, sizeof (this->title));
            get_str16_nolen (this, len2, this->author, sizeof (this->author));
            get_str16_nolen (this, len3, this->copyright,
                             sizeof (this->copyright));
            get_str16_nolen (this, len4, this->comment,
                             sizeof (this->comment));
            this->input->seek (this->input, len5, SEEK_CUR);
            /*
               } else if (url_feof(this)) {
               goto fail;
             */
          }
          break;

        case GUID_ASF_STREAM_BITRATE_PROPERTIES:
          {
            uint16_t streams, stream_id;
            uint16_t i;

#ifdef LOG
            printf ("demux_asf: GUID stream group\n");
#endif

            streams = get_le16 (this);
            for (i = 0; i < streams; i++)
              {
                stream_id = get_le16 (this);
                this->bitrates[stream_id] = get_le32 (this);
              }
          }
          break;

        default:
          this->input->seek (this->input, gsize - 24, SEEK_CUR);
        }
    }

headers_ok:
  this->input->seek (this->input, sizeof (GUID) + 10, SEEK_CUR);
  this->packet_size_left = 0;
  this->first_packet_pos = this->input->get_current_pos (this->input);
  return 1;

fail:
  return 0;
}

static void
asf_reorder (demux_asf_t * this, uint8_t * src, int len)
{
  uint8_t *dst = malloc (len);
  uint8_t *s2 = src;
  int i = 0, x, y;

  while (len - i >= this->reorder_h * this->reorder_w * this->reorder_b)
    {
      for (x = 0; x < this->reorder_w; x++)
        for (y = 0; y < this->reorder_h; y++)
          {
            memcpy (dst + i, s2 + (y * this->reorder_w + x) * this->reorder_b,
                    this->reorder_b);
            i += this->reorder_b;
          }
      s2 += this->reorder_h * this->reorder_w * this->reorder_b;
    }

  xine_fast_memcpy (src, dst, i);
  free (dst);
}

static uint32_t
asf_get_packet (demux_asf_t * this)
{

  int64_t timestamp;
  int duration;
  uint8_t ecd_flags;
  uint8_t buf[16];
  uint32_t p_hdr_size;
  int invalid_packet;

  do
    {
      ecd_flags = get_byte (this);
      p_hdr_size = 1;
      invalid_packet = 0;

      /* skip ecd */
      if (ecd_flags & 0x80)
        p_hdr_size += this->input->read (this->input, buf, ecd_flags & 0x0F);

      /* skip invalid packet */
      if (ecd_flags & 0x70)
        {
#ifdef LOG
          printf ("demux_asf: skip invalid packet: %d\n", ecd_flags);
#endif
          this->input->seek (this->input, this->packet_size - p_hdr_size,
                             SEEK_CUR);
          invalid_packet = 1;
        }

      if (this->status != DEMUX_OK)
        return 0;

    }
  while (invalid_packet);

  if (this->status != DEMUX_OK)
    return 0;

  this->packet_flags = get_byte (this);
  p_hdr_size += 1;
  this->segtype = get_byte (this);
  p_hdr_size += 1;

  /* packet size */
  switch ((this->packet_flags >> 5) & 3)
    {
    case 1:
      this->data_size = get_byte (this);
      p_hdr_size += 1;
      break;
    case 2:
      this->data_size = get_le16 (this);
      p_hdr_size += 2;
      break;
    case 3:
      this->data_size = get_le32 (this);
      p_hdr_size += 4;
      break;
    default:
      this->data_size = 0;
    }

  /* sequence */
  switch ((this->packet_flags >> 1) & 3)
    {
    case 1:
      get_byte (this);
      p_hdr_size += 1;
      break;
    case 2:
      get_le16 (this);
      p_hdr_size += 2;
      break;
    case 3:
      get_le32 (this);
      p_hdr_size += 4;
      break;
    }

  /* padding size */
  switch ((this->packet_flags >> 3) & 3)
    {
    case 1:
      this->packet_padsize = get_byte (this);
      p_hdr_size += 1;
      break;
    case 2:
      this->packet_padsize = get_le16 (this);
      p_hdr_size += 2;
      break;
    case 3:
      this->packet_padsize = get_le32 (this);
      p_hdr_size += 4;
      break;
    default:
      this->packet_padsize = 0;
    }

  timestamp = get_le32 (this);
  p_hdr_size += 4;
  duration = get_le16 (this);
  p_hdr_size += 2;

  if ((this->packet_flags >> 5) & 3)
    {
      /* absolute data size */
#ifdef LOG
      printf ("demux_asf: absolute data size\n");
#endif
      this->packet_padsize = this->packet_size - this->data_size;       /* not used */
    }
  else
    {
      /* relative data size */
#ifdef LOG
      printf ("demux_asf: relative data size\n");
#endif
      this->data_size = this->packet_size - this->packet_padsize;
    }

  /* this->packet_size_left = this->packet_size - p_hdr_size; */
  this->packet_size_left = this->data_size - p_hdr_size;
#ifdef LOG
  printf
    ("demux_asf: new packet, size = %d, size_left = %d, flags = 0x%02x, padsize = %d, this->packet_size = %d\n",
     this->data_size, this->packet_size_left, this->packet_flags,
     this->packet_padsize, this->packet_size);
#endif

  return 1;
}

static void
hexdump (unsigned char *data, int len, xine_t * xine)
{
  int i;

  for (i = 0; i < len; i++)
    printf ("%02x ", data[i]);
  printf ("\n");

}

/* redefine abs as macro to handle 64-bit diffs.
   i guess llabs may not be available everywhere */
#define abs(x) ( ((x)<0) ? -(x) : (x) )

static void
check_newpts (demux_asf_t * this, int64_t pts, int video, int frame_end)
{
  int64_t diff;

  diff = pts - this->last_pts[video];

  if (pts
      && (this->send_newpts
          || (this->last_pts[video] && abs (diff) > WRAP_THRESHOLD)))
    {

#ifdef LOG
      printf ("demux_asf: sending newpts %lld (video = %d diff = %lld)\n",
              pts, video, diff);
#endif

      if (this->buf_flag_seek)
        {
          xine_demux_control_newpts (this->stream, pts, BUF_FLAG_SEEK);
          this->buf_flag_seek = 0;
        }
      else
        {
          xine_demux_control_newpts (this->stream, pts, 0);
        }

      this->send_newpts = 0;
      this->last_pts[1 - video] = 0;
    }

  if (pts)
    this->last_pts[video] = pts;

  /*
   * frame rate estimation
   */

  if (pts && video && frame_end)
    {

      if (this->last_frame_pts)
        {

          diff = pts - this->last_frame_pts;

          if ((diff > 0) && (diff < WRAP_THRESHOLD))
            {
#ifdef LOG
              printf ("demux_asf: last_frame_pts = %8lld, diff=%8lld\n",
                      this->last_frame_pts, diff);
#endif
              this->frame_duration = (15 * this->frame_duration + diff) / 16;
#ifdef LOG
              printf ("demux_asf: frame_duration is %d\n",
                      this->frame_duration);
#endif
            }
        }

      this->last_frame_pts = pts;
    }
}


static void
asf_send_buffer_nodefrag (demux_asf_t * this, asf_stream_t * stream,
                          int frag_offset, int seq,
                          int64_t timestamp, int frag_len, int payload_size)
{

  buf_element_t *buf;
  int bufsize;
  int package_done;

  if (stream->frag_offset == 0)
    {
      /* new packet */
      stream->seq = seq;
    }
  else
    {
      if (seq == stream->seq && frag_offset == stream->frag_offset)
        {
          /* continuing packet */
        }
      else
        {
          /* cannot continue current packet: free it */
          stream->frag_offset = 0;
          if (frag_offset != 0)
            {
              /* cannot create new packet */
              this->input->seek (this->input, frag_len, SEEK_CUR);
              return;
            }
          else
            {
              /* create new packet */
              stream->seq = seq;
            }
        }
    }


  while (frag_len)
    {
      if (frag_len < stream->fifo->buffer_pool_buf_size)
        bufsize = frag_len;
      else
        bufsize = stream->fifo->buffer_pool_buf_size;

      buf = stream->fifo->buffer_pool_alloc (stream->fifo);
      this->input->read (this->input, buf->content, bufsize);

      buf->extra_info->input_pos = this->input->get_current_pos (this->input);
      if (this->rate)
        buf->extra_info->input_time =
          (int) ((int64_t) buf->extra_info->input_pos * 1000 / this->rate);
      else
        buf->extra_info->input_time = 0;

#ifdef LOG
      printf ("demux_asf: input pos is %lld, input time is %d\n",
              buf->extra_info->input_pos, buf->extra_info->input_time);
#endif

      buf->pts = timestamp * 90;

      buf->type = stream->buf_type;
      buf->size = bufsize;
      timestamp = 0;

      stream->frag_offset += bufsize;
      frag_len -= bufsize;

      package_done = (stream->frag_offset == payload_size);

      if ((buf->type & BUF_MAJOR_MASK) == BUF_VIDEO_BASE)
        check_newpts (this, buf->pts, PTS_VIDEO, package_done);
      else
        check_newpts (this, buf->pts, PTS_AUDIO, package_done);

      /* test if whole packet read */
      if (package_done)
        {

          if ((buf->type & BUF_MAJOR_MASK) == BUF_VIDEO_BASE)
            {

              buf->decoder_flags = BUF_FLAG_FRAME_END | BUF_FLAG_FRAMERATE;
              buf->decoder_info[0] = this->frame_duration;

            }
          else
            {

              buf->decoder_flags = BUF_FLAG_FRAME_END;
            }

          stream->frag_offset = 0;

        }

      if (!this->keyframe_found)
        buf->decoder_flags |= BUF_FLAG_PREVIEW;

#ifdef LOG
      printf ("demux_asf: buffer type %08x %8d bytes, %8lld pts\n",
              buf->type, buf->size, buf->pts);
#endif
      stream->fifo->put (stream->fifo, buf);
    }
}

static void
asf_send_buffer_defrag (demux_asf_t * this, asf_stream_t * stream,
                        int frag_offset, int seq,
                        int64_t timestamp, int frag_len, int payload_size)
{

  buf_element_t *buf;

  /*
     printf("asf_send_buffer seq=%d frag_offset=%d frag_len=%d\n",
     seq, frag_offset, frag_len );
   */

  if (stream->frag_offset == 0)
    {
      /* new packet */
      stream->seq = seq;
    }
  else
    {
      if (seq == stream->seq && frag_offset == stream->frag_offset)
        {
          /* continuing packet */
        }
      else
        {
          /* cannot continue current packet: free it */
          if (stream->frag_offset)
            {
              int bufsize;
              uint8_t *p;

              if (stream->fifo == this->audio_fifo &&
                  this->reorder_h > 1 && this->reorder_w > 1)
                {
                  asf_reorder (this, stream->buffer, stream->frag_offset);
                }

              p = stream->buffer;
              while (stream->frag_offset)
                {
                  if (stream->frag_offset <
                      stream->fifo->buffer_pool_buf_size)
                    bufsize = stream->frag_offset;
                  else
                    bufsize = stream->fifo->buffer_pool_buf_size;

                  buf = stream->fifo->buffer_pool_alloc (stream->fifo);
                  xine_fast_memcpy (buf->content, p, bufsize);

                  buf->extra_info->input_pos =
                    this->input->get_current_pos (this->input);
                  if (this->rate)
                    buf->extra_info->input_time =
                      (int) ((int64_t) buf->extra_info->input_pos * 1000 /
                             this->rate);
                  else
                    buf->extra_info->input_time = 0;

                  buf->pts = stream->timestamp * 90 + stream->ts_per_kbyte *
                    (p - stream->buffer) / 1024;

                  buf->type = stream->buf_type;
                  buf->size = bufsize;

#ifdef LOG
                  printf
                    ("demux_asf: buffer type %08x %8d bytes, %8lld pts\n",
                     buf->type, buf->size, buf->pts);
#endif

                  stream->frag_offset -= bufsize;
                  p += bufsize;

                  if ((buf->type & BUF_MAJOR_MASK) == BUF_VIDEO_BASE)
                    check_newpts (this, buf->pts, PTS_VIDEO,
                                  !stream->frag_offset);
                  else
                    check_newpts (this, buf->pts, PTS_AUDIO,
                                  !stream->frag_offset);

                  /* test if whole packet read */
                  if (!stream->frag_offset)
                    buf->decoder_flags |= BUF_FLAG_FRAME_END;

                  if (!this->keyframe_found)
                    buf->decoder_flags |= BUF_FLAG_PREVIEW;

                  stream->fifo->put (stream->fifo, buf);
                }
            }

          stream->frag_offset = 0;
          if (frag_offset != 0)
            {
              /* cannot create new packet */
              this->input->seek (this->input, frag_len, SEEK_CUR);
              return;
            }
          else
            {
              /* create new packet */
              stream->seq = seq;
            }
        }
    }


  if (frag_offset)
    {
      if (timestamp)
        stream->ts_per_kbyte =
          (timestamp - stream->timestamp) * 1024 * 90 / frag_offset;
    }
  else
    {
      stream->ts_per_kbyte = 0;
      stream->timestamp = timestamp;
    }

  if (stream->frag_offset + frag_len > DEFRAG_BUFSIZE)
    {
      printf ("demux_asf: buffer overflow on defrag!\n");
    }
  else
    {
      this->input->read (this->input, &stream->buffer[stream->frag_offset],
                         frag_len);
      stream->frag_offset += frag_len;
    }
}

static void
asf_read_packet (demux_asf_t * this)
{

  uint8_t raw_id, stream_id;
  uint32_t seq, frag_offset, payload_size, frag_len, rlen;
  int i;
  int64_t timestamp;
  asf_stream_t *stream;
  uint32_t s_hdr_size = 0;
  uint64_t current_pos;
  uint32_t mod;
  uint32_t psl;

  if ((++this->frame == (this->nb_frames & 0x3f)))
    {
      psl = this->packet_size_left;
      current_pos = this->input->get_current_pos (this->input);
      mod = (current_pos - this->first_packet_pos) % this->packet_size;
      this->packet_size_left = mod ? this->packet_size - mod : 0;

#ifdef LOG
      printf
        ("demux_asf: reading new packet, packet size left psl=%d, pad=%d, %d\n",
         psl, this->packet_padsize, this->packet_size_left);
#endif

      if (this->packet_size_left)
        this->input->seek (this->input, this->packet_size_left, SEEK_CUR);

      if (!asf_get_packet (this))
        {
          if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
            printf ("demux_asf: get_packet failed\n");
          this->status = DEMUX_FINISHED;
          return;
        }

      if (this->packet_padsize > this->packet_size)
        {
          /* skip packet */
          if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
            printf ("demux_asf: invalid padsize: %d\n", this->packet_padsize);
          this->frame = this->nb_frames - 1;
          return;
        }

      /* Multiple frames */
      this->frame = 0;
      if (this->packet_flags & 0x01)
        {
          this->frame_flag = get_byte (this);
          s_hdr_size += 1;
          this->nb_frames = this->frame_flag & 0x3F;
#ifdef LOG
          printf ("demux_asf: multiple frames %d\n", this->nb_frames);
#endif
        }
      else
        {
          this->frame_flag = 0;
          this->nb_frames = 1;
        }
    }

  /* read segment header, find stream */
  raw_id = get_byte (this);
  s_hdr_size += 1;
  stream_id = raw_id & 0x7f;

  stream = NULL;
#ifdef LOG
  printf ("demux_asf: got raw_id =%d keyframe_found=%d\n", raw_id,
          this->keyframe_found);
#endif

  if ((raw_id & 0x80) || this->keyframe_found
      || (this->num_video_streams == 0))
    {
      for (i = 0; i < this->num_streams; i++)
        {
          if (this->streams[i].stream_id == stream_id &&
              (stream_id == this->audio_stream_id
               || stream_id == this->video_stream_id))
            {
              stream = &this->streams[i];
            }
        }

#ifdef LOG
      /* display control stream content */
      if (stream_id == this->control_stream_id)
        {
          printf ("demux_asf: Control Stream : begin\n");
          for (i = 0; i < (this->packet_size_left - s_hdr_size); i++)
            {
              printf ("%c", get_byte (this));
            }
          printf ("\ndemux_asf: Control Stream : end\n");
          return;
        }
#endif
    }

  switch ((this->segtype >> 4) & 3)
    {
    case 1:
      seq = get_byte (this);
      s_hdr_size += 1;
      break;
    case 2:
      seq = get_le16 (this);
      s_hdr_size += 2;
      break;
    case 3:
      seq = get_le32 (this);
      s_hdr_size += 4;
      break;
    default:
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: seq=0\n");
      seq = 0;
    }

  switch ((this->segtype >> 2) & 3)
    {
    case 1:
      frag_offset = get_byte (this);
      s_hdr_size += 1;
      break;
    case 2:
      frag_offset = get_le16 (this);
      s_hdr_size += 2;
      break;
    case 3:
      frag_offset = get_le32 (this);
      s_hdr_size += 4;
      break;
    default:
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: frag_offset=0\n");
      frag_offset = 0;
    }

  /* only set keyframe_found if we have it's beginning */
  if ((raw_id & 0x80) && stream && !frag_offset)
    this->keyframe_found = 1;

  switch (this->segtype & 3)
    {
    case 1:
      rlen = get_byte (this);
      s_hdr_size += 1;
      break;
    case 2:
      rlen = get_le16 (this);
      s_hdr_size += 2;
      break;
    case 3:
      rlen = get_le32 (this);
      s_hdr_size += 4;
      break;
    default:
      rlen = 0;
    }

  if (rlen > this->packet_size_left)
    {
      /* skip packet */
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: invalid rlen %d\n", rlen);
      this->frame = this->nb_frames - 1;
      return;
    }

#ifdef LOG
  printf
    ("demux_asf: segment header, stream id %02x, frag_offset %d, flags : %02x\n",
     stream_id, frag_offset, rlen);
#endif

  if (rlen == 1)
    {
      int data_length, data_sent = 0;

      /* multiple part segment */
      timestamp = frag_offset;
      get_byte (this);
      s_hdr_size += 1;

      if (this->packet_flags & 0x01)
        {
          /* multiple frames */
          switch ((this->frame_flag >> 6) & 3)
            {
            case 1:
              data_length = get_byte (this);
              s_hdr_size += 1;
              break;
            case 2:
              data_length = get_le16 (this);
              s_hdr_size += 2;
              break;
            case 3:
              data_length = get_le32 (this);
              s_hdr_size += 4;
              break;
            default:
#ifdef LOG
              printf ("demux_asf: invalid frame_flag %d\n", this->frame_flag);
#endif
              data_length = get_le16 (this);
              s_hdr_size += 2;
            }

#ifdef LOG
          printf ("demux_asf: reading grouping part segment, size = %d\n",
                  data_length);
#endif

        }
      else
        {
          data_length = this->packet_size_left - s_hdr_size;
#ifdef LOG
          printf ("demux_asf: reading grouping single segment, size = %d\n",
                  data_length);
#endif
        }

      if (data_length > this->packet_size_left)
        {
          /* skip packet */
          if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
            printf ("demux_asf: invalid data_length\n");
          this->frame = this->nb_frames - 1;
          return;
        }

      this->packet_size_left -= s_hdr_size;

      while (data_sent < data_length)
        {
          int object_length = get_byte (this);

#ifdef LOG
          printf ("demux_asf: sending grouped object, len = %d\n",
                  object_length);
#endif


          if (stream && stream->fifo)
            {
#ifdef LOG
              printf ("demux_asf: sending buffer of type %08x\n",
                      stream->buf_type);
#endif

              if (stream->defrag)
                asf_send_buffer_defrag (this, stream, 0, seq, timestamp,
                                        object_length, object_length);
              else
                asf_send_buffer_nodefrag (this, stream, 0, seq, timestamp,
                                          object_length, object_length);
            }
          else
            {
#ifdef LOG
              printf ("demux_asf: unhandled stream type, id %d\n", stream_id);
#endif
              this->input->seek (this->input, object_length, SEEK_CUR);
            }
          seq++;
          data_sent += object_length + 1;
          this->packet_size_left -= object_length + 1;
          timestamp = 0;
        }

    }
  else
    {

      /* single part segment */
      if (rlen >= 8)
        {
          payload_size = get_le32 (this);
          s_hdr_size += 4;
          timestamp = get_le32 (this);
          s_hdr_size += 4;
          this->input->seek (this->input, rlen - 8, SEEK_CUR);
          s_hdr_size += rlen - 8;
        }
      else
        {
          if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
            printf ("demux_asf: strange rlen %d\n", rlen);
          timestamp = 0;
          payload_size = 0;
          this->input->seek (this->input, rlen, SEEK_CUR);
          s_hdr_size += rlen;
        }

      if (this->packet_flags & 0x01)
        {
          switch ((this->frame_flag >> 6) & 3)
            {
            case 1:
              frag_len = get_byte (this);
              s_hdr_size += 1;
              break;
            case 2:
              frag_len = get_le16 (this);
              s_hdr_size += 2;
              break;
            case 3:
              frag_len = get_le32 (this);
              s_hdr_size += 4;
              break;
            default:
#ifdef LOG
              printf ("demux_asf: invalid frame_flag %d\n", this->frame_flag);
#endif
              frag_len = get_le16 (this);
              s_hdr_size += 2;
            }

#ifdef LOG
          printf ("demux_asf: reading part segment, size = %d\n", frag_len);
#endif
        }
      else
        {
          frag_len = this->packet_size_left - s_hdr_size;

#ifdef LOG
          printf ("demux_asf: reading single segment, size = %d\n", frag_len);
#endif
        }

      if (frag_len > this->packet_size_left)
        {
          /* skip packet */
          if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
            printf ("demux_asf: invalid frag_len %d\n", frag_len);
          this->frame = this->nb_frames - 1;
          return;
        }

      if (!payload_size)
        {
          payload_size = frag_len;
        }

      this->packet_size_left -= s_hdr_size;

      if (stream && stream->fifo)
        {

#ifdef LOG
          printf ("demux_asf: sending buffer of type %08x\n",
                  stream->buf_type);
#endif

          if (stream->defrag)
            asf_send_buffer_defrag (this, stream, frag_offset, seq, timestamp,
                                    frag_len, payload_size);
          else
            asf_send_buffer_nodefrag (this, stream, frag_offset, seq,
                                      timestamp, frag_len, payload_size);
        }
      else
        {
#ifdef LOG
          printf ("demux_asf: unhandled stream type, id %d\n", stream_id);
#endif
          this->input->seek (this->input, frag_len, SEEK_CUR);
        }
      this->packet_size_left -= frag_len;
    }
}

/*
 * parse a m$ http reference
 * format :
 * [Reference]
 * Ref1=http://www.blabla.com/blabla
 */
static int
demux_asf_parse_http_references (demux_asf_t * this)
{
  char *buf = NULL;
  char *ptr;
  int buf_size = 0;
  int buf_used = 0;
  int len;
  char *href = NULL;
  xine_mrl_reference_data_t *data;
  xine_event_t uevent;

  /* read file to memory.
   * warning: dumb code, but hopefuly ok since reference file is small */
  do
    {
      buf_size += 1024;
      buf = realloc (buf, buf_size + 1);

      len =
        this->input->read (this->input, &buf[buf_used], buf_size - buf_used);

      if (len > 0)
        buf_used += len;

      /* 50k of reference file? no way. something must be wrong */
      if (buf_used > 50 * 1024)
        break;
    }
  while (len > 0);

  if (buf_used)
    buf[buf_used] = '\0';

  ptr = buf;
  if (!strncmp (ptr, "[Reference]", 11))
    {
      ptr += 11;
      if (*ptr == '\r')
        ptr++;
      if (*ptr == '\n')
        ptr++;
      href = strchr (ptr, '=') + 1;
      *strchr (ptr, '\r') = '\0';

      /* replace http by mmsh */
      if (!strncmp (href, "http", 4))
        {
          memcpy (href, "mmsh", 4);
        }
      if (this->stream->xine->verbosity >= XINE_VERBOSITY_LOG)
        printf ("demux_asf: http ref: %s\n", href);
      uevent.type = XINE_EVENT_MRL_REFERENCE;
      uevent.stream = this->stream;
      uevent.data_length = strlen (href) + sizeof (xine_mrl_reference_data_t);
      data = malloc (uevent.data_length);
      uevent.data = data;
      strcpy (data->mrl, href);
      data->alternative = 0;
      xine_event_send (this->stream, &uevent);
    }

  free (buf);

  this->status = DEMUX_FINISHED;
  return this->status;
}

/*
 * parse a stupid ASF reference in an asx file
 * format : "ASF http://www.blabla.com/blabla"
 */
static int
demux_asf_parse_asf_references (demux_asf_t * this)
{
  char *buf = NULL;
  char *ptr;
  int buf_size = 0;
  int buf_used = 0;
  int len;
  xine_mrl_reference_data_t *data;
  xine_event_t uevent;
  int i;

  /* read file to memory.
   * warning: dumb code, but hopefuly ok since reference file is small */
  do
    {
      buf_size += 1024;
      buf = realloc (buf, buf_size + 1);

      len =
        this->input->read (this->input, &buf[buf_used], buf_size - buf_used);

      if (len > 0)
        buf_used += len;

      /* 50k of reference file? no way. something must be wrong */
      if (buf_used > 50 * 1024)
        break;
    }
  while (len > 0);

  if (buf_used)
    buf[buf_used] = '\0';

  ptr = buf;
  if (!strncmp (ptr, "ASF ", 4))
    {
      ptr += 4;

      /* find the end of the string */
      for (i = 4; i < buf_used; i++)
        {
          if ((buf[i] == ' ') || (buf[i] == '\r') || (buf[i] == '\n'))
            {
              buf[i] = '\0';
              break;
            }
        }

      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: asf ref: %s\n", ptr);
      uevent.type = XINE_EVENT_MRL_REFERENCE;
      uevent.stream = this->stream;
      uevent.data_length = strlen (ptr) + sizeof (xine_mrl_reference_data_t);
      data = malloc (uevent.data_length);
      uevent.data = data;
      strcpy (data->mrl, ptr);
      data->alternative = 0;
      xine_event_send (this->stream, &uevent);
    }

  free (buf);

  this->status = DEMUX_FINISHED;
  return this->status;
}

/*
 * parse .asx playlist files
 */
static int
demux_asf_parse_asx_references (demux_asf_t * this)
{

  char *buf = NULL;
  int buf_size = 0;
  int buf_used = 0;
  int len;
  xine_mrl_reference_data_t *data;
  xine_event_t uevent;
  xml_node_t *xml_tree, *asx_entry, *asx_ref;
  xml_property_t *asx_prop;
  int result;


  /* read file to memory.
   * warning: dumb code, but hopefuly ok since reference file is small */
  do
    {
      buf_size += 1024;
      buf = realloc (buf, buf_size + 1);

      len =
        this->input->read (this->input, &buf[buf_used], buf_size - buf_used);

      if (len > 0)
        buf_used += len;

      /* 50k of reference file? no way. something must be wrong */
      if (buf_used > 50 * 1024)
        break;
    }
  while (len > 0);

  if (buf_used)
    buf[buf_used] = '\0';

  xml_parser_init (buf, buf_used, XML_PARSER_CASE_INSENSITIVE);
  if ((result = xml_parser_build_tree (&xml_tree)) != XML_PARSER_OK)
    goto __failure;

  if (!strcasecmp (xml_tree->name, "ASX"))
    {

      asx_prop = xml_tree->props;

      while ((asx_prop) && (strcasecmp (asx_prop->name, "VERSION")))
        asx_prop = asx_prop->next;

      if (asx_prop)
        {
          int version_major, version_minor = 0;

          if ((((sscanf
                 (asx_prop->value, "%d.%d", &version_major,
                  &version_minor)) == 2)
               || ((sscanf (asx_prop->value, "%d", &version_major)) == 1))
              && ((version_major == 3) && (version_minor == 0)))
            {

              asx_entry = xml_tree->child;
              while (asx_entry)
                {
                  if ((!strcasecmp (asx_entry->name, "ENTRY")) ||
                      (!strcasecmp (asx_entry->name, "ENTRYREF")))
                    {
                      char *href = NULL;

                      asx_ref = asx_entry->child;
                      while (asx_ref)
                        {

                          if (!strcasecmp (asx_ref->name, "REF"))
                            {

                              for (asx_prop = asx_ref->props; asx_prop;
                                   asx_prop = asx_prop->next)
                                {

                                  if (!strcasecmp (asx_prop->name, "HREF"))
                                    {

                                      if (!href)
                                        href = asx_prop->value;
                                    }
                                  if (href)
                                    break;
                                }
                            }
                          asx_ref = asx_ref->next;
                        }

                      if (href && strlen (href))
                        {
                          uevent.type = XINE_EVENT_MRL_REFERENCE;
                          uevent.stream = this->stream;
                          uevent.data_length =
                            strlen (href) +
                            sizeof (xine_mrl_reference_data_t);
                          data = malloc (uevent.data_length);
                          uevent.data = data;
                          strcpy (data->mrl, href);
                          data->alternative = 0;
                          xine_event_send (this->stream, &uevent);
                        }
                      href = NULL;
                    }
                  asx_entry = asx_entry->next;
                }
            }
          else
            printf ("demux_asf: Wrong ASX version: %s\n", asx_prop->value);

        }
      else
        printf ("demux_asf: Unable to find VERSION tag from ASX.\n");
    }
  else
    printf ("demux_asf: Unsupported XML type: `%s'.\n", xml_tree->name);

  xml_parser_free_tree (xml_tree);
__failure:
  free (buf);

  this->status = DEMUX_FINISHED;
  return this->status;
}


/*
 * xine specific functions start here
 */

static int
demux_asf_send_chunk (demux_plugin_t * this_gen)
{

  demux_asf_t *this = (demux_asf_t *) this_gen;

  switch (this->reference_mode)
    {
    case 1:
      return demux_asf_parse_asx_references (this);
      break;

    case 2:
      return demux_asf_parse_http_references (this);
      break;

    case 3:
      return demux_asf_parse_asf_references (this);
      break;

    default:
      asf_read_packet (this);
      return this->status;
    }
}

static void
demux_asf_dispose (demux_plugin_t * this_gen)
{

  demux_asf_t *this = (demux_asf_t *) this_gen;
  int i;

  for (i = 0; i < this->num_streams; i++)
    {
      if (this->streams[i].buffer)
        {
          free (this->streams[i].buffer);
          this->streams[i].buffer = NULL;
        }
    }

  free (this);
}

static int
demux_asf_get_status (demux_plugin_t * this_gen)
{
  demux_asf_t *this = (demux_asf_t *) this_gen;

  return this->status;
}

static void
demux_asf_send_headers (demux_plugin_t * this_gen)
{

  demux_asf_t *this = (demux_asf_t *) this_gen;
  int i;
  int stream_id;
  uint32_t buf_type, max_vrate, max_arate;
  uint32_t bitrate = 0;

  this->video_fifo = this->stream->video_fifo;
  this->audio_fifo = this->stream->audio_fifo;

  this->last_pts[0] = 0;
  this->last_pts[1] = 0;
  this->last_frame_pts = 0;

  this->status = DEMUX_OK;

  /* will get overridden later */
  this->stream->stream_info[XINE_STREAM_INFO_HAS_VIDEO] = 0;
  this->stream->stream_info[XINE_STREAM_INFO_HAS_AUDIO] = 0;

  /*
   * initialize asf engine
   */

  this->num_streams = 0;
  this->num_audio_streams = 0;
  this->num_video_streams = 0;
  this->audio_stream = 0;
  this->video_stream = 0;
  this->audio_stream_id = 0;
  this->video_stream_id = 0;
  this->control_stream_id = 0;
  this->packet_size = 0;
  this->seqno = 0;
  this->frame_duration = 3000;

  if (this->input->get_capabilities (this->input) & INPUT_CAP_SEEKABLE)
    this->input->seek (this->input, 0, SEEK_SET);

  if (this->reference_mode)
    {
      xine_demux_control_start (this->stream);
      return;
    }

  if (!asf_read_header (this))
    {

      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: asf_read_header failed.\n");

      this->status = DEMUX_FINISHED;
      return;
    }
  else
    {

      /*
       * send start buffer
       */
      xine_demux_control_start (this->stream);


      this->header_size = this->input->get_current_pos (this->input);

      this->stream->meta_info[XINE_META_INFO_TITLE] = strdup (this->title);
      this->stream->meta_info[XINE_META_INFO_ARTIST] = strdup (this->author);
      this->stream->meta_info[XINE_META_INFO_COMMENT] =
        strdup (this->comment);


      /*  Choose the best audio and the best video stream.
       *  Use the bitrate to do the choice.
       */
      max_vrate = 0;
      max_arate = 0;
      for (i = 0; i < this->num_streams; i++)
        {
          buf_type = (this->streams[i].buf_type & BUF_MAJOR_MASK);
          stream_id = this->streams[i].stream_id;
          bitrate = this->bitrates[stream_id];

          if (this->stream->xine->verbosity >= XINE_VERBOSITY_LOG)
            printf ("demux_asf: stream: %d, bitrate %d bps\n", stream_id,
                    bitrate);
          if ((buf_type == BUF_VIDEO_BASE)
              && (bitrate > max_vrate || this->video_stream_id == 0))
            {

              this->stream->stream_info[XINE_STREAM_INFO_HAS_VIDEO] = 1;
              this->stream->stream_info[XINE_STREAM_INFO_VIDEO_BITRATE] =
                bitrate;

              max_vrate = bitrate;
              this->video_stream = i;
              this->video_stream_id = stream_id;
            }
          else if ((buf_type == BUF_AUDIO_BASE) &&
                   (bitrate > max_arate || this->audio_stream_id == 0))
            {

              this->stream->stream_info[XINE_STREAM_INFO_HAS_AUDIO] = 1;
              this->stream->stream_info[XINE_STREAM_INFO_AUDIO_BITRATE] =
                bitrate;

              max_arate = bitrate;
              this->audio_stream = i;
              this->audio_stream_id = stream_id;
            }
        }

      this->stream->stream_info[XINE_STREAM_INFO_BITRATE] = bitrate;

      if (this->stream->xine->verbosity >= XINE_VERBOSITY_DEBUG)
        printf ("demux_asf: video stream_id: %d, audio stream_id: %d\n",
                this->video_stream_id, this->audio_stream_id);

      asf_send_audio_header (this, this->audio_stream);
      asf_send_video_header (this, this->video_stream);
#ifdef LOG
      printf ("demux_asf: send header done\n");
#endif

    }

  this->frame = 0;
  this->nb_frames = 1;
}

static int
demux_asf_seek (demux_plugin_t * this_gen, off_t start_pos, int start_time)
{

  demux_asf_t *this = (demux_asf_t *) this_gen;
  int i;

  this->status = DEMUX_OK;

  xine_demux_flush_engine (this->stream);

  /*
   * seek to start position
   */
  this->send_newpts = 1;
  this->frame = 0;
  this->nb_frames = 1;
  this->packet_size_left = 0;
  this->keyframe_found = (this->num_video_streams == 0);

  for (i = 0; i < this->num_streams; i++)
    {
      this->streams[i].frag_offset = 0;
      this->streams[i].seq = 0;
      this->streams[i].timestamp = 0;
    }

  if (this->input->get_capabilities (this->input) & INPUT_CAP_SEEKABLE)
    {

      if ((!start_pos) && (start_time))
        start_pos = start_time * this->rate;

      if (start_pos < this->header_size)
        start_pos = this->header_size;

      this->input->seek (this->input, start_pos, SEEK_SET);

    }

  /*
   * now start demuxing
   */

  if (!this->stream->demux_thread_running)
    {
      this->buf_flag_seek = 0;
    }
  else
    {
      this->buf_flag_seek = 1;
      xine_demux_flush_engine (this->stream);
    }

  return this->status;
}

static int
demux_asf_get_stream_length (demux_plugin_t * this_gen)
{

  demux_asf_t *this = (demux_asf_t *) this_gen;

  return this->length;
}

static uint32_t
demux_asf_get_capabilities (demux_plugin_t * this_gen)
{
  return DEMUX_CAP_NOCAP;
}

static int
demux_asf_get_optional_data (demux_plugin_t * this_gen,
                             void *data, int data_type)
{
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *
open_plugin (demux_class_t * class_gen,
             xine_stream_t * stream, input_plugin_t * input)
{

  demux_asf_t *this;
  uint8_t buf[MAX_PREVIEW_SIZE + 1];
  int len;

  switch (stream->content_detection_method)
    {
    case METHOD_BY_CONTENT:

      /*
       * try to get a preview of the data
       */
      len =
        input->get_optional_data (input, buf, INPUT_OPTIONAL_DATA_PREVIEW);
      if (len == INPUT_OPTIONAL_UNSUPPORTED)
        {

          if (input->get_capabilities (input) & INPUT_CAP_SEEKABLE)
            {

              input->seek (input, 0, SEEK_SET);
              if ((len = input->read (input, buf, 1024)) <= 0)
                return NULL;

#ifdef LOG
              printf
                ("demux_asf: PREVIEW data unavailable, but seek+read worked.\n");
#endif

            }
          else
            return NULL;
        }

      if (memcmp (buf, &guids[GUID_ASF_HEADER].guid, sizeof (GUID)))
        {
          buf[len] = '\0';
          if (!strstr (buf, "asx") &&
              !strstr (buf, "ASX") &&
              strncmp (buf, "[Reference]", 11) &&
              strncmp (buf, "ASF ", 4) &&
              ((buf[0] != 0x30)
               || (buf[1] != 0x26) || (buf[2] != 0xb2) || (buf[3] != 0x75)))
            return NULL;
        }

#ifdef LOG
      printf ("demux_asf: file starts with an asf header\n");
#endif

      break;

    case METHOD_BY_EXTENSION:
      {
        char *ending, *mrl;

        mrl = input->get_mrl (input);

        /*
         * check extension
         */

        ending = strrchr (mrl, '.');

        if (!ending)
          return NULL;

        if (strncasecmp (ending, ".asf", 4) &&
            strncasecmp (ending, ".wmv", 4) &&
            strncasecmp (ending, ".wma", 4))
          {
            return NULL;
          }
#ifdef LOG
        printf ("demux_asf: extension accepted.\n");
#endif
      }
      break;

    case METHOD_EXPLICIT:
      break;

    default:
      printf ("demux_asf: warning, unkown method %d\n",
              stream->content_detection_method);
      return NULL;
    }

  this = xine_xmalloc (sizeof (demux_asf_t));
  this->stream = stream;
  this->input = input;

  /*
   * check for reference stream
   */
  this->reference_mode = 0;
  len = input->get_optional_data (input, buf, INPUT_OPTIONAL_DATA_PREVIEW);
  if ((len == INPUT_OPTIONAL_UNSUPPORTED) &&
      (input->get_capabilities (input) & INPUT_CAP_SEEKABLE))
    {
      input->seek (input, 0, SEEK_SET);
      len = input->read (input, buf, 1024);
    }
  if (len > 0)
    {
      buf[len] = '\0';
      if (strstr (buf, "asx") || strstr (buf, "ASX"))
        this->reference_mode = 1;
      if (strstr (buf, "[Reference]"))
        this->reference_mode = 2;
      if (strstr (buf, "ASF "))
        this->reference_mode = 3;
    }

  this->demux_plugin.send_headers = demux_asf_send_headers;
  this->demux_plugin.send_chunk = demux_asf_send_chunk;
  this->demux_plugin.seek = demux_asf_seek;
  this->demux_plugin.dispose = demux_asf_dispose;
  this->demux_plugin.get_status = demux_asf_get_status;
  this->demux_plugin.get_stream_length = demux_asf_get_stream_length;
  this->demux_plugin.get_video_frame = NULL;
  this->demux_plugin.got_video_frame_cb = NULL;
  this->demux_plugin.get_capabilities = demux_asf_get_capabilities;
  this->demux_plugin.get_optional_data = demux_asf_get_optional_data;
  this->demux_plugin.demux_class = class_gen;

  this->status = DEMUX_FINISHED;

  return &this->demux_plugin;
}

static char *
get_description (demux_class_t * this_gen)
{
  return "ASF demux plugin";
}

static char *
get_identifier (demux_class_t * this_gen)
{
  return "ASF";
}

static char *
get_extensions (demux_class_t * this_gen)
{
  /* asx, wvx, wax are metafile or playlist */
  return "asf wmv wma asx wvx wax";
}

static char *
get_mimetypes (demux_class_t * this_gen)
{

  return "video/x-ms-asf: asf: ASF stream;"
    "video/x-ms-wmv: wmv: Windows Media Video;"
    "video/x-ms-wma: wma: Windows Media Audio;"
    "application/vnd.ms-asf: asf: ASF stream;"
    "application/x-mplayer2: asf,asx,asp: mplayer2;"
    "video/x-ms-asf-plugin: asf,asx,asp: mms animation;"
    "video/x-ms-wvx: wvx: wmv metafile;" "video/x-ms-wax: wva: wma metafile;";
}

static void
class_dispose (demux_class_t * this_gen)
{

  demux_asf_class_t *this = (demux_asf_class_t *) this_gen;

  free (this);
}

static void *
init_class (xine_t * xine, void *data)
{

  demux_asf_class_t *this;

  this = xine_xmalloc (sizeof (demux_asf_class_t));
  this->config = xine->config;
  this->xine = xine;

  this->demux_class.open_plugin = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier = get_identifier;
  this->demux_class.get_mimetypes = get_mimetypes;
  this->demux_class.get_extensions = get_extensions;
  this->demux_class.dispose = class_dispose;

  return this;
}


/*
 * exported plugin catalog entry
 */

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */
  {PLUGIN_DEMUX, 20, "asf", XINE_VERSION_CODE, NULL, init_class}
  ,
  {PLUGIN_NONE, 0, "", 0, NULL, NULL}
};
