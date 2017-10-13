/*
     This file is part of libextractor.
     Copyright (C) 2007, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */

/**
 * @file plugins/flac_extractor.c
 * @brief plugin to support FLAC files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <FLAC/all.h>


/**
 * Bytes each FLAC file must begin with (not used, but we might
 * choose to add this back in the future to improve performance
 * for non-ogg files).
 */
#define FLAC_HEADER "fLaC"


/**
 * Custom read function for flac.
 *
 * @param decoder unused
 * @param buffer where to write the data
 * @param bytes how many bytes to read, set to how many bytes were read
 * @param client_data our 'struct EXTRACTOR_ExtractContxt*'
 * @return status code (error, end-of-file or success)
 */
static FLAC__StreamDecoderReadStatus
flac_read (const FLAC__StreamDecoder *decoder,
	   FLAC__byte buffer[],
	   size_t *bytes,
	   void *client_data)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;
  void *data;
  ssize_t ret;

  data = NULL;
  ret = ec->read (ec->cls,
		  &data,
		  *bytes);
  if (-1 == ret)
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  if (0 == ret)
    {
      errno = 0;
      return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
  memcpy (buffer, data, ret);
  *bytes = ret;
  errno = 0;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}


/**
 * Seek to a particular position in the file.
 *
 * @param decoder unused
 * @param absolute_byte_offset where to seek
 * @param client_data  the 'struct EXTRACTOR_ExtractContext'
 * @return status code (error or success)
 */
static FLAC__StreamDecoderSeekStatus
flac_seek (const FLAC__StreamDecoder *decoder,
	   FLAC__uint64 absolute_byte_offset,
	   void *client_data)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;

  if (absolute_byte_offset !=
      ec->seek (ec->cls, (int64_t) absolute_byte_offset, SEEK_SET))
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}


/**
 * Tell FLAC about our current position in the file.
 *
 * @param decoder unused
 * @param absolute_byte_offset location to store the current offset
 * @param client_data  the 'struct EXTRACTOR_ExtractContext'
 * @return status code (error or success)
 */
static FLAC__StreamDecoderTellStatus
flac_tell (const FLAC__StreamDecoder *decoder,
	   FLAC__uint64 *absolute_byte_offset,
	   void *client_data)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;

  *absolute_byte_offset = ec->seek (ec->cls,
				    0,
				    SEEK_CUR);
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}


/**
 * Tell FLAC the size of the file.
 *
 * @param decoder unused
 * @param stream_length where to store the file size
 * @param client_data  the 'struct EXTRACTOR_ExtractContext'
 * @return true at EOF, false if not
 */
static FLAC__StreamDecoderLengthStatus
flac_length (const FLAC__StreamDecoder *decoder,
	     FLAC__uint64 *stream_length,
	     void *client_data)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;

  *stream_length = ec->get_size (ec->cls);
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}


/**
 * Tell FLAC if we are at the end of the file.
 *
 * @param decoder unused
 * @param absolute_byte_offset location to store the current offset
 * @param client_data  the 'struct EXTRACTOR_ExtractContext'
 * @return true at EOF, false if not
 */
static FLAC__bool
flac_eof (const FLAC__StreamDecoder *decoder,
	  void *client_data)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;
  uint64_t size;
  int64_t seekresult;
  size = ec->get_size (ec->cls);
  seekresult = ec->seek (ec->cls, 0, SEEK_CUR);

  if (seekresult == -1)
    /* Treat seek error as error (not as indication of file not being
     * seekable).
     */
    return true;
  return (size == seekresult) ? true : false;
}


/**
 * FLAC wants to write.  Always succeeds but does nothing.
 *
 * @param decoder unused
 * @param frame unused
 * @param buffer unused
 * @param client_data  the 'struct EXTRACTOR_ExtractContext'
 * @return always claims success
 */
