/*
     This file is part of libextractor.
     Copyright (C) 2008 Heikki Lindholm

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

/**
 * @file thumbnailextractorffmpeg.c
 * @author Heikki Lindholm
 * @brief this extractor produces a binary encoded
 * thumbnail of images and videos using the ffmpeg libs.
 */

#include "platform.h"
#include "extractor.h"

#include <avformat.h>
#include <avcodec.h>
#include <swscale.h>

#define DEBUG 0

struct StreamDescriptor
{
  const uint8_t *data;
  size_t offset;
  size_t size;
};


void __attribute__ ((constructor)) ffmpeg_lib_init (void)
{
#if DEBUG
  printf ("av_register_all()\n");
#endif
  av_register_all ();
}

static int
stream_read (void *opaque, uint8_t * buf, int buf_size)
{
  struct StreamDescriptor *rs = (struct StreamDescriptor *) opaque;
  size_t len;
#if DEBUG
  printf ("read_packet: %zu\n", buf_size);
#endif
  if (rs)
    {
      if (rs->data == NULL)
        return -1;
      if (rs->offset >= rs->size)
        return 0;
      len = buf_size;
      if (rs->offset + len > rs->size)
        len = rs->size - rs->offset;

      memcpy (buf, rs->data + rs->offset, len);
      rs->offset += len;
#if DEBUG
      printf ("read_packet: len: %zu\n", len);
#endif
      return len;
    }
  return -1;
}

static offset_t
stream_seek (void *opaque, offset_t offset, int whence)
{
  struct StreamDescriptor *rs = (struct StreamDescriptor *) opaque;
  offset_t off_abs;
#if DEBUG
  printf ("my_seek: %lld %d\n", offset, whence);
#endif
  if (rs)
    {
      if (whence == AVSEEK_SIZE)
        return (offset_t) rs->size;
      else if (whence == SEEK_CUR)
        off_abs = (offset_t) rs->offset + offset;
      else if (whence == SEEK_SET)
        off_abs = offset;
      else if (whence == SEEK_END)
        off_abs = (offset_t) rs->size + offset;
      else
        {
          printf ("whence error %d\n", whence);
          abort ();
          return AVERROR (EINVAL);
        }
      if (off_abs >= 0 && off_abs < (offset_t) rs->size)
        rs->offset = (size_t) off_abs;
      else
        off_abs = AVERROR (EINVAL);
      return off_abs;
    }
  return -1;
}

