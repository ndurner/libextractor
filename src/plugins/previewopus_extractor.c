/*
     This file is part of libextractor.
     Copyright (C) 2008, 2013 Bruno Cabral and Christian Grothoff

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
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */
/**
 * @file previewopus_extractor.c
 * @author Bruno Cabral
 * @author Christian Grothoff
 * @brief this extractor produces a binary encoded
 * audio snippet of music/video files using ffmpeg libs.
 *
 * Based on ffmpeg samples.
 *
 * Note that ffmpeg has a few issues:
 * (1) there are no recent official releases of the ffmpeg libs
 * (2) ffmpeg has a history of having security issues (parser is not robust)
 *
 *  So this plugin cannot be recommended for system with high security
 *requirements. 
 */
#include "platform.h"
#include "extractor.h"
#include <magic.h>

#if HAVE_LIBAVUTIL_AVUTIL_H
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>

#elif HAVE_FFMPEG_AVUTIL_H
#include <ffmpeg/avutil.h>
#include <ffmpeg/audio_fifo.h>
#include <ffmpeg/opt.h>
#include <ffmpeg/mathematics.h>
#endif
#if HAVE_LIBAVFORMAT_AVFORMAT_H
#include <libavformat/avformat.h>
#elif HAVE_FFMPEG_AVFORMAT_H
#include <ffmpeg/avformat.h>
#endif
#if HAVE_LIBAVCODEC_AVCODEC_H
#include <libavcodec/avcodec.h>
#elif HAVE_FFMPEG_AVCODEC_H
#include <ffmpeg/avcodec.h>
#endif
#if HAVE_LIBSWSCALE_SWSCALE_H
#include <libswscale/swscale.h>
#elif HAVE_FFMPEG_SWSCALE_H
#include <ffmpeg/swscale.h>
#endif

//TODO: Check for ffmpeg
#include <libavresample/avresample.h>







/**
 * Set to 1 to enable debug output.
 */ 
#define DEBUG 0

/**
 * Set to 1 to enable a output file for testing.
 */ 
#define OUTPUT_FILE 0



/**
 * Maximum size in bytes for the preview.
 */
#define MAX_SIZE (28*1024)

/**
 * HardLimit for file
 */
#define HARD_LIMIT_SIZE (50*1024)


/** The output bit rate in kbit/s */
#define OUTPUT_BIT_RATE 28000
/** The number of output channels */
#define OUTPUT_CHANNELS 2
/** The audio sample output format */
#define OUTPUT_SAMPLE_FORMAT AV_SAMPLE_FMT_S16


/** Our output buffer*/
static unsigned char *buffer;

/** Actual output buffer size */
static int totalSize;

/**
 * Convert an error code into a text message.
 * @param error Error code to be converted
 * @return Corresponding error text (not thread-safe)
 */
static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}


/**
 * Read callback.
 *
 * @param opaque the 'struct EXTRACTOR_ExtractContext'
 * @param buf where to write data
 * @param buf_size how many bytes to read
 * @return -1 on error (or for unknown file size)
 */
static int
read_cb (void *opaque,
	 uint8_t *buf,
	 int buf_size)
{
  struct EXTRACTOR_ExtractContext *ec = opaque;
  void *data;
  ssize_t ret;

  ret = ec->read (ec->cls, &data, buf_size);
  if (ret <= 0)
    return ret;
  memcpy (buf, data, ret);
  return ret;
}


/**
 * Seek callback.
 *
 * @param opaque the 'struct EXTRACTOR_ExtractContext'
 * @param offset where to seek
 * @param whence how to seek; AVSEEK_SIZE to return file size without seeking
 * @return -1 on error (or for unknown file size)
 */
static int64_t
seek_cb (void *opaque,
	 int64_t offset,
	 int whence)
{
  struct EXTRACTOR_ExtractContext *ec = opaque;

  if (AVSEEK_SIZE == whence)
    return ec->get_size (ec->cls);
  return ec->seek (ec->cls, offset, whence);
}


