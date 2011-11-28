/*
     This file is part of libextractor.
     (C) 2002, 2003, 2011 Vidyut Samanta and Christian Grothoff

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

/*
 * This file is based on demux_asf from the xine project (copyright follows).
 *
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: asfextractor.c,v 1.6 2004/10/05 20:02:08 grothoff Exp $
 *
 * demultiplexer for asf streams
 *
 * based on ffmpeg's
 * ASF compatible encoder and decoder.
 * Copyright (c) 2000, 2001 Gerard Lantau.
 *
 * GUID list from avifile
 * some other ideas from MPlayer
 */

#include "platform.h"
#include "extractor.h"
#include "convert.h"
#include <stdint.h>

#define DEMUX_FINISHED 0
#define DEMUX_START 1


/*
 * define asf GUIDs (list from avifile)
 */
#define GUID_ERROR                              0
#define GUID_ASF_HEADER                         1
#define GUID_ASF_DATA                           2
#define GUID_ASF_SIMPLE_INDEX                   3
#define GUID_ASF_FILE_PROPERTIES                4
#define GUID_ASF_STREAM_PROPERTIES              5
#define GUID_ASF_STREAM_BITRATE_PROPERTIES      6
#define GUID_ASF_CONTENT_DESCRIPTION            7
#define GUID_ASF_EXTENDED_CONTENT_ENCRYPTION    8
#define GUID_ASF_SCRIPT_COMMAND                 9
#define GUID_ASF_MARKER                        10
#define GUID_ASF_HEADER_EXTENSION              11
#define GUID_ASF_BITRATE_MUTUAL_EXCLUSION      12
#define GUID_ASF_CODEC_LIST                    13
#define GUID_ASF_EXTENDED_CONTENT_DESCRIPTION  14
#define GUID_ASF_ERROR_CORRECTION              15
#define GUID_ASF_PADDING                       16
#define GUID_ASF_AUDIO_MEDIA                   17
#define GUID_ASF_VIDEO_MEDIA                   18
#define GUID_ASF_COMMAND_MEDIA                 19
#define GUID_ASF_NO_ERROR_CORRECTION           20
#define GUID_ASF_AUDIO_SPREAD                  21
#define GUID_ASF_MUTEX_BITRATE                 22
#define GUID_ASF_MUTEX_UKNOWN                  23
#define GUID_ASF_RESERVED_1                    24
#define GUID_ASF_RESERVED_SCRIPT_COMMNAND      25
#define GUID_ASF_RESERVED_MARKER               26
#define GUID_ASF_AUDIO_CONCEAL_NONE            27
#define GUID_ASF_CODEC_COMMENT1_HEADER         28
#define GUID_ASF_2_0_HEADER                    29

#define GUID_END                               30


typedef struct
{
  uint32_t v1;
  uint16_t v2;
  uint16_t v3;
  uint8_t v4[8];
} LE_GUID;


static const struct
{
  const char *name;
  const LE_GUID guid;
} guids[] =
{
  {
    "error",
    {
  0x0,}},
    /* base ASF objects */
  {
    "header",
    {
      0x75b22630, 0x668e, 0x11cf,
      {
  0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}}},
  {
    "data",
    {
      0x75b22636, 0x668e, 0x11cf,
      {
  0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}}},
  {
    "simple index",
    {
      0x33000890, 0xe5b1, 0x11cf,
      {
  0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb}}},
    /* header ASF objects */
  {
    "file properties",
    {
      0x8cabdca1, 0xa947, 0x11cf,
      {
  0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}}},
  {
    "stream header",
    {
      0xb7dc0791, 0xa9b7, 0x11cf,
      {
  0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}}},
  {
    "stream bitrate properties",        /* (http://get.to/sdp) */
    {
      0x7bf875ce, 0x468d, 0x11d1,
      {
  0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2}}},
  {
    "content description",
    {
      0x75b22633, 0x668e, 0x11cf,
      {
  0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}}},
  {
    "extended content encryption",
    {
      0x298ae614, 0x2622, 0x4c17,
      {
  0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c}}},
  {
    "script command",
    {
      0x1efb1a30, 0x0b62, 0x11d0,
      {
  0xa3, 0x9b, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6}}},
  {
    "marker",
    {
      0xf487cd01, 0xa951, 0x11cf,
      {
  0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}}},
  {
    "header extension",
    {
      0x5fbf03b5, 0xa92e, 0x11cf,
      {
  0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}}},
  {
    "bitrate mutual exclusion",
    {
      0xd6e229dc, 0x35da, 0x11d1,
      {
  0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe}}},
  {
    "codec list",
    {
      0x86d15240, 0x311d, 0x11d0,
      {
  0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6}}},
  {
    "extended content description",
    {
      0xd2d0a440, 0xe307, 0x11d2,
      {
  0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50}}},
  {
    "error correction",
    {
      0x75b22635, 0x668e, 0x11cf,
      {
  0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}}},
  {
    "padding",
    {
      0x1806d474, 0xcadf, 0x4509,
      {
  0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8}}},
    /* stream properties object stream type */
  {
    "audio media",
    {
      0xf8699e40, 0x5b4d, 0x11cf,
      {
  0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b}}},
  {
    "video media",
    {
      0xbc19efc0, 0x5b4d, 0x11cf,
      {
  0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b}}},
  {
    "command media",
    {
      0x59dacfc0, 0x59e6, 0x11d0,
      {
  0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6}}},
    /* stream properties object error correction */
  {
    "no error correction",
    {
      0x20fb5700, 0x5b55, 0x11cf,
      {
  0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b}}},
  {
    "audio spread",
    {
      0xbfc3cd50, 0x618f, 0x11cf,
      {
  0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20}}},
    /* mutual exclusion object exlusion type */
  {
    "mutex bitrate",
    {
      0xd6e22a01, 0x35da, 0x11d1,
      {
  0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe}}},
  {
    "mutex unknown",
    {
      0xd6e22a02, 0x35da, 0x11d1,
      {
  0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe}}},
    /* header extension */
  {
    "reserved_1",
    {
      0xabd3d211, 0xa9ba, 0x11cf,
      {
  0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}}},
    /* script command */
  {
    "reserved script command",
    {
      0x4B1ACBE3, 0x100B, 0x11D0,
      {
  0xA3, 0x9B, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}}},
    /* marker object */
  {
    "reserved marker",
    {
      0x4CFEDB20, 0x75F6, 0x11CF,
      {
  0x9C, 0x0F, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB}}},
    /* various */
    /* Already defined (reserved_1)
       { "head2",
       { 0xabd3d211, 0xa9ba, 0x11cf, { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },
     */
  {
    "audio conceal none",
    {
      0x49f1a440, 0x4ece, 0x11d0,
      {
  0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6}}},
  {
    "codec comment1 header",
    {
      0x86d15241, 0x311d, 0x11d0,
      {
  0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6}}},
  {
    "asf 2.0 header",
    {
      0xd6e229d1, 0x35da, 0x11d1,
      {
0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe}}},};


