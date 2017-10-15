/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/ogg_extractor.c
 * @brief plugin to support OGG files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <vorbis/vorbisfile.h>

/**
 * Bytes each ogg file must begin with (not used, but we might
 * choose to add this back in the future to improve performance
 * for non-ogg files).
 */
#define OGG_HEADER 0x4f676753


/**
 * Custom read function for ogg.
 *
 * @param ptr where to write the data
 * @param size number of bytes to read per member
 * @param nmemb number of members to read
 * @param datasource the 'struct EXTRACTOR_ExtractContext'
 * @return 0 on end-of-data, 0 with errno set to indicate read error
 */
static size_t
read_ogg (void *ptr, size_t size, size_t nmemb, void *datasource)
{
  struct EXTRACTOR_ExtractContext *ec = datasource;
  void *data;
  ssize_t ret;

  data = NULL;
  ret = ec->read (ec->cls,
		  &data,
		  size * nmemb);
  if (-1 == ret)
    return 0;
  if (0 == ret)
    {
      errno = 0;
      return 0;
    }
  memcpy (ptr, data, ret);
  errno = 0;
  return ret;
}


/**
 * Seek to a particular position in the file.
 *
 * @param datasource  the 'struct EXTRACTOR_ExtractContext'
 * @param offset where to seek
 * @param whence how to seek
 * @return -1 on error, new position on success
 */
static int
seek_ogg (void *datasource,
	  ogg_int64_t offset,
	  int whence)
{
  struct EXTRACTOR_ExtractContext *ec = datasource;
  int64_t new_position;

  new_position = ec->seek (ec->cls, (int64_t) offset, whence);
  return (long) new_position;
}


/**
 * Tell ogg where we are in the file
 *
 * @param datasource  the 'struct EXTRACTOR_ExtractContext'
 * @return
 */
static long
tell_ogg (void *datasource)
{
  struct EXTRACTOR_ExtractContext *ec = datasource;

  return (long) ec->seek (ec->cls,
			  0,
			  SEEK_CUR);
}



/**
 * Extract the associated meta data for a given label from vorbis.
 *
 * @param vc vorbis comment data
 * @param label label marking the desired entry
 * @return NULL on error, otherwise the meta data
 */
static char *
get_comment (vorbis_comment *vc,
	     const char *label)
{
  if (NULL == vc)
    return NULL;
  return vorbis_comment_query (vc, label, 0);
}


/**
 * Extract meta data from vorbis using the given LE type and value.
 *
 * @param t LE meta data type
 * @param s meta data to add
 */
#define ADD(t,s) do { if (0 != (ret = ec->proc (ec->cls, "ogg", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto FINISH; } while (0)


/**
 * Extract meta data from vorbis using the given LE type and label.
 *
 * @param t LE meta data type
 * @param d vorbis meta data label
 */
#define ADDG(t,d) do { m = get_comment (comments, d); if (NULL != m) ADD(t,m); } while (0)


/**
 * Main entry method for the 'application/ogg' extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_ogg_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  uint64_t fsize;
  ov_callbacks callbacks;
  OggVorbis_File vf;
  vorbis_comment *comments;
  int ret;
  const char *m;

  fsize = ec->get_size (ec->cls);
  if (fsize < 8)
    return;

  callbacks.read_func = &read_ogg;
  callbacks.seek_func = &seek_ogg;
  callbacks.close_func = NULL;
  callbacks.tell_func = &tell_ogg;
  ret = ov_open_callbacks (ec, &vf, NULL, 0, callbacks);
  if (0 != ret)
  {
    ov_clear (&vf);
    return;
  }
  comments = ov_comment (&vf, -1);
  if (NULL == comments)
  {
    ov_clear (&vf);
    return;
  }
  ADD (EXTRACTOR_METATYPE_MIMETYPE, "application/ogg");
  if ((comments->vendor != NULL) && (strlen (comments->vendor) > 0))
    ADD (EXTRACTOR_METATYPE_VENDOR, comments->vendor);
  ADDG (EXTRACTOR_METATYPE_TITLE, "title");
  ADDG (EXTRACTOR_METATYPE_ARTIST, "artist");
  ADDG (EXTRACTOR_METATYPE_PERFORMER, "performer");
  ADDG (EXTRACTOR_METATYPE_ALBUM, "album");
  ADDG (EXTRACTOR_METATYPE_TRACK_NUMBER, "tracknumber");
  ADDG (EXTRACTOR_METATYPE_DISC_NUMBER, "discnumber");
  ADDG (EXTRACTOR_METATYPE_CONTACT_INFORMATION, "contact");
  ADDG (EXTRACTOR_METATYPE_GENRE, "genre");
  ADDG (EXTRACTOR_METATYPE_CREATION_DATE, "date");
  ADDG (EXTRACTOR_METATYPE_COMMENT, "");
  ADDG (EXTRACTOR_METATYPE_LOCATION_SUBLOCATION, "location");
  ADDG (EXTRACTOR_METATYPE_DESCRIPTION, "description");
  ADDG (EXTRACTOR_METATYPE_ISRC, "isrc");
  ADDG (EXTRACTOR_METATYPE_ORGANIZATION, "organization");
  ADDG (EXTRACTOR_METATYPE_COPYRIGHT, "copyright");
  ADDG (EXTRACTOR_METATYPE_LICENSE, "license");
  ADDG (EXTRACTOR_METATYPE_SONG_VERSION, "version");
FINISH:
  ov_clear (&vf);
}

/* end of ogg_extractor.c */
