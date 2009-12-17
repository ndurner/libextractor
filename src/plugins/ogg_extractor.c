/*
     This file is part of libextractor.
     (C) 2002, 2003, 2009 Vidyut Samanta and Christian Grothoff

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

#define DEBUG_EXTRACT_OGG 0
#define OGG_HEADER 0x4f676753

#if HAVE_VORBIS_VORBISFILE_H
#include <vorbis/vorbisfile.h>
#else
#error You must install the libvorbis header files!
#endif

static char *
get_comment (vorbis_comment * vc, char *label)
{
  char *tag;
  if (vc && (tag = vorbis_comment_query (vc, label, 0)) != NULL)
    return tag;
  return NULL;
}

static size_t
readError (void *ptr, size_t size, size_t nmemb, void *datasource)
{
  return -1;
}

static int
seekError (void *datasource, int64_t offset, int whence)
{
  return -1;
}

static int
closeOk (void *datasource)
{
  return 0;
}

static long
tellError (void *datasource)
{
  return -1;
}

#define ADD(t,s) do { if (0 != (ret = proc (proc_cls, "ogg", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto FINISH; } while (0)

#define ADDG(t,d) do { m = get_comment (comments, d); if (m != NULL) ADD(t,m); } while (0)

/* mimetype = application/ogg */
int 
EXTRACTOR_ogg_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  OggVorbis_File vf;
  vorbis_comment *comments;
  ov_callbacks callbacks;
  int ret;
  const char *m;

  if (size < 2 * sizeof (int))
    return 0;
  if (OGG_HEADER != ntohl (*(int *) data))
    return 0;
  callbacks.read_func = &readError;
  callbacks.seek_func = &seekError;
  callbacks.close_func = &closeOk;
  callbacks.tell_func = &tellError;
  if (0 != ov_open_callbacks (NULL, &vf, (char*) data, size, callbacks))
    {
      ov_clear (&vf);
      return 0;
    }
  comments = ov_comment (&vf, -1);
  if (NULL == comments)
    {
      ov_clear (&vf);
      return 0;
    }
  ret = 0;
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
  return ret;
}