/**
 * write callback.
 *
 * @param opaque NULL
 * @param pBuffer to write
 * @param pBufferSize , amount to write
 * @return 0 on error
 */
static int writePacket(void *opaque, unsigned char *pBuffer, int pBufferSize) {

	int sizeToCopy = pBufferSize;
	if( (totalSize + pBufferSize) > HARD_LIMIT_SIZE)
		sizeToCopy = HARD_LIMIT_SIZE - totalSize;

    memcpy(buffer + totalSize, pBuffer, sizeToCopy);
	totalSize+= sizeToCopy;
	
	return sizeToCopy;
}


/**
 * Open an output file and the required encoder.
 * Also set some basic encoder parameters.
 * Some of these parameters are based on the input file's parameters.
 */
static int open_output_file(
                            AVCodecContext *input_codec_context,
                            AVFormatContext **output_format_context,
                            AVCodecContext **output_codec_context)
{
	AVStream *stream               = NULL;
    AVCodec *output_codec          = NULL;
	AVIOContext *io_ctx;
    int error;
	

	unsigned char *iob;

  if (NULL == (iob = av_malloc (16 * 1024)))
    return AVERROR_EXIT;
  if (NULL == (io_ctx = avio_alloc_context (iob, 16 * 1024,
					    AVIO_FLAG_WRITE, NULL, 
					   NULL,
					    &writePacket /* no writing */,
					    NULL)))
    {
      av_free (iob);
      return AVERROR_EXIT;
    }
  if (NULL == ((*output_format_context) = avformat_alloc_context ()))
    {
      av_free (io_ctx);
      return AVERROR_EXIT;
    }
  (*output_format_context)->pb = io_ctx;
  
    /** Guess the desired container format based on the file extension. */
    if (!((*output_format_context)->oformat = av_guess_format(NULL, "file.ogg",
                                                              NULL))) {
 #if DEBUG															  
        fprintf(stderr, "Could not find output file format\n");
#endif
        goto cleanup;
    }
	

    /** Find the encoder to be used by its name. */
    if (!(output_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS))) {
 #if DEBUG
        fprintf(stderr, "Could not find an OPUS encoder.\n");
#endif
        goto cleanup;
    }

    /** Create a new audio stream in the output file container. */
    if (!(stream = avformat_new_stream(*output_format_context, output_codec))) {
 #if DEBUG
        fprintf(stderr, "Could not create new stream\n");
#endif
        error = AVERROR(ENOMEM);
        goto cleanup;
    }

    /** Save the encoder context for easiert access later. */
    *output_codec_context = stream->codec;


    /**
     * Set the basic encoder parameters.
     * The input file's sample rate is used to avoid a sample rate conversion.
     */
    (*output_codec_context)->channels       = OUTPUT_CHANNELS;
    (*output_codec_context)->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    (*output_codec_context)->sample_rate    = 48000; //Opus need 48000
    (*output_codec_context)->sample_fmt     = AV_SAMPLE_FMT_S16;
    (*output_codec_context)->bit_rate       = OUTPUT_BIT_RATE;

	
    /** Open the encoder for the audio stream to use it later. */
    if ((error = avcodec_open2(*output_codec_context, output_codec, NULL)) < 0) {
 #if DEBUG
        fprintf(stderr, "Could not open output codec (error '%s')\n",
                get_error_text(error));
#endif
        goto cleanup;
    }

    return 0;

cleanup:
    return error < 0 ? error : AVERROR_EXIT;
}

/** Initialize one data packet for reading or writing. */
static void init_packet(AVPacket *packet)
{
    av_init_packet(packet);
    /** Set the packet data and size so that it is recognized as being empty. */
    packet->data = NULL;
    packet->size = 0;
}

