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

void __attribute__ ((constructor)) ffmpeg_lib_init (void)
{
  av_register_all ();
}

#define THUMBSIZE 128           /* max dimension in pixels */
#define MAX_THUMB_SIZE (100*1024)       /* in bytes */


const char *
EXTRACTOR_thumbnailffmpeg_options ()
{
  return "close-stderr";
}

const char *
EXTRACTOR_thumbnail_options ()
{
  return "close-stderr";
}

/*
 * Rescale and encode a png thumbnail
 * on success, fills in output_data and returns the number of bytes used
 */
static size_t create_thumbnail(
  int src_width, int src_height, int src_stride[],
  enum PixelFormat src_pixfmt, uint8_t *src_data[],
  int dst_width, int dst_height,
  uint8_t **output_data, size_t output_max_size)
{
  AVCodecContext *encoder_codec_ctx = NULL;
  AVCodec *encoder_codec = NULL;
  struct SwsContext *scaler_ctx = NULL;
  int sws_flags = SWS_BILINEAR;
  AVFrame *dst_frame = NULL;
  uint8_t *dst_buffer = NULL;
  uint8_t *encoder_output_buffer = NULL;
  size_t encoder_output_buffer_size;
  int err;

  encoder_codec = avcodec_find_encoder_by_name ("png");
  if (encoder_codec == NULL)
    {
#if DEBUG
      fprintf (stderr,
	       "Couldn't find a PNG encoder\n");
#endif
      return 0;
    }

  /* NOTE: the scaler will be used even if the src and dst image dimensions
   * match, because the scaler will also perform colour space conversion */
  scaler_ctx =
    sws_getContext (src_width, src_height, src_pixfmt,
                    dst_width, dst_height, PIX_FMT_RGB24, 
                    sws_flags, NULL, NULL, NULL);
  if (scaler_ctx == NULL)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to get a scaler context\n");
#endif
      return 0;
    }

  dst_frame = avcodec_alloc_frame ();
  if (dst_frame == NULL)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to allocate the destination image frame\n");
#endif
      sws_freeContext(scaler_ctx);
      return 0;
    }
  dst_buffer =
    av_malloc (avpicture_get_size (PIX_FMT_RGB24, dst_width, dst_height));
  if (dst_buffer == NULL)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to allocate the destination image buffer\n");
#endif
      av_free (dst_frame);
      sws_freeContext(scaler_ctx);
      return 0;
    }
  avpicture_fill ((AVPicture *) dst_frame, dst_buffer,
                  PIX_FMT_RGB24, dst_width, dst_height);
      
  sws_scale (scaler_ctx,
             src_data, 
             src_stride,
             0, src_height, 
             dst_frame->data, 
             dst_frame->linesize);

  encoder_output_buffer_size = output_max_size;
  encoder_output_buffer = av_malloc (encoder_output_buffer_size);
  if (encoder_output_buffer == NULL)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to allocate the encoder output buffer\n");
#endif
      av_free (dst_buffer);
      av_free (dst_frame);
      sws_freeContext(scaler_ctx);
      return 0;
    }

  encoder_codec_ctx = avcodec_alloc_context ();
  if (encoder_codec_ctx == NULL)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to allocate the encoder codec context\n");
#endif
      av_free (encoder_output_buffer);
      av_free (dst_buffer);
      av_free (dst_frame);
      sws_freeContext(scaler_ctx);
      return 0;
    }
  encoder_codec_ctx->width = dst_width;
  encoder_codec_ctx->height = dst_height;
  encoder_codec_ctx->pix_fmt = PIX_FMT_RGB24;

  if (avcodec_open (encoder_codec_ctx, encoder_codec) < 0)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to open the encoder\n");
#endif
      av_free (encoder_codec_ctx);
      av_free (encoder_output_buffer);
      av_free (dst_buffer);
      av_free (dst_frame);
      sws_freeContext(scaler_ctx);
      return 0;
    }

  err = avcodec_encode_video (encoder_codec_ctx,
                              encoder_output_buffer,
                              encoder_output_buffer_size, dst_frame);

  avcodec_close (encoder_codec_ctx);
  av_free (encoder_codec_ctx);
  av_free (dst_buffer);
  av_free (dst_frame);
  sws_freeContext(scaler_ctx);

  *output_data = encoder_output_buffer;

  return err < 0 ? 0 : err;
}

