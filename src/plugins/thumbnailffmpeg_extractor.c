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
  return "force-kill;oop-only;close-stderr";
}

const char *
EXTRACTOR_thumbnail_options ()
{
  return "force-kill;oop-only;close-stderr";
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
  AVInputFormat *m_pInputFormat;
  ByteIOContext *bio;
  struct AVFormatContext *fmt;
  AVCodecContext *codec_ctx;
  AVCodec *codec = NULL;
  AVFrame *frame = NULL;
  AVFrame *thumb_frame = NULL;
  uint8_t *encoder_output_buffer = NULL;
  AVCodecContext *enc_codec_ctx = NULL;
  AVCodec *enc_codec = NULL;
  const AVFrame *tframe;
  struct SwsContext *scaler_ctx = NULL;
  uint8_t *thumb_buffer = NULL;
  int sws_flags = SWS_BILINEAR;
  int64_t ts;
  size_t encoder_output_buffer_size;
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
  pd.buf_size = size;
  pd.buf = (void *) data; 
  pd.filename = "__url_prot";
 
  if (NULL == (m_pInputFormat = av_probe_input_format(&pd, 1))) 
    {
#if DEBUG
      fprintf (stderr,
	       "Failed to probe input format\n");
#endif
      return 0;
    }
  m_pInputFormat->flags |= AVFMT_NOFILE;
  bio = NULL; 
  url_open_buf(&bio, pd.buf, pd.buf_size, URL_RDONLY);
  bio->is_streamed = 1;  
  if ((av_open_input_stream(&fmt, bio, pd.filename, m_pInputFormat, NULL)) < 0)
    {
      url_close_buf (bio);
      fprintf (stderr,
	       "Failed to open input stream\n");
      return 0;
    }
  if (0 > av_find_stream_info (fmt))
    {
      av_close_input_stream (fmt);
      url_close_buf (bio);
      fprintf (stderr,
	       "Failed to read stream info\n");
      return 0;
    }

  codec_ctx = NULL;
  for (i=0; i<fmt->nb_streams; i++)
    {
      codec_ctx = fmt->streams[i]->codec;
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
      if (codec_ctx != NULL)
	avcodec_close (codec_ctx);
      av_close_input_stream (fmt);
      url_close_buf (bio);
      fprintf (stderr,
	       "No video codec found\n");
      return 0;
    }

  frame = avcodec_alloc_frame ();
  if (frame == NULL)
    {
      if (codec != NULL)
	avcodec_close (codec_ctx);
      av_close_input_stream (fmt);
      url_close_buf (bio);
      fprintf (stderr,
	       "Failed to allocate frame\n");
      return 0;
    }
#if DEBUG
  if (fmt->duration == AV_NOPTS_VALUE)
    fprintf (stderr,
	     "duration unknown\n");
  else
    fprintf (stderr,
	     "duration: %lld\n", 
	     fmt->duration);      
#endif
  /* TODO: if duration is known seek to to some better place(?) */
  ts = 10;                  /* s */
  ts = ts * AV_TIME_BASE;
  err = av_seek_frame (fmt, -1, ts, 0);
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
          err = av_read_frame (fmt, &packet);
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
      if (frame != NULL)
	av_free (frame);
      av_close_input_stream (fmt);
      free (bio);
      fprintf (stderr,
	       "Failed to seek to frame\n");
      return 0;
    }

  sar_num = codec_ctx->sample_aspect_ratio.num;
  sar_den = codec_ctx->sample_aspect_ratio.den;
  if (sar_num <= 0 || sar_den <= 0)
    {
      sar_num = 1;
      sar_den = 1;
    }
  if ( (codec_ctx->width < THUMBSIZE) &&
       (codec_ctx->height < THUMBSIZE) )
    {
      /* no resize */
      thumb_width = codec_ctx->width;
      thumb_height = codec_ctx->height;
      tframe = frame;
    }
  else
    {
      /* need resize */
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
	       "thumb dim: %d %d\n", 
	       thumb_width,
	       thumb_height);
#endif

      scaler_ctx =
	sws_getContext (codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
			thumb_width, thumb_height, PIX_FMT_RGB24, sws_flags, NULL,
			NULL, NULL);
      if (scaler_ctx == NULL)
	{
#if DEBUG
	  fprintf (stderr,
		   "failed to alloc scaler\n");
#endif
	  goto out;
	}
      thumb_frame = avcodec_alloc_frame ();
      thumb_buffer =
	av_malloc (avpicture_get_size (PIX_FMT_RGB24, thumb_width, thumb_height));
      if (thumb_frame == NULL || thumb_buffer == NULL)
	{
#if DEBUG
	  fprintf (stderr,
		   "failed to alloc thumb frame\n");
#endif
	  goto out;
	}
      avpicture_fill ((AVPicture *) thumb_frame, thumb_buffer,
		      PIX_FMT_RGB24, thumb_width, thumb_height);
      
      sws_scale (scaler_ctx,
		 frame->data, 
		 frame->linesize,
		 0, codec_ctx->height, 
		 thumb_frame->data, 
		 thumb_frame->linesize);
      tframe = thumb_frame;  
    }

  encoder_output_buffer_size = MAX_THUMB_SIZE;
  encoder_output_buffer = av_malloc (encoder_output_buffer_size);
  if (encoder_output_buffer == NULL)
    {
#if DEBUG
      fprintf (stderr,
	       "couldn't alloc encoder output buf\n");
#endif
      goto out;
    }

  enc_codec = avcodec_find_encoder_by_name ("png");
  if (enc_codec == NULL)
    {
#if DEBUG
      fprintf (stderr,
	       "couldn't find encoder\n");
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
      fprintf (stderr,
	       "couldn't open encoder\n");
#endif
      enc_codec = NULL;
      goto out;
    }

  err = avcodec_encode_video (enc_codec_ctx,
                              encoder_output_buffer,
                              encoder_output_buffer_size, tframe);
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
  if (codec_ctx != NULL)
    avcodec_close (codec_ctx);
  if (fmt != NULL)
    av_close_input_stream (fmt);
  if (frame != NULL)
    av_free (frame);
  if (thumb_buffer != NULL)
    av_free (thumb_buffer);
  if (thumb_frame != NULL)
    av_free (thumb_frame);
  if (bio != NULL)
    url_close_buf (bio);
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