/** Initialize one audio frame for reading from the input file */
static int init_input_frame(AVFrame **frame)
{
    if (!(*frame = avcodec_alloc_frame())) {
 #if DEBUG
        fprintf(stderr, "Could not allocate input frame\n");
#endif
        return AVERROR(ENOMEM);
    }
    return 0;
}

/**
 * Initialize the audio resampler based on the input and output codec settings.
 * If the input and output sample formats differ, a conversion is required
 * libavresample takes care of this, but requires initialization.
 */
static int init_resampler(AVCodecContext *input_codec_context,
                          AVCodecContext *output_codec_context,
                          AVAudioResampleContext  **resample_context)
{
    /**
     * Only initialize the resampler if it is necessary, i.e.,
     * if and only if the sample formats differ.
     */
    if (input_codec_context->sample_fmt != output_codec_context->sample_fmt ||
        input_codec_context->channels != output_codec_context->channels) {
        int error;
		
		      /** Create a resampler context for the conversion. */
       if (!(*resample_context = avresample_alloc_context())) {
	    #if DEBUG
           fprintf(stderr, "Could not allocate resample context\n");
		#endif
           return AVERROR(ENOMEM);
       }
	   

        /**
         * Set the conversion parameters.
         * Default channel layouts based on the number of channels
         * are assumed for simplicity (they are sometimes not detected
         * properly by the demuxer and/or decoder).
         */
       av_opt_set_int(*resample_context, "in_channel_layout",
                     av_get_default_channel_layout(input_codec_context->channels), 0);
      av_opt_set_int(*resample_context, "out_channel_layout",
                     av_get_default_channel_layout(output_codec_context->channels), 0);
      av_opt_set_int(*resample_context, "in_sample_rate",
                     input_codec_context->sample_rate, 0);
      av_opt_set_int(*resample_context, "out_sample_rate",
                     output_codec_context->sample_rate, 0);
      av_opt_set_int(*resample_context, "in_sample_fmt",
                     input_codec_context->sample_fmt, 0);
      av_opt_set_int(*resample_context, "out_sample_fmt",
                     output_codec_context->sample_fmt, 0);

        /** Open the resampler with the specified parameters. */
        if ((error = avresample_open(*resample_context)) < 0) {
		 #if DEBUG
            fprintf(stderr, "Could not open resample context\n");
		#endif
            avresample_free(resample_context);
            return error;
        }
    }
    return 0;
}

/** Initialize a FIFO buffer for the audio samples to be encoded. */
static int init_fifo(AVAudioFifo **fifo)
{
    /** Create the FIFO buffer based on the specified output sample format. */
    if (!(*fifo = av_audio_fifo_alloc(OUTPUT_SAMPLE_FORMAT, OUTPUT_CHANNELS, 1))) {
	 #if DEBUG
        fprintf(stderr, "Could not allocate FIFO\n");
	#endif
        return AVERROR(ENOMEM);
    }
    return 0;
}

/** Write the header of the output file container. */
static int write_output_file_header(AVFormatContext *output_format_context)
{
    int error;
    if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
	 #if DEBUG
        fprintf(stderr, "Could not write output file header (error '%s')\n",
                get_error_text(error));
	  #endif
        return error;
    }
    return 0;
}

