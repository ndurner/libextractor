/*
     This file is part of libextractor.
     (C) 2007, 2009 Vidyut Samanta and Christian Grothoff

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

#define FLAC_HEADER "fLaC"

#if HAVE_FLAC_ALL_H
#include <FLAC/all.h>
#else
#error You must install the libflac header files!
#endif


struct Context {
  const char * data;
  size_t size;
  size_t pos;
  EXTRACTOR_MetaDataProcessor proc;
  void *proc_cls;
  int ret;
};

static FLAC__StreamDecoderReadStatus
flac_read (const FLAC__StreamDecoder *decoder, 
	   FLAC__byte buffer[], 
	   size_t *bytes, 
	   void *client_data)
{
  struct Context * ctx = client_data;
  
  if (*bytes <= 0)
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  if (*bytes > ctx->size - ctx->pos)
    *bytes = ctx->size - ctx->pos;
  if (*bytes == 0)
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  memcpy(buffer,
	 &ctx->data[ctx->pos],
	 *bytes);
  ctx->pos += *bytes;  
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus 
flac_seek(const FLAC__StreamDecoder *decoder,
	  FLAC__uint64 absolute_byte_offset, void *client_data)
{
  struct Context * ctx = client_data;
  
  if (absolute_byte_offset > ctx->size)
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  ctx->pos = (size_t) absolute_byte_offset;
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus 
flac_tell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
  struct Context * ctx = client_data;
  
  *absolute_byte_offset = ctx->pos;
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;  
}

static FLAC__StreamDecoderLengthStatus 
flac_length(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
  struct Context * ctx = client_data;
  
  ctx->pos = *stream_length;
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool
flac_eof(const FLAC__StreamDecoder *decoder, void *client_data) 
{
  struct Context * ctx = client_data;

  return (ctx->pos == ctx->size) ? true : false;
}

static FLAC__StreamDecoderWriteStatus
flac_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data) 
{
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

typedef struct
{
  const char *text;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tmap[] = {
  {"TITLE", EXTRACTOR_METATYPE_TITLE},
  {"VERSION", EXTRACTOR_METATYPE_SONG_VERSION},
  {"ALBUM", EXTRACTOR_METATYPE_ALBUM},
  {"ARTIST", EXTRACTOR_METATYPE_ARTIST},
  {"PERFORMER", EXTRACTOR_METATYPE_PERFORMER},
  {"COPYRIGHT", EXTRACTOR_METATYPE_COPYRIGHT},
  {"LICENSE", EXTRACTOR_METATYPE_LICENSE},
  {"ORGANIZATION", EXTRACTOR_METATYPE_ORGANIZATION},
  {"DESCRIPTION", EXTRACTOR_METATYPE_DESCRIPTION},
  {"GENRE", EXTRACTOR_METATYPE_GENRE},
  {"DATE", EXTRACTOR_METATYPE_CREATION_DATE},
  {"LOCATION", EXTRACTOR_METATYPE_LOCATION_SUBLOCATION},
  {"CONTACT", EXTRACTOR_METATYPE_CONTACT_INFORMATION}, 
  {"TRACKNUMBER", EXTRACTOR_METATYPE_TRACK_NUMBER},
  {"ISRC", EXTRACTOR_METATYPE_ISRC},
  {NULL, 0},
};


static char * xstrndup(const char * s, size_t n){
  char * d;

  d= malloc(n+1);
  if (d == NULL)
    return NULL;
  memcpy(d,s,n);
  d[n]='\0';
  return d;
}

#define ADD(t,s) do { if (ctx->ret == 0) ctx->ret = ctx->proc (ctx->proc_cls, "flac", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1); } while (0)


static void
check(const char * type,
      unsigned int type_length,
      const char * value,
      unsigned int value_length,
      struct Context *ctx)
{
  unsigned int i;
  char *tmp;

  i = 0;
  while (tmap[i].text != NULL) 
    {
      if ( (type_length == strlen(tmap[i].text)) &&
	   (0 == strncasecmp(tmap[i].text,
			     type,
			     type_length)) )
	{
	  tmp = xstrndup(value,
			 value_length);
	  if (tmp != NULL)
	    {
	      ADD (tmap[i].type, tmp);
	      free (tmp);
	    }
	  break;
	}
      i++;
    }
}


static void 
flac_metadata(const FLAC__StreamDecoder *decoder,
	      const FLAC__StreamMetadata *metadata, 
	      void *client_data) 
{
  struct Context * ctx = client_data;
  
  switch (metadata->type)
    {
    case FLAC__METADATA_TYPE_STREAMINFO:
      {
	char buf[512];
	snprintf(buf, 512,
		_("%u Hz, %u channels"), 
		metadata->data.stream_info.sample_rate,
		metadata->data.stream_info.channels); 
	ADD (EXTRACTOR_METATYPE_RESOURCE_TYPE, buf);
	break;
      }
    case FLAC__METADATA_TYPE_APPLICATION:
      /* FIXME: could find out generator application here:
	 http://flac.sourceforge.net/api/structFLAC____StreamMetadata__Application.html and
	 http://flac.sourceforge.net/id.html 
      */
      break;
    case FLAC__METADATA_TYPE_VORBIS_COMMENT:
      {
	const FLAC__StreamMetadata_VorbisComment * vc = &metadata->data.vorbis_comment;
	unsigned int count = vc->num_comments;
	const FLAC__StreamMetadata_VorbisComment_Entry * entry;
	const char * eq;
	unsigned int len;
	unsigned int ilen;
	
	while (count-- > 0) 
	  {
	    entry = &vc->comments[count];
	    eq = (const char*) entry->entry;
	    len = entry->length;
	    ilen = 0;
	    while ( ('=' != *eq) && (*eq != '\0') &&
		    (ilen < len) )
	      {
		eq++;
		ilen++;
	      }
	    if ( ('=' != *eq) ||
		 (ilen == len) )
	      break;
	    eq++;
	    check((const char*) entry->entry,
		  ilen,
		  eq,
		  len - ilen,
		  ctx);		  
	  }
	break;
      }
    case FLAC__METADATA_TYPE_PICTURE:
      {
	if (ctx->ret == 0)
	  {
	    enum EXTRACTOR_MetaType type;
	    switch (metadata->data.picture.type)
	      {
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_OTHER:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON_STANDARD:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON:
		type = EXTRACTOR_METATYPE_THUMBNAIL; 
		break;
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_BACK_COVER:
		type = EXTRACTOR_METATYPE_COVER_PICTURE; 
		break;
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_LEAD_ARTIST:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_ARTIST:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_CONDUCTOR:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_BAND:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_COMPOSER:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_LYRICIST:
		type = EXTRACTOR_METATYPE_CONTRIBUTOR_PICTURE; 
		break;
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_RECORDING_LOCATION:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_DURING_RECORDING:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_DURING_PERFORMANCE:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_VIDEO_SCREEN_CAPTURE:
		type = EXTRACTOR_METATYPE_EVENT_PICTURE; 
		break;
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_BAND_LOGOTYPE:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_PUBLISHER_LOGOTYPE:
		type = EXTRACTOR_METATYPE_LOGO;
		break;
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_LEAFLET_PAGE:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_MEDIA:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_FISH:
	      case FLAC__STREAM_METADATA_PICTURE_TYPE_ILLUSTRATION:
	      default:
		type = EXTRACTOR_METATYPE_PICTURE;
		break;		
	      }
	    ctx->ret = ctx->proc (ctx->proc_cls, 
				  "flac", 
				  type,
				  EXTRACTOR_METAFORMAT_BINARY,
				  metadata->data.picture.mime_type,
				  (const char*) metadata->data.picture.data,
				  metadata->data.picture.data_length);
	  }
	break;
      }
    case FLAC__METADATA_TYPE_PADDING:
    case FLAC__METADATA_TYPE_SEEKTABLE:
    case FLAC__METADATA_TYPE_CUESHEET:
    case FLAC__METADATA_TYPE_UNDEFINED:
      break;
    }
}  