static EXTRACTOR_KeywordList *
addKeyword (EXTRACTOR_KeywordType type,
            char *keyword, EXTRACTOR_KeywordList * next)
{
  EXTRACTOR_KeywordList *result;

  if (keyword == NULL)
    return next;
  result = malloc (sizeof (EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}


struct MimeToDecoderMapping
{
  const char *mime_type;
  enum CodecID codec_id;
};

/* map mime image types to a decoder */
static const struct MimeToDecoderMapping m2d_map[] = {
  {"image/x-bmp", CODEC_ID_BMP},
  {"image/gif", CODEC_ID_GIF},
  {"image/jpeg", CODEC_ID_MJPEG},
  {"image/png", CODEC_ID_PNG},
  {"image/x-portable-pixmap", CODEC_ID_PPM},
  {NULL, CODEC_ID_NONE}
};

#define PROBE_MAX (1<<20)
#define BIOBUF_SIZE (64*1024)
#define THUMBSIZE 128           /* max dimension in pixels */
#define MAX_THUMB_SIZE (100*1024)       /* in bytes */

struct EXTRACTOR_Keywords *
libextractor_thumbnailffmpeg_extract (const char *filename,
                                      const unsigned char *data,
                                      size_t size,
                                      struct EXTRACTOR_Keywords *prev)
{
  int score;

  AVInputFormat *fmt;
  AVProbeData pdat;

  ByteIOContext *bio_ctx = NULL;
  uint8_t *bio_buffer;
  struct StreamDescriptor reader_state;

  AVFormatContext *format_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  AVPacket packet;
  int video_stream_index;
  AVCodec *codec;
  AVFrame *frame = NULL;
  AVFrame *thumb_frame = NULL;
  int64_t ts;

  struct SwsContext *scaler_ctx;
  int sws_flags = SWS_BILINEAR;
  uint8_t *thumb_buffer;
  int thumb_width, thumb_height;
  int sar_num, sar_den;

  FILE *output = NULL;
  uint8_t *encoder_output_buffer;
  size_t encoder_output_buffer_size;
  AVCodecContext *enc_codec_ctx;
  AVCodec *enc_codec;

  int i;
  int err;
  int frame_finished;

  char *binary;
  const char *mime;
  int is_image;
  enum CodecID image_codec_id;

  bio_ctx = NULL;
  bio_buffer = NULL;
  format_ctx = NULL;
  codec = NULL;
  frame = NULL;
  thumb_frame = NULL;
  thumb_buffer = NULL;
  scaler_ctx = NULL;
  encoder_output_buffer = NULL;
  enc_codec = NULL;
  enc_codec_ctx = NULL;

  is_image = 0;

  mime = EXTRACTOR_extractLast (EXTRACTOR_MIMETYPE, prev);
  if (mime != NULL)
    {
      i = 0;
      while (m2d_map[i].mime_type != NULL)
        {
          if (!strcmp (m2d_map[i].mime_type, mime))
            {
              is_image = 1;
              image_codec_id = m2d_map[i].codec_id;
              break;
            }
          i++;
        }
    }

#if DEBUG
  printf ("is_image: %d codec:%d\n", is_image, image_codec_id);
#endif
  if (!is_image)
    {
      pdat.filename = filename;
      pdat.buf = (unsigned char *) data;
      pdat.buf_size = (size > PROBE_MAX) ? PROBE_MAX : size;

      fmt = av_probe_input_format (&pdat, 1);
      if (fmt == NULL)
        return prev;
#if DEBUG
      printf ("format %p [%s] [%s]\n", fmt, fmt->name, fmt->long_name);
#endif
      pdat.buf = (unsigned char *) data;
      pdat.buf_size = size > PROBE_MAX ? PROBE_MAX : size;
      score = fmt->read_probe (&pdat);
#if DEBUG
      printf ("score: %d\n", score);
#endif
      /*if (score < 50) return prev; */
    }

  if (is_image)
    {
      codec_ctx = avcodec_alloc_context ();
      codec = avcodec_find_decoder (image_codec_id);
      if (codec != NULL)
        {
          if (avcodec_open (codec_ctx, codec) != 0)
            {
#if DEBUG
              printf ("open codec failed\n");
#endif
              codec = NULL;
            }
        }
    }
  else
    {
      bio_ctx = malloc (sizeof (ByteIOContext));
      bio_buffer = malloc (BIOBUF_SIZE);

      reader_state.data = data;
      reader_state.offset = 0;
      reader_state.size = size;

      init_put_byte (bio_ctx, bio_buffer,
                     BIOBUF_SIZE, 0, &reader_state,
                     stream_read, NULL, stream_seek);

      fmt->flags |= AVFMT_NOFILE;
      err = av_open_input_stream (&format_ctx, bio_ctx, "", fmt, NULL);
      if (err < 0)
        {
#if DEBUG
          printf ("couldn't open input stream\n");
#endif
          goto out;
        }

      err = av_find_stream_info (format_ctx);
      if (err < 0)
        {
#if DEBUG
          printf ("couldn't find codec params\n");
#endif
          goto out;
        }

      for (i = 0; i < format_ctx->nb_streams; i++)
        {
          codec_ctx = format_ctx->streams[i]->codec;
          if (codec_ctx->codec_type == CODEC_TYPE_VIDEO)
            {
              video_stream_index = i;
              codec = avcodec_find_decoder (codec_ctx->codec_id);
              if (codec == NULL)
                {
#if DEBUG
                  printf ("find_decoder failed\n");
#endif
                  break;
                }
              err = avcodec_open (codec_ctx, codec);
              if (err != 0)
                {
#if DEBUG
                  printf ("failed to open codec\n");
#endif
                  codec = NULL;
                }
              break;
            }
        }
    }

  if (codec_ctx == NULL || codec == NULL)
    {
#if DEBUG
      printf ("failed to open codec");
#endif
      goto out;
    }
  frame = avcodec_alloc_frame ();
  if (frame == NULL)
    {
#if DEBUG
      printf ("failed to alloc frame");
#endif
      goto out;
    }

  if (!is_image)
    {
#if DEBUG
      printf ("duration: %lld\n", format_ctx->duration);
      if (format_ctx->duration == AV_NOPTS_VALUE)
        printf ("duration unknown\n");
#endif
      /* TODO: if duration is known seek to to some better place(?) */
      ts = 10;                  // s
      ts = ts * AV_TIME_BASE;
      err = av_seek_frame (format_ctx, -1, ts, 0);
      if (err >= 0)
        {
          avcodec_flush_buffers (codec_ctx);
        }
#if DEBUG
      else
        printf ("seeking failed %d\n", err);
#endif
    }

  frame_finished = 0;
  if (is_image)
    {
      avcodec_decode_video (codec_ctx, frame, &frame_finished, data, size);
    }
  else
    {
      while (1)
        {
          err = av_read_frame (format_ctx, &packet);
          if (err < 0)
            break;
          if (packet.stream_index == video_stream_index)
            {
              avcodec_decode_video (codec_ctx,
                                    frame,
                                    &frame_finished,
                                    packet.data, packet.size);
              if (frame_finished && frame->key_frame)
                {
                  av_free_packet (&packet);
                  break;
                }
            }
          av_free_packet (&packet);
        }
    }

  if (!frame_finished)
    goto out;

  sar_num = codec_ctx->sample_aspect_ratio.num;
  sar_den = codec_ctx->sample_aspect_ratio.den;
  if (sar_num <= 0 || sar_den <= 0)
    {
      sar_num = 1;
      sar_den = 1;
    }
  if ((codec_ctx->width * sar_num) / sar_den > codec_ctx->height)
    {
      thumb_width = THUMBSIZE;
      thumb_height = (thumb_width * codec_ctx->height) /
        ((codec_ctx->width * sar_num) / sar_den);
    }
  else
    {
      thumb_height = THUMBSIZE;
      thumb_width = (thumb_height *
                     ((codec_ctx->width * sar_num) / sar_den)) /
        codec_ctx->height;
    }
  if (thumb_width < 8)
    thumb_width = 8;
  if (thumb_height < 1)
    thumb_height = 1;
#if DEBUG
  printf ("thumb dim: %d %d\n", thumb_width, thumb_height);
#endif

  scaler_ctx =
    sws_getContext (codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                    thumb_width, thumb_height, PIX_FMT_RGB24, sws_flags, NULL,
                    NULL, NULL);
  if (scaler_ctx == NULL)
    {
#if DEBUG
      printf ("failed to alloc scaler\n");
#endif
      goto out;
    }
  thumb_frame = avcodec_alloc_frame ();
  thumb_buffer =
    av_malloc (avpicture_get_size (PIX_FMT_RGB24, thumb_width, thumb_height));
  if (thumb_frame == NULL || thumb_buffer == NULL)
    {
#if DEBUG
      printf ("failed to alloc thumb frame\n");
#endif
      goto out;
    }
  avpicture_fill ((AVPicture *) thumb_frame, thumb_buffer,
                  PIX_FMT_RGB24, thumb_width, thumb_height);

  sws_scale (scaler_ctx,
             frame->data, frame->linesize,
             0, codec_ctx->height, thumb_frame->data, thumb_frame->linesize);

  encoder_output_buffer_size = MAX_THUMB_SIZE;
  encoder_output_buffer = av_malloc (encoder_output_buffer_size);
  if (encoder_output_buffer == NULL)
    {
#if DEBUG
      printf ("couldn't alloc encoder output buf\n");
#endif
      goto out;
    }

  enc_codec = avcodec_find_encoder_by_name ("png");
  if (enc_codec == NULL)
    {
#if DEBUG
      printf ("couldn't find encoder\n");
#endif
      goto out;
    }
  enc_codec_ctx = avcodec_alloc_context ();
  enc_codec_ctx->width = thumb_width;
  enc_codec_ctx->height = thumb_height;
  enc_codec_ctx->pix_fmt = PIX_FMT_RGB24;

  if (avcodec_open (enc_codec_ctx, enc_codec) < 0)
    {
#if DEBUG
      printf ("couldn't open encoder\n");
#endif
      enc_codec = NULL;
      goto out;
    }

  err = avcodec_encode_video (enc_codec_ctx,
                              encoder_output_buffer,
                              encoder_output_buffer_size, thumb_frame);
  if (err <= 0)
    goto out;

  binary =
    EXTRACTOR_binaryEncode ((const unsigned char *) encoder_output_buffer,
                            err);
  if (binary != NULL)
    prev = addKeyword (EXTRACTOR_THUMBNAIL_DATA, binary, prev);

out:
  if (enc_codec != NULL)
    avcodec_close (enc_codec_ctx);
  if (enc_codec_ctx != NULL)
    av_free (enc_codec_ctx);
  if (encoder_output_buffer != NULL)
    av_free (encoder_output_buffer);
  if (scaler_ctx != NULL)
    sws_freeContext(scaler_ctx);
  if (codec != NULL)
    avcodec_close (codec_ctx);
  if (format_ctx != NULL)
    av_close_input_file (format_ctx);
  if (frame != NULL)
    av_free (frame);
  if (thumb_buffer != NULL)
    av_free (thumb_buffer);
  if (thumb_frame != NULL)
    av_free (thumb_frame);
  if (bio_ctx != NULL)
    free (bio_ctx);
  if (bio_buffer != NULL)
    free (bio_buffer);

  return prev;
}

struct EXTRACTOR_Keywords *
libextractor_thumbnail_extract (const char *filename,
                                const unsigned char *data,
                                size_t size,
                                struct EXTRACTOR_Keywords *prev,
                                const char *options)
{
  return libextractor_thumbnailffmpeg_extract (filename, data, size, prev);
}

/* end of thumbnailextractorffmpeg.c */