/** Decode one audio frame from the input file. */
static int decode_audio_frame(AVFrame *frame,
                              AVFormatContext *input_format_context,
                              AVCodecContext *input_codec_context, int audio_stream_index, 
                              int *data_present, int *finished)
{
    /** Packet used for temporary storage. */
    AVPacket input_packet;
    int error;
    init_packet(&input_packet);

    /** Read one audio frame from the input file into a temporary packet. */
	while(1){
		if ((error = av_read_frame(input_format_context, &input_packet)) < 0) {
			/** If we are the the end of the file, flush the decoder below. */
			if (error == AVERROR_EOF){
			 #if DEBUG
				fprintf(stderr, "EOF in decode_audio\n");
			  #endif
				*finished = 1;
				}
			else {
			 #if DEBUG
				fprintf(stderr, "Could not read frame (error '%s')\n",
						get_error_text(error));
			 #endif
				return error;
			}
		}
		
		if(input_packet.stream_index == audio_stream_index)
			break;
	}

    /**
     * Decode the audio frame stored in the temporary packet.
     * The input audio stream decoder is used to do this.
     * If we are at the end of the file, pass an empty packet to the decoder
     * to flush it.
     */
    if ((error = avcodec_decode_audio4(input_codec_context, frame,
                                       data_present, &input_packet)) < 0) {
	#if DEBUG
        fprintf(stderr, "Could not decode frame (error '%s')\n",
                get_error_text(error));
	 #endif
        av_free_packet(&input_packet);
        return error;
    }

    /**
     * If the decoder has not been flushed completely, we are not finished,
     * so that this function has to be called again.
     */
    if (*finished && *data_present)
        *finished = 0;
    av_free_packet(&input_packet);
    return 0;
}

/**
 * Initialize a temporary storage for the specified number of audio samples.
 * The conversion requires temporary storage due to the different format.
 * The number of audio samples to be allocated is specified in frame_size.
 */