static void
flac_error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) 
{
#if 0
  fprintf(stderr,
	  "Got error: %u\n", status);
#endif
}

/* mimetype = audio/flac */
int 
EXTRACTOR_flac_extract (const char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  FLAC__StreamDecoder * decoder;
  struct Context le_cls;
  struct Context *ctx;

  if (size < strlen(FLAC_HEADER) + sizeof (int))    
    return 0;    
  if (0 != memcmp(FLAC_HEADER,
		  data,
		  strlen(FLAC_HEADER)))
    return 0;
  decoder = FLAC__stream_decoder_new();
  if (NULL == decoder)
    return 0;
  FLAC__stream_decoder_set_md5_checking(decoder, false);
  FLAC__stream_decoder_set_metadata_ignore_all(decoder);
  if (false == FLAC__stream_decoder_set_metadata_respond_all(decoder))
    {
      FLAC__stream_decoder_delete(decoder);     
      return 0;
    }
  le_cls.ret = 0;
  le_cls.size = size;
  le_cls.data = data;
  le_cls.proc = proc;
  le_cls.proc_cls = proc_cls;
  le_cls.pos = 0;
  if (FLAC__STREAM_DECODER_INIT_STATUS_OK !=
      FLAC__stream_decoder_init_stream(decoder,
				       &flac_read,
				       &flac_seek,
				       &flac_tell,
				       &flac_length,
				       &flac_eof,
				       &flac_write,
				       &flac_metadata,
				       &flac_error,
				       &le_cls))
    {
      FLAC__stream_decoder_delete(decoder);     
      return le_cls.ret;
    }
  if (FLAC__stream_decoder_get_state(decoder) != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    {
      FLAC__stream_decoder_delete(decoder);     
      return le_cls.ret;
    }
  if (! FLAC__stream_decoder_process_until_end_of_metadata(decoder))
    {
      FLAC__stream_decoder_delete(decoder);     
      return le_cls.ret;
    }
  switch (FLAC__stream_decoder_get_state(decoder))
    {
    case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
    case FLAC__STREAM_DECODER_READ_METADATA:
    case FLAC__STREAM_DECODER_END_OF_STREAM:
    case FLAC__STREAM_DECODER_READ_FRAME:
      ctx = &le_cls;
      ADD (EXTRACTOR_METATYPE_MIMETYPE, "audio/flac");
      break;
    default:
      /* not so sure... */
      break;
    }
  FLAC__stream_decoder_finish (decoder); 
  FLAC__stream_decoder_delete(decoder);
  return le_cls.ret;
}