struct demux_asf_s
{
  /* pointer to the stream data */
  const char *input;
  /* current position in stream */
  size_t inputPos;

  size_t inputLen;

  uint32_t length;

  int status;

  char *title;
  char *author;
  char *copyright;
  char *comment;
  char *rating;
};

static int
readBuf (struct demux_asf_s * this, void *buf, int len)
{
  int min;

  min = len;
  if (this->inputLen - this->inputPos < min)
    min = this->inputLen - this->inputPos;
  memcpy (buf, &this->input[this->inputPos], min);
  this->inputPos += min;
  return min;
}

static uint8_t
get_byte (struct demux_asf_s * this)
{
  uint8_t buf;
  int i;

  i = readBuf (this, &buf, 1);
  if (i != 1)
    this->status = DEMUX_FINISHED;
  return buf;
}

static uint16_t
get_le16 (struct demux_asf_s * this)
{
  uint8_t buf[2];
  int i;

  i = readBuf (this, buf, 2);
  if (i != 2)
    this->status = DEMUX_FINISHED;
  return buf[0] | (buf[1] << 8);
}

static uint32_t
get_le32 (struct demux_asf_s * this)
{
  uint8_t buf[4];
  int i;

  i = readBuf (this, buf, 4);
  if (i != 4)
    this->status = DEMUX_FINISHED;
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static uint64_t
get_le64 (struct demux_asf_s * this)
{
  uint8_t buf[8];
  int i;

  i = readBuf (this, buf, 8);
  if (i != 8)
    this->status = DEMUX_FINISHED;
  return (uint64_t) buf[0]
    | ((uint64_t) buf[1] << 8)
    | ((uint64_t) buf[2] << 16)
    | ((uint64_t) buf[3] << 24)
    | ((uint64_t) buf[4] << 32)
    | ((uint64_t) buf[5] << 40)
    | ((uint64_t) buf[6] << 48) | ((uint64_t) buf[7] << 54);
}

static int
get_guid (struct demux_asf_s * this)
{
  int i;
  LE_GUID g;

  g.v1 = get_le32 (this);
  g.v2 = get_le16 (this);
  g.v3 = get_le16 (this);
  for (i = 0; i < 8; i++)
    g.v4[i] = get_byte (this);
  if (this->status == DEMUX_FINISHED)
    return GUID_ERROR;
  for (i = 1; i < GUID_END; i++)
    if (!memcmp (&g, &guids[i].guid, sizeof (LE_GUID)))
      return i;

  return GUID_ERROR;
}


static int
asf_read_header (struct demux_asf_s * this)
{
  int guid;
  uint64_t gsize;
  uint16_t len1, len2, len3, len4, len5;

  guid = get_guid (this);
  if (guid != GUID_ASF_HEADER)
    return 0;
  get_le64 (this); /* object size */
  get_le32 (this); /* number of header objects */
  get_byte (this); /* reserved 1 */
  get_byte (this); /* reserved 2 */		    
  while (this->status != DEMUX_FINISHED)
    {
      guid = get_guid (this); /* object ID */
      gsize = get_le64 (this); /* object size */
      if (gsize < 24)
        goto fail;
      switch (guid)
        {
        case GUID_ASF_FILE_PROPERTIES:
	  guid = get_guid (this); /* file ID */
	  get_le64 (this);    /* file size */
	  get_le64 (this);    /* creation date */
	  get_le64 (this);    /* nb_packets */
	  this->length = get_le64 (this); /* play duration in 100 ns units */
	  get_le64 (this); /* send duration */
	  get_le64 (this); /* preroll */
	  get_le32 (this);    /* flags */
	  get_le32 (this);    /* min size */
	  get_le32 (this);        /* max size */
	  get_le32 (this);    /* max bitrate */          
          break;
        case GUID_ASF_DATA:
          goto headers_ok;
          break;
        case GUID_ASF_CONTENT_DESCRIPTION:
	  len1 = get_le16 (this);
	  len2 = get_le16 (this);
	  len3 = get_le16 (this);
	  len4 = get_le16 (this);
	  len5 = get_le16 (this);
	  this->title = EXTRACTOR_common_convert_to_utf8 (&this->input[this->inputPos],
							  len1,
							  "UTF-16");
	  this->inputPos += len1;
	  this->author = EXTRACTOR_common_convert_to_utf8 (&this->input[this->inputPos],
							   len2,
							   "UTF-16");
	  this->inputPos += len2;
	  this->copyright = EXTRACTOR_common_convert_to_utf8 (&this->input[this->inputPos],
							      len3,
							      "UTF-16");
	  this->inputPos += len3;
	  this->comment = EXTRACTOR_common_convert_to_utf8 (&this->input[this->inputPos],
							    len4,
							    "UTF-16");
	  this->inputPos += len4;
	  this->rating = EXTRACTOR_common_convert_to_utf8 (&this->input[this->inputPos],
							   len5,
							   "UTF-16");
	  this->inputPos += len5;
          break;
        default:
          this->inputPos += gsize - 24;
        }
    }

headers_ok:
  this->inputPos += sizeof (LE_GUID) + 10;
  return 1;
fail:
  return 0;
}


/* mimetypes:
   video/x-ms-asf: asf: ASF stream;
   video/x-ms-wmv: wmv: Windows Media Video;
   video/x-ms-wma: wma: Windows Media Audio;
   application/vnd.ms-asf: asf: ASF stream;
   application/x-mplayer2: asf,asx,asp: mplayer2;
   video/x-ms-asf-plugin: asf,asx,asp: mms animation;
   video/x-ms-wvx: wvx: wmv metafile;
   video/x-ms-wax: wva: wma metafile; */

/* mimetype = application/applefile */
int 
EXTRACTOR_asf_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  struct demux_asf_s this;
  size_t slen;
  char duration_str[30];
  int ret;

  memset (&this, 0, sizeof (struct demux_asf_s));
  this.input = data;
  this.inputLen = size;
  this.status = DEMUX_START;
  ret = 0;
  if (1 == asf_read_header (&this))
  {
    snprintf (duration_str, 
	      sizeof (duration_str),
	      "%llu ms", (unsigned long long) (this.length / 10000LL));
    if ( ( (this.title != NULL) &&
	   (0 < (slen = strlen(this.title))) &&
	   (0 != proc (proc_cls, 
		       "asf",
		       EXTRACTOR_METATYPE_TITLE,
		       EXTRACTOR_METAFORMAT_C_STRING,
		       "text/plain",
		       this.title,
		       slen + 1)) ) ||
	 ( (this.author != NULL) &&
	   (0 < (slen = strlen(this.author))) &&
	   (0 != proc (proc_cls, 
		       "asf",
		       EXTRACTOR_METATYPE_AUTHOR_NAME,
		       EXTRACTOR_METAFORMAT_C_STRING,
		       "text/plain",
		       this.author,
		       slen + 1)) ) ||
	 ( (this.comment != NULL)  &&
	   (0 < (slen = strlen(this.comment))) &&
	   (0 != proc (proc_cls, 
		       "asf",
		       EXTRACTOR_METATYPE_COMMENT,
		       EXTRACTOR_METAFORMAT_C_STRING,
		       "text/plain",
		       this.comment,
		       slen + 1)) ) ||
	 ( (this.copyright != NULL) &&
	   (0 < (slen = strlen(this.copyright))) &&
	   (0 != proc (proc_cls, 
		       "asf",
		       EXTRACTOR_METATYPE_COPYRIGHT,
		       EXTRACTOR_METAFORMAT_C_STRING,
		       "text/plain",
		       this.copyright,
		       slen + 1)) ) ||
	 (0 != proc (proc_cls, 
		     "asf",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_C_STRING,
		     "text/plain",
		     "video/x-ms-asf",
		     strlen("video/x-ms-asf") + 1)) ||
	 (0 != proc (proc_cls, 
		     "asf",
		     EXTRACTOR_METATYPE_DURATION,
		     EXTRACTOR_METAFORMAT_C_STRING,
		     "text/plain",
		     duration_str,
		     strlen(duration_str) + 1)) ) 
      ret = 1;	
  }
  free (this.title);
  free (this.author);
  free (this.copyright);
  free (this.comment);
  free (this.rating);
  return ret;
}

/*  end of asf_extractor.c */