static int init_converted_samples(uint8_t ***converted_input_samples, int* out_linesize,
                                  AVCodecContext *output_codec_context,
                                  int frame_size)
{
    int error;

    /**
     * Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples = calloc(output_codec_context->channels,
                                            sizeof(**converted_input_samples)))) {
	 #if DEBUG
        fprintf(stderr, "Could not allocate converted input sample pointers\n");
	  #endif
        return AVERROR(ENOMEM);
    }

    /**
     * Allocate memory for the samples of all channels in one consecutive
     * block for convenience.
     */
    if ((error = av_samples_alloc(*converted_input_samples, out_linesize,
                                  output_codec_context->channels,
                                  frame_size,
                                  output_codec_context->sample_fmt, 0)) < 0) {
	#if DEBUG
        fprintf(stderr,
                "Could not allocate converted input samples (error '%s')\n",
                get_error_text(error));
	 #endif
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}

/**
 * Convert the input audio samples into the output sample format.
 * The conversion happens on a per-frame basis, the size of which is specified
 * by frame_size.
 */
static int convert_samples(uint8_t **input_data,
                           uint8_t **converted_data, const int in_sample, const int out_sample, const int out_linesize, 
                           AVAudioResampleContext  *resample_context)
{
    int error;

    /** Convert the samples using the resampler. */
   if ((error = avresample_convert(resample_context, converted_data, out_linesize,
                                   out_sample, input_data, 0, in_sample)) < 0) {
	 #if DEBUG
        fprintf(stderr, "Could not convert input samples (error '%s')\n",
                get_error_text(error));
	  #endif
        return error;
    }
	
	 
    /**
     * Perform a sanity check so that the number of converted samples is
     * not greater than the number of samples to be converted.
     * If the sample rates differ, this case has to be handled differently
     */
    if (avresample_available(resample_context)) {
	 #if DEBUG
        fprintf(stderr, "%i Converted samples left over\n",avresample_available(resample_context));
	 #endif
    }


    return 0;
}

/** Add converted input audio samples to the FIFO buffer for later processing. */
static int add_samples_to_fifo(AVAudioFifo *fifo,
                               uint8_t **converted_input_samples,
                               const int frame_size)
{
    int error;

    /**
     * Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples.
     */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
	 #if DEBUG
        fprintf(stderr, "Could not reallocate FIFO\n");
	 #endif
        return error;
    }

    /** Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
                            frame_size) < frame_size) {
	 #if DEBUG
        fprintf(stderr, "Could not write data to FIFO\n");
	 #endif
        return AVERROR_EXIT;
    }
    return 0;
}

/**
 * Read one audio frame from the input file, decodes, converts and stores
 * it in the FIFO buffer.
 */
static int read_decode_convert_and_store(AVAudioFifo *fifo,
                                         AVFormatContext *input_format_context,
                                         AVCodecContext *input_codec_context,
                                         AVCodecContext *output_codec_context,
                                         AVAudioResampleContext  *resampler_context, int audio_stream_index,
                                         int *finished)
{
    /** Temporary storage of the input samples of the frame read from the file. */
    AVFrame *input_frame = NULL;
    /** Temporary storage for the converted input samples. */
    uint8_t **converted_input_samples = NULL;
    int data_present;
    int ret = AVERROR_EXIT;

    /** Initialize temporary storage for one input frame. */
    if (init_input_frame(&input_frame)){
    #if DEBUG
		fprintf(stderr, "Failed at init frame\n");
	#endif
		goto cleanup;
		
		}
    /** Decode one frame worth of audio samples. */
    if (decode_audio_frame(input_frame, input_format_context,
                           input_codec_context, audio_stream_index,  &data_present,  finished)){
        #if DEBUG
		fprintf(stderr, "Failed at decode audio\n");
		#endif
		
		goto cleanup;
		
		}
    /**
     * If we are at the end of the file and there are no more samples
     * in the decoder which are delayed, we are actually finished.
     * This must not be treated as an error.
     */
    if (*finished && !data_present) {
        ret = 0;
		#if DEBUG
		fprintf(stderr, "Failed at finished or no data\n");
		#endif
        goto cleanup;
    }
    /** If there is decoded data, convert and store it */
    if (data_present) {
	int out_linesize;
	//FIX ME: I'm losing samples, but can't get it to work.
	 int out_samples = avresample_available(resampler_context) + avresample_get_delay(resampler_context) + input_frame->nb_samples;


		//fprintf(stderr, "Input nbsamples %i out_samples: %i \n",input_frame->nb_samples,out_samples);

        /** Initialize the temporary storage for the converted input samples. */
        if (init_converted_samples(&converted_input_samples, &out_linesize, output_codec_context, 
                                   out_samples)){
        #if DEBUG
		fprintf(stderr, "Failed at init_converted_samples\n");
		#endif
            goto cleanup;
			}

        /**
         * Convert the input samples to the desired output sample format.
         * This requires a temporary storage provided by converted_input_samples.
         */
        if (convert_samples(input_frame->extended_data, converted_input_samples,
                            input_frame->nb_samples, out_samples, out_linesize ,resampler_context)){
							
							
        #if DEBUG
		fprintf(stderr, "Failed at convert_samples, input frame %i \n",input_frame->nb_samples);
		#endif
            goto cleanup;
			}
        /** Add the converted input samples to the FIFO buffer for later processing. */
        if (add_samples_to_fifo(fifo, converted_input_samples,
                                out_samples)){
        #if DEBUG
		fprintf(stderr, "Failed at add_samples_to_fifo\n");
		#endif
            goto cleanup;
			}
        ret = 0;
    }
    ret = 0;

cleanup:
    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        free(converted_input_samples);
    }
    avcodec_free_frame(&input_frame);

    return ret;
}

/**
 * Initialize one input frame for writing to the output file.
 * The frame will be exactly frame_size samples large.
 */
static int init_output_frame(AVFrame **frame,
                             AVCodecContext *output_codec_context,
                             int frame_size)
{
    int error;

    /** Create a new frame to store the audio samples. */
    if (!(*frame = avcodec_alloc_frame())) {
        #if DEBUG
		fprintf(stderr, "Could not allocate output frame\n");
		#endif
        return AVERROR_EXIT;
    }

    /**
     * Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity.
     */
    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = output_codec_context->channel_layout;
    (*frame)->format         = output_codec_context->sample_fmt;
    (*frame)->sample_rate    = output_codec_context->sample_rate;

	
	
	  //fprintf(stderr, "%i %i  \n",frame_size , (*frame)->format,(*frame)->sample_rate); 
	
    /**
     * Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified.
     */
    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        #if DEBUG
		fprintf(stderr, "Could allocate output frame samples (error '%s')\n", get_error_text(error));
		#endif
        avcodec_free_frame(frame);
        return error;
    }

    return 0;
}

