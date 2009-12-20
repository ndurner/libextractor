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
 * @file thumbnailffmpeg_extractor.c
 * @author Heikki Lindholm
 * @brief this extractor produces a binary encoded
 * thumbnail of images and videos using the ffmpeg libs.
 */

/* This is a thumbnail extractor using the ffmpeg libraries that will eventually
   support extracting thumbnails from both image and video files. 

   Note that ffmpeg has a few issues:
   (1) there are no recent official releases of the ffmpeg libs
   (2) ffmpeg has a history of having security issues (parser is not robust)

   So this plugin cannot be recommended for system with high security
   requirements. 
*/

#include "platform.h"
#include "extractor.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

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

static int64_t
stream_seek (void *opaque, int64_t offset, int whence)
{
  struct StreamDescriptor *rs = (struct StreamDescriptor *) opaque;
  int64_t off_abs;
#if DEBUG
  printf ("my_seek: %lld %d\n", offset, whence);
#endif
  if (rs)
    {
      if (whence == AVSEEK_SIZE)
        return (int64_t) rs->size;
      else if (whence == SEEK_CUR)
        off_abs = (int64_t) rs->offset + offset;
      else if (whence == SEEK_SET)
        off_abs = offset;
      else if (whence == SEEK_END)
        off_abs = (int64_t) rs->size + offset;
      else
        {
          printf ("whence error %d\n", whence);
          abort ();
          return AVERROR (EINVAL);
        }
      if (off_abs >= 0 && off_abs < (int64_t) rs->size)
        rs->offset = (size_t) off_abs;
      else
        off_abs = AVERROR (EINVAL);
      return off_abs;
    }
  return -1;
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


/* ******** mime type detection code (copied from mime_extractor) ************* */


/**
 * Detect a file-type.
 * @param data the contents of the file
 * @param len the length of the file
 * @param arg closure...
 * @return 0 if the file does not match, 1 if it does
 **/
typedef int (*Detector) (const char *data, size_t len, void *arg);

/**
 * Detect a file-type.
 * @param data the contents of the file
 * @param len the length of the file
 * @return always 1
 **/
static int
defaultDetector (const char *data, size_t len, void *arg)
{
  return 1;
}

/**
 * Detect a file-type.
 * @param data the contents of the file
 * @param len the length of the file
 * @return always 0
 **/
static int
disableDetector (const char *data, size_t len, void *arg)
{
  return 0;
}

typedef struct ExtraPattern
{
  int pos;
  int len;
  const char *pattern;
} ExtraPattern;

/**
 * Define special matching rules for complicated formats...
 **/
static ExtraPattern xpatterns[] = {
#define AVI_XPATTERN 0
  {8, 4, "AVI "},
  {0, 0, NULL},
#define WAVE_XPATTERN 2
  {8, 4, "WAVE"},
  {0, 0, NULL},
#define ACON_XPATTERN 12
  {8, 4, "ACON"},
  {0, 0, NULL},
#define CR2_PATTERN 14
  {8, 3, "CR\x02"},
  {0, 0, NULL},
};

/**
 * Detect AVI. A pattern matches if all XPatterns until the next {0,
 * 0, NULL} slot match. OR-ing patterns can be achieved using multiple
 * entries in the main table, so this "AND" (all match) semantics are
 * the only reasonable answer.
 **/
static int
xPatternMatcher (const char *data, size_t len, void *cls)
{
  ExtraPattern *arg = cls;

  while (arg->pattern != NULL)
    {
      if (arg->pos + arg->len > len)
        return 0;
      if (0 != memcmp (&data[arg->pos], arg->pattern, arg->len))
        return 0;
      arg++;
    }
  return 1;
}

/**
 * Use this detector, if the simple header-prefix matching is
 * sufficient.
 */
#define DEFAULT &defaultDetector, NULL

/**
 * Use this detector, to disable the mime-type (effectively comment it
 * out).
 */
#define DISABLED &disableDetector, NULL

/**
 * Select an entry in xpatterns for matching
 */
#define XPATTERN(a) &xPatternMatcher, &xpatterns[(a)]


typedef struct Pattern
{
  const char *pattern;
  int size;
  const char *mimetype;
  Detector detector;
  void *arg;
} Pattern;


/* FIXME: find out if ffmpeg actually makes sense for all of these,
   and add those for which it does make sense to the m2d_map! */
static Pattern patterns[] = {
  {"\xFF\xD8", 2, "image/jpeg", DEFAULT},
  {"\211PNG\r\n\032\n", 8, "image/png", DEFAULT},
  {"/* XPM */", 9, "image/x-xpm", DEFAULT},
  {"GIF8", 4, "image/gif", DEFAULT},
  {"P1", 2, "image/x-portable-bitmap", DEFAULT},
  {"P2", 2, "image/x-portable-graymap", DEFAULT},
  {"P3", 2, "image/x-portable-pixmap", DEFAULT},
  {"P4", 2, "image/x-portable-bitmap", DEFAULT},
  {"P5", 2, "image/x-portable-graymap", DEFAULT},
  {"P6", 2, "image/x-portable-pixmap", DEFAULT},
  {"P7", 2, "image/x-portable-anymap", DEFAULT},
  {"BM", 2, "image/x-bmp", DEFAULT},
  {"\x89PNG", 4, "image/x-png", DEFAULT},
  {"id=ImageMagick", 14, "application/x-imagemagick-image", DEFAULT},
  {"hsi1", 4, "image/x-jpeg-proprietary", DEFAULT},
  {"FLV", 3, "video/x-flv", DEFAULT},
  {"\x2E\x52\x4d\x46", 4, "video/real", DEFAULT},
  {"\x2e\x72\x61\xfd", 4, "audio/real", DEFAULT},
  {"gimp xcf", 8, "image/xcf", DEFAULT},
  {"II\x2a\x00\x10", 5, "image/x-canon-cr2", XPATTERN (CR2_PATTERN)},
  {"IIN1", 4, "image/tiff", DEFAULT},
  {"MM\x00\x2a", 4, "image/tiff", DEFAULT},     /* big-endian */
  {"II\x2a\x00", 4, "image/tiff", DEFAULT},     /* little-endian */
  {"RIFF", 4, "video/x-msvideo", XPATTERN (AVI_XPATTERN)},
  {"RIFF", 4, "audio/x-wav", XPATTERN (WAVE_XPATTERN)},
  {"RIFX", 4, "video/x-msvideo", XPATTERN (AVI_XPATTERN)},
  {"RIFX", 4, "audio/x-wav", XPATTERN (WAVE_XPATTERN)},
  {"RIFF", 4, "image/x-animated-cursor", XPATTERN (ACON_XPATTERN)},
  {"RIFX", 4, "image/x-animated-cursor", XPATTERN (ACON_XPATTERN)},
  {"\x00\x00\x01\xb3", 4, "video/mpeg", DEFAULT},
  {"\x00\x00\x01\xba", 4, "video/mpeg", DEFAULT},
  {"moov", 4, "video/quicktime", DEFAULT},
  {"mdat", 4, "video/quicktime", DEFAULT},
  {"\x8aMNG", 4, "video/x-mng", DEFAULT},
  {"\x30\x26\xb2\x75\x8e\x66", 6, "video/asf", DEFAULT},        /* same as .wmv ? */
  {"FWS", 3, "application/x-shockwave-flash", DEFAULT},
  {NULL, 0, NULL, DISABLED}
};



static const char *
find_mime (const char *data,
	   size_t size)
{
  int i;

  i = 0;
  while (patterns[i].pattern != NULL)
    {
      if (size < patterns[i].size)
        {
          i++;
          continue;
        }
      if (0 == memcmp (patterns[i].pattern, data, patterns[i].size))
        {
          if (patterns[i].detector (data, size, patterns[i].arg))
	    return patterns[i].mimetype;
	}
      i++;
    }
  return NULL;
}


int 
EXTRACTOR_thumbnailffmpeg_extract (const unsigned char *data,
				   size_t size,
				   EXTRACTOR_MetaDataProcessor proc,
				   void *proc_cls,
				   const char *options)
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

  uint8_t *encoder_output_buffer;
  size_t encoder_output_buffer_size;
  AVCodecContext *enc_codec_ctx;
  AVCodec *enc_codec;

  int i;
  int err;
  int ret = 0;
  int frame_finished;

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

  mime = find_mime ((const char*) data, size);
  if (mime == NULL)
    return 0;
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
      pdat.filename = NULL;
      pdat.buf = (unsigned char *) data;
      pdat.buf_size = (size > PROBE_MAX) ? PROBE_MAX : size;

      fmt = av_probe_input_format (&pdat, 1);
      if (fmt == NULL)
        return 0;
#if DEBUG
      printf ("format %p [%s] [%s]\n", fmt, fmt->name, fmt->long_name);
#endif
      pdat.buf = (unsigned char *) data;
      pdat.buf_size = size > PROBE_MAX ? PROBE_MAX : size;
      score = fmt->read_probe (&pdat);
#if DEBUG
      printf ("score: %d\n", score);
#endif
      /*if (score < 50) return 0; */
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

  if (!frame_finished || codec_ctx->width == 0 || codec_ctx->height == 0)
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
  if (err > 0)
    ret = proc (proc_cls,
		"thumbnailffmpeg",
		EXTRACTOR_METATYPE_THUMBNAIL,
		EXTRACTOR_METAFORMAT_BINARY,
		"image/png",
		(const char*) encoder_output_buffer,
		err);
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

  return ret;
}

int 
EXTRACTOR_thumbnail_extract (const unsigned char *data,
			     size_t size,
			     EXTRACTOR_MetaDataProcessor proc,
			     void *proc_cls,
			     const char *options)
{
  return EXTRACTOR_thumbnailffmpeg_extract (data, size, proc, proc_cls, options);
}

/* end of thumbnailffmpeg_extractor.c */
