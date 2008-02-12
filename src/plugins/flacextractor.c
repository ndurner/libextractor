/*
     This file is part of libextractor.
     (C) 2007 Vidyut Samanta and Christian Grothoff

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

static struct EXTRACTOR_Keywords *
addKeyword (EXTRACTOR_KeywordType type,
            char *keyword, struct EXTRACTOR_Keywords *next)
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

struct Context {
  const char * data;
  size_t size;
  struct EXTRACTOR_Keywords * prev;
  size_t pos;
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
  char *text;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tmap[] = {
  {"TITLE", EXTRACTOR_TITLE},
  {"VERSION", EXTRACTOR_VERSION},
  {"ALBUM", EXTRACTOR_ALBUM},
  {"ARTIST", EXTRACTOR_ARTIST},
  {"PERFORMER", EXTRACTOR_INTERPRET},
  {"COPYRIGHT", EXTRACTOR_COPYRIGHT},
  {"LICENSE", EXTRACTOR_LICENSE},
  {"ORGANIZATION", EXTRACTOR_ORGANIZATION},
  {"DESCRIPTION", EXTRACTOR_DESCRIPTION},
  {"GENRE", EXTRACTOR_GENRE},
  {"DATE", EXTRACTOR_DATE},
  {"LOCATION", EXTRACTOR_LOCATION},
  {"CONTACT", EXTRACTOR_CONTACT},
  /*
    {"ISRC", EXTRACTOR_...},
    {"TRACKNUMBER", EXTRACTOR_...},
  */
  {NULL, 0},
};


static EXTRACTOR_KeywordList *
check(const char * type,
      unsigned int type_length,
      const char * value,
      unsigned int value_length,
      EXTRACTOR_KeywordList * prev)
{
  unsigned int i;
  i = 0;
  while (tmap[i].text != NULL) 
    {
      if ( (type_length == strlen(tmap[i].text)) &&
	   (0 == strncasecmp(tmap[i].text,
			     type,
			     type_length)) )
	return addKeyword(tmap[i].type,
			  strndup(value,
				  value_length),
			  prev);
      i++;
    }
  return prev;
}

static void 
flac_metadata(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) 
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
	ctx->prev = addKeyword(EXTRACTOR_FORMAT,
			       strdup(buf),
			       ctx->prev);
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
	    ctx->prev = check((const char*) entry->entry,
			      ilen,
			      eq,
			      len - ilen,
			      ctx->prev);		  
	  }
	break;
      }
    case FLAC__METADATA_TYPE_PICTURE:
      {
	FLAC__byte * data = metadata->data.picture.data;
	FLAC__uint32 length = metadata->data.picture.data_length;
	char * enc;
	
	enc = EXTRACTOR_binaryEncode(data, length);
	ctx->prev = addKeyword(EXTRACTOR_THUMBNAILS,
			       enc,
			       ctx->prev);	
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
  fprintf(stderr,
	  "Got error: %u\n", status);
}

/* mimetype = audio/flac */
struct EXTRACTOR_Keywords *
libextractor_flac_extract (const char *filename,
			   const char *data,
			   size_t size, struct EXTRACTOR_Keywords *prev)
{
  FLAC__StreamDecoder * decoder;
  struct Context le_cls;

  if (size < strlen(FLAC_HEADER) + sizeof (int))    
    return prev;    
  if (0 != memcmp(FLAC_HEADER,
		  data,
		  strlen(FLAC_HEADER)))
    return prev;
  decoder = FLAC__stream_decoder_new();
  if (NULL == decoder)
    return prev;
  FLAC__stream_decoder_set_md5_checking(decoder, false);
  FLAC__stream_decoder_set_metadata_ignore_all(decoder);
  if (false == FLAC__stream_decoder_set_metadata_respond_all(decoder))
    {
      FLAC__stream_decoder_delete(decoder);     
      return prev;
    }
  le_cls.prev = prev;
  le_cls.prev = prev;
  le_cls.size = size;
  le_cls.data = data;
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
      return le_cls.prev;
    }
  if (FLAC__stream_decoder_get_state(decoder) != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    {
      FLAC__stream_decoder_delete(decoder);     
      return le_cls.prev;
    }
  if (! FLAC__stream_decoder_process_until_end_of_metadata(decoder))
    {
      FLAC__stream_decoder_delete(decoder);     
      return le_cls.prev;
    }
  switch (FLAC__stream_decoder_get_state(decoder))
   {
   case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
   case FLAC__STREAM_DECODER_READ_METADATA:
   case FLAC__STREAM_DECODER_END_OF_STREAM:
   case FLAC__STREAM_DECODER_READ_FRAME:
     le_cls.prev = addKeyword(EXTRACTOR_MIMETYPE,
			      strdup("audio/flac"),
			      le_cls.prev);
     break;
   default:
     /* not so sure... */
     break;
   }
  FLAC__stream_decoder_finish (decoder); 
  FLAC__stream_decoder_delete(decoder);
  return le_cls.prev;
}