/** Encode one frame worth of audio to the output file. */
static int encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present)
{
    /** Packet used for temporary storage. */
    AVPacket output_packet;
    int error;
    init_packet(&output_packet);

    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    if ((error = avcodec_encode_audio2(output_codec_context, &output_packet,
                                       frame, data_present)) < 0) {
        #if DEBUG
		fprintf(stderr, "Could not encode frame (error '%s')\n",            
		get_error_text(error));
		#endif
        av_free_packet(&output_packet);
        return error;
    }

    /** Write one audio frame from the temporary packet to the output file. */
    if (*data_present) {
        if ((error = av_write_frame(output_format_context, &output_packet)) < 0) {
            #if DEBUG
			fprintf(stderr, "Could not write frame (error '%s')\n",
			get_error_text(error));
			#endif
                    
            av_free_packet(&output_packet);
            return error;
        }

        av_free_packet(&output_packet);
    }
	
    return 0;
}

/**
 * Load one audio frame from the FIFO buffer, encode and write it to the
 * output file.
 */
static int load_encode_and_write(AVAudioFifo *fifo,
                                 AVFormatContext *output_format_context,
                                 AVCodecContext *output_codec_context)
{
    /** Temporary storage of the output samples of the frame written to the file. */
    AVFrame *output_frame;
    /**
     * Use the maximum number of possible samples per frame.
     * If there is less than the maximum possible frame size in the FIFO
     * buffer use this number. Otherwise, use the maximum possible frame size
     */
    const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                                 output_codec_context->frame_size);
    int data_written;
	 
    /** Initialize temporary storage for one output frame. */
    if (init_output_frame(&output_frame, output_codec_context, frame_size))
        return AVERROR_EXIT;

    /**
     * Read as many samples from the FIFO buffer as required to fill the frame.
     * The samples are stored in the frame temporarily.
     */
    if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
        #if DEBUG
		fprintf(stderr, "Could not read data from FIFO\n");
		#endif
        avcodec_free_frame(&output_frame);
        return AVERROR_EXIT;
    }

    /** Encode one frame worth of audio samples. */
    if (encode_audio_frame(output_frame, output_format_context,
                           output_codec_context, &data_written)) {
        avcodec_free_frame(&output_frame);
        return AVERROR_EXIT;
    }
    avcodec_free_frame(&output_frame);
    return 0;
}
/** Write the trailer of the output file container. */
static int write_output_file_trailer(AVFormatContext *output_format_context)
{
    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        #if DEBUG
		fprintf(stderr, "Could not write output file trailer (error '%s')\n",    
		get_error_text(error));
		#endif
        return error;
    }
    return 0;
}

#define ENUM_CODEC_ID enum AVCodecID


/**
 * Perform the audio snippet extraction
 *
 * @param ec extraction context to use
 */