static FLAC__StreamDecoderWriteStatus
flac_write (const FLAC__StreamDecoder *decoder,
	    const FLAC__Frame *frame,
	    const FLAC__int32 *const buffer[],
	    void *client_data)
{
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


/**
 * A mapping from FLAC meta data strings to extractor types.
 */
struct Matches
{
  /**
   * FLAC Meta data description text.
   */
  const char *text;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * Mapping of FLAC meta data description texts to LE types.
 * NULL-terminated.
 */
static struct Matches tmap[] = {
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
  {NULL, 0}
};


/**
 * Give meta data to extractor.
 *
 * @param t type of the meta data
 * @param s meta data value in utf8 format
 */
#define ADD(t,s) do { ec->proc (ec->cls, "flac", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1); } while (0)


/**
 * Create 0-terminated version of n-character string.
 *
 * @param s input string (non 0-terminated)
 * @param n number of bytes in 's'
 * @return NULL on error, otherwise 0-terminated version of 's'
 */
static char *
xstrndup (const char *s,
	  size_t n)
{
  char * d;

  if (NULL == (d = malloc (n + 1)))
    return NULL;
  memcpy (d, s, n);
  d[n] = '\0';
  return d;
}


/**
 * Check if a mapping exists for the given meta data value
 * and if so give the result to LE.
 *
 * @param type type of the meta data according to FLAC
 * @param type_length number of bytes in 'type'
 * @param value meta data as UTF8 string (non 0-terminated)
 * @param value_length number of bytes in value
 * @param ec extractor context
 */
static void
check (const char *type,
       unsigned int type_length,
       const char *value,
       unsigned int value_length,
       struct EXTRACTOR_ExtractContext *ec)
{
  unsigned int i;
  char *tmp;

  for (i=0; NULL != tmap[i].text; i++)
    {
      if ( (type_length != strlen (tmap[i].text)) ||
	   (0 != strncasecmp (tmap[i].text,
			      type,
			      type_length)) )
	continue;
      if (NULL ==
	  (tmp = xstrndup (value,
			   value_length)))
	continue;
      ADD (tmap[i].type, tmp);
      free (tmp);
      break;
    }
}


/**
 * Function called whenever FLAC finds meta data.
 *
 * @param decoder unused
 * @param metadata meta data that was found
 * @param client_data  the 'struct EXTRACTOR_ExtractContext'
 */
static void
flac_metadata (const FLAC__StreamDecoder *decoder,
	       const FLAC__StreamMetadata *metadata,
	       void *client_data)
{
  struct EXTRACTOR_ExtractContext *ec = client_data;
  enum EXTRACTOR_MetaType type;
  const FLAC__StreamMetadata_VorbisComment * vc;
  unsigned int count;
  const FLAC__StreamMetadata_VorbisComment_Entry * entry;
  const char * eq;
  unsigned int len;
  unsigned int ilen;
  char buf[128];

  switch (metadata->type)
    {
    case FLAC__METADATA_TYPE_STREAMINFO:
      {
	snprintf (buf, sizeof (buf),
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
	vc = &metadata->data.vorbis_comment;
	count = vc->num_comments;
	while (count-- > 0)
	  {
	    entry = &vc->comments[count];
	    eq = (const char*) entry->entry;
            if (NULL == eq)
              break;
	    len = entry->length;
	    ilen = 0;
	    while ( ('=' != *eq) && ('\0' != *eq) &&
		    (ilen < len) )
	      {
		eq++;
		ilen++;
	      }
	    if ( ('=' != *eq) ||
		 (ilen == len) )
	      break;
	    eq++;
	    check ((const char*) entry->entry,
		   ilen,
		   eq,
		   len - ilen,
		   ec);
	  }
	break;
      }
    case FLAC__METADATA_TYPE_PICTURE:
      {
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
	ec->proc (ec->cls,
		  "flac",
		  type,
		  EXTRACTOR_METAFORMAT_BINARY,
		  metadata->data.picture.mime_type,
		  (const char*) metadata->data.picture.data,
		  metadata->data.picture.data_length);
	break;
      }
    default:
      break;
    }
}


/**
 * Function called whenever FLAC decoder has trouble.  Does nothing.
 *
 * @param decoder the decoder handle
 * @param status type of the error
 * @param client_data our 'struct EXTRACTOR_ExtractContext'
 */
static void
flac_error (const FLAC__StreamDecoder *decoder,
	    FLAC__StreamDecoderErrorStatus status,
	    void *client_data)
{
  /* ignore errors */
}


/**
 * Main entry method for the 'audio/flac' extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_flac_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  FLAC__StreamDecoder * decoder;

  if (NULL == (decoder = FLAC__stream_decoder_new ()))
    return;
  FLAC__stream_decoder_set_md5_checking (decoder, false);
  FLAC__stream_decoder_set_metadata_ignore_all (decoder);
  if (false == FLAC__stream_decoder_set_metadata_respond_all (decoder))
    {
      FLAC__stream_decoder_delete (decoder);
      return;
    }
  if (FLAC__STREAM_DECODER_INIT_STATUS_OK !=
      FLAC__stream_decoder_init_stream (decoder,
					&flac_read,
					&flac_seek,
					&flac_tell,
					&flac_length,
					&flac_eof,
					&flac_write,
					&flac_metadata,
					&flac_error,
					ec))
    {
      FLAC__stream_decoder_delete (decoder);
      return;
    }
  if (FLAC__STREAM_DECODER_SEARCH_FOR_METADATA != FLAC__stream_decoder_get_state(decoder))
    {
      FLAC__stream_decoder_delete (decoder);
      return;
    }
  if (! FLAC__stream_decoder_process_until_end_of_metadata(decoder))
    {
      FLAC__stream_decoder_delete (decoder);
      return;
    }
  switch (FLAC__stream_decoder_get_state (decoder))
    {
    case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
    case FLAC__STREAM_DECODER_READ_METADATA:
    case FLAC__STREAM_DECODER_END_OF_STREAM:
    case FLAC__STREAM_DECODER_READ_FRAME:
      break;
    default:
      /* not so sure... */
      break;
    }
  FLAC__stream_decoder_finish (decoder);
  FLAC__stream_decoder_delete (decoder);
}

/* end of flac_extractor.c */