int 
EXTRACTOR_thumbnailffmpeg_extract (const unsigned char *data,
				   size_t size,
				   EXTRACTOR_MetaDataProcessor proc,
				   void *proc_cls,
				   const char *options)
{
  AVProbeData pd; 
  AVPacket packet;
  AVInputFormat *input_format;
  int input_format_nofileflag;
  ByteIOContext *bio_ctx;
  struct AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  AVCodec *codec = NULL;
  AVFrame *frame = NULL;
  uint8_t *encoded_thumbnail;
  int video_stream_index;
  int thumb_width;
  int thumb_height;
  int sar_num;
  int sar_den;
  int i;
  int err;
  int frame_finished;
  int ret = 0;

#if DEBUG
  fprintf (stderr,
	   "ffmpeg starting\n");
#endif
  /* probe format
   * initial try with a smaller probe size for efficiency */
  pd.filename = "";
  pd.buf = (void *) data; 
  pd.buf_size = 128*1024 > size ? size : 128*1024;
RETRY_PROBE: 
  if (NULL == (input_format = av_probe_input_format(&pd, 1))) 
    {
#if DEBUG
      fprintf (stderr,
	       "Failed to probe input format\n");
#endif
      if (pd.buf_size != size) /* retry probe once with full data size */
        {
	  pd.buf_size = size;
          goto RETRY_PROBE;
        }
      return 0;
    }
  input_format_nofileflag = input_format->flags & AVFMT_NOFILE;
  input_format->flags |= AVFMT_NOFILE;
  bio_ctx = NULL; 
  pd.buf_size = size;
  url_open_buf(&bio_ctx, pd.buf, pd.buf_size, URL_RDONLY);
  bio_ctx->is_streamed = 1;  
  if ((av_open_input_stream(&format_ctx, bio_ctx, pd.filename, input_format, NULL)) < 0)
    {
 #if DEBUG
      fprintf (stderr,
	       "Failed to open input stream\n");
#endif
      url_close_buf (bio_ctx);
      if (!input_format_nofileflag)
        input_format->flags ^= AVFMT_NOFILE;
      return 0;
    }
  if (0 > av_find_stream_info (format_ctx))
    {
 #if DEBUG
      fprintf (stderr,
               "Failed to read stream info\n");
#endif
      av_close_input_stream (format_ctx);
      url_close_buf (bio_ctx);
      if (!input_format_nofileflag)
        input_format->flags ^= AVFMT_NOFILE;
      return 0;
    }

  codec_ctx = NULL;
  for (i=0; i<format_ctx->nb_streams; i++)
    {
      codec_ctx = format_ctx->streams[i]->codec;
      if (codec_ctx->codec_type != CODEC_TYPE_VIDEO)
	continue;
      codec = avcodec_find_decoder (codec_ctx->codec_id);
      if (codec == NULL)
	continue;
      err = avcodec_open (codec_ctx, codec);
      if (err != 0)
	{
	  codec = NULL;
	  continue;
	}
      video_stream_index = i;
      break;
    } 
  if ( (codec_ctx == NULL) || 
       (codec == NULL) ||
       (codec_ctx->width == 0) || 
       (codec_ctx->height == 0) )
    {
#if DEBUG
      fprintf (stderr,
               "No video streams or no suitable codec found\n");
#endif
      if (codec_ctx != NULL)
	avcodec_close (codec_ctx);
      av_close_input_stream (format_ctx);
      url_close_buf (bio_ctx);
      if (!input_format_nofileflag)
        input_format->flags ^= AVFMT_NOFILE;
      return 0;
    }

  frame = avcodec_alloc_frame ();
  if (frame == NULL)
    {
#if DEBUG
      fprintf (stderr,
               "Failed to allocate frame\n");
#endif
      if (codec != NULL)
	avcodec_close (codec_ctx);
      av_close_input_stream (format_ctx);
      url_close_buf (bio_ctx);
      if (!input_format_nofileflag)
        input_format->flags ^= AVFMT_NOFILE;
      return 0;
    }
#if DEBUG
  if (format_ctx->duration == AV_NOPTS_VALUE)
    fprintf (stderr,
	     "Duration unknown\n");
  else
    fprintf (stderr,
	     "Duration: %lld\n", 
	     format_ctx->duration);      
#endif
  /* TODO: if duration is known, seek to some better place,
   * but use 10 sec into stream for now */
  err = av_seek_frame (format_ctx, -1, 10 * AV_TIME_BASE, 0);
  if (err >= 0)        
    avcodec_flush_buffers (codec_ctx);        
  frame_finished = 0;

  if (0 /* is_image */)
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
    {
      fprintf (stderr,
	       "Failed to decode a complete frame\n");
      if (frame != NULL)
	av_free (frame);
      av_close_input_stream (format_ctx);
      free (bio_ctx);
      if (!input_format_nofileflag)
        input_format->flags ^= AVFMT_NOFILE;
      return 0;
    }

  /* calculate the thumbnail dimensions, taking pixel aspect into account */
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
  fprintf (stderr,
           "Thumbnail dimensions: %d %d\n", 
           thumb_width, thumb_height);
#endif

  err = create_thumbnail (codec_ctx->width, codec_ctx->height,
                          frame->linesize, codec_ctx->pix_fmt, frame->data,
                          thumb_width, thumb_height,
                          &encoded_thumbnail, MAX_THUMB_SIZE);

  if (err > 0)
    ret = proc (proc_cls,
		"thumbnailffmpeg",
		EXTRACTOR_METATYPE_THUMBNAIL,
		EXTRACTOR_METAFORMAT_BINARY,
		"image/png",
		(const char*) encoded_thumbnail,
		err);

  avcodec_close (codec_ctx);
  av_close_input_stream (format_ctx);
  if (!input_format_nofileflag)
    input_format->flags ^= AVFMT_NOFILE;
  av_free (frame);
  url_close_buf (bio_ctx);
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