static void
extract_audio (struct EXTRACTOR_ExtractContext *ec)
{
  AVIOContext *io_ctx;
  struct AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  AVFormatContext *output_format_context = NULL;
  AVCodec *codec;
  AVDictionary *options;
  AVFrame *frame;
  AVCodecContext* output_codec_context = NULL;
  AVAudioResampleContext  *resample_context = NULL;
  AVAudioFifo *fifo = NULL;
	
	
  int audio_stream_index;
  int i;
  int err;
  int duration;
  unsigned char *iob;
  
  
  totalSize =0;
  
  if (NULL == (iob = av_malloc (16 * 1024)))
    return;
  if (NULL == (io_ctx = avio_alloc_context (iob, 16 * 1024,
					    0, ec, 
					    &read_cb,
					    NULL /* no writing */,
					    &seek_cb)))
    {
      av_free (iob);
      return;
    }
  if (NULL == (format_ctx = avformat_alloc_context ()))
    {
      av_free (io_ctx);
      return;
    }
  format_ctx->pb = io_ctx;
  options = NULL;
  if (0 != avformat_open_input (&format_ctx, "<no file>", NULL, &options))
    return;
  av_dict_free (&options);  
  if (0 > avformat_find_stream_info (format_ctx, NULL))
    {
 #if DEBUG
      fprintf (stderr,
               "Failed to read stream info\n");
#endif
      avformat_close_input (&format_ctx);
      av_free (io_ctx);
      return;
    }
  codec = NULL;
  codec_ctx = NULL;
  audio_stream_index = -1;
  for (i=0; i<format_ctx->nb_streams; i++)
    {
      codec_ctx = format_ctx->streams[i]->codec;
      if (AVMEDIA_TYPE_AUDIO != codec_ctx->codec_type)
        continue;
      if (NULL == (codec = avcodec_find_decoder (codec_ctx->codec_id)))
        continue;
      options = NULL;
      if (0 != (err = avcodec_open2 (codec_ctx, codec, &options)))
        {
          codec = NULL;
          continue;
        }
      av_dict_free (&options); 
      audio_stream_index = i;
      break;
    }
  if ( (-1 == audio_stream_index) ||
       (0 == codec_ctx->channels) )
    {
#if DEBUG
      fprintf (stderr,
               "No audio streams or no suitable codec found\n");
#endif
      if (NULL != codec)
        avcodec_close (codec_ctx);
      avformat_close_input (&format_ctx);
      av_free (io_ctx);
      return;
    }

  if (NULL == (frame = avcodec_alloc_frame ()))
    {
#if DEBUG
      fprintf (stderr,
               "Failed to allocate frame\n");
#endif
      avcodec_close (codec_ctx);
      avformat_close_input (&format_ctx);
      av_free (io_ctx);
      return;
    }
	
	
	if(!(buffer = malloc(HARD_LIMIT_SIZE)))
		goto cleanup;
	
	
	 /** Open the output file for writing. */
    if (open_output_file( codec_ctx,&output_format_context, &output_codec_context))
        goto cleanup;
    /** Initialize the resampler to be able to convert audio sample formats. */
    if (init_resampler(codec_ctx, output_codec_context,
                       &resample_context))
        goto cleanup;
    /** Initialize the FIFO buffer to store audio samples to be encoded. */
    if (init_fifo(&fifo))
        goto cleanup;
	
	    /** Write the header of the output file container. */
    if (write_output_file_header(output_format_context))
        goto cleanup;
	

  if (format_ctx->duration == AV_NOPTS_VALUE)
	{
	duration = -1;
#if DEBUG
    fprintf (stderr,
	     "Duration unknown\n");
#endif
	}
  else
  {
 #if DEBUG
	duration = format_ctx->duration;
    fprintf (stderr,
	     "Duration: %lld\n", 
	     format_ctx->duration);  
#endif		 
	}
	
	

  /* if duration is known, seek to first tried,
   * else use 10 sec into stream */
 
  if(-1 != duration)
	err = av_seek_frame (format_ctx, -1, (duration/3), 0);
  else
	err = av_seek_frame (format_ctx, -1, 10 * AV_TIME_BASE, 0);
  
  
  
  if (err >= 0)        
    avcodec_flush_buffers (codec_ctx);        


	/**
     * Loop as long as we have input samples to read or output samples
     * to write; abort as soon as we have neither.
     */
    while (1) {
        /** Use the encoder's desired frame size for processing. */
        const int output_frame_size = output_codec_context->frame_size;
        int finished                = 0;

        /**
         * Make sure that there is one frame worth of samples in the FIFO
         * buffer so that the encoder can do its work.
         * Since the decoder's and the encoder's frame size may differ, we
         * need to FIFO buffer to store as many frames worth of input samples
         * that they make up at least one frame worth of output samples.
         */
		 
        while ((av_audio_fifo_size(fifo) < output_frame_size)) {
            /**
             * Decode one frame worth of audio samples, convert it to the
             * output sample format and put it into the FIFO buffer.
             */

		 
            if (read_decode_convert_and_store(fifo, format_ctx,codec_ctx,
                                              output_codec_context,
                                              resample_context,audio_stream_index, &finished)){

                goto cleanup;
				
				}

            /**
             * If we are at the end of the input file, we continue
             * encoding the remaining audio samples to the output file.
             */
            if (finished)
                break;
        }

		/* Already over our limit*/
		if(totalSize >= MAX_SIZE)
			finished = 1;
		
		
        /**
         * If we have enough samples for the encoder, we encode them.
         * At the end of the file, we pass the remaining samples to
         * the encoder.
         */

        while (av_audio_fifo_size(fifo) >= output_frame_size ||
               (finished && av_audio_fifo_size(fifo) > 0)){
            /**
             * Take one frame worth of audio samples from the FIFO buffer,
             * encode it and write it to the output file.
             */

		 
            if (load_encode_and_write(fifo,output_format_context,  output_codec_context))
                goto cleanup;
			}
        /**
         * If we are at the end of the input file and have encoded
         * all remaining samples, we can exit this loop and finish.
         */
        if (finished) {
            int data_written;
            /** Flush the encoder as it may have delayed frames. */
            do {
                encode_audio_frame(NULL, output_format_context, output_codec_context, &data_written);
            } while (data_written);
            break;
        }
    }

    /** Write the trailer of the output file container. */
    if (write_output_file_trailer(output_format_context))
        goto cleanup;
		

    ec->proc (ec->cls,
		"previewopus",
		EXTRACTOR_METATYPE_AUDIO_PREVIEW,
		EXTRACTOR_METAFORMAT_BINARY,
		"audio/opus",
		buffer,
		totalSize);
		
		
#if OUTPUT_FILE
	FILE *f;
	f = fopen("example.opus", "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", "file");
        exit(1);
    }
	
	fwrite(buffer, 1, totalSize, f);
	fclose(f);

#endif


  cleanup:
  av_free (frame);
  
  free(buffer);
  
    if (fifo)
        av_audio_fifo_free(fifo);
	if (resample_context) {
		avresample_close(resample_context);
		avresample_free(&resample_context);
	}
    if (output_codec_context)
        avcodec_close(output_codec_context);

    if (codec_ctx)
        avcodec_close(codec_ctx);
    if (format_ctx)
        avformat_close_input(&format_ctx);
	av_free (io_ctx);
		
		
}

/**
 * Main method for the opus-preview plugin.
 *
 * @param ec extraction context
 */
void 
EXTRACTOR_previewopus_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  ssize_t iret;
  void *data;


  if (-1 == (iret = ec->read (ec->cls,
			      &data,
			      16 * 1024)))
    return;

  if (0 != ec->seek (ec->cls, 0, SEEK_SET))
    return;

  extract_audio (ec);
}



/**
 * Log callback.  Does nothing.
 *
 * @param ptr NULL
 * @param level log level
 * @param format format string
 * @param ap arguments for format
 */
static void 
previewopus_av_log_callback (void* ptr, 
				 int level,
				 const char *format,
				 va_list ap)
{
#if DEBUG
  vfprintf(stderr, format, ap);
#endif
}


/**
 * Initialize av-libs
 */
void __attribute__ ((constructor)) 
previewopus_lib_init (void)
{
  av_log_set_callback (&previewopus_av_log_callback);
  av_register_all ();

}


/**
 * Destructor for the library, cleans up.
 */
void __attribute__ ((destructor)) 
previewopus_ltdl_fini () 
{

}


/* end of previewopus_extractor.c */
