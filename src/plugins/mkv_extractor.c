/*
     This file is part of libextractor.
     (C) 2004, 2005, 2006, 2009 Vidyut Samanta and Christian Grothoff

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
  * Made by Gabriel Peixoto
  * Using AVInfo 1.x code. Copyright (c) 2004 George Shuklin.
  *
  */

#include "platform.h"
#include "extractor.h"
#include <stdint.h>

#define ADD(s,t) do { if (0 != (ret = proc (proc_cls, "mkv", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto EXIT; } while (0)

/**
 * FIXME: document
 */
#define BUFFER_SIZE 0x4000

/**
 * FIXME: document
 */
#define MAX_STRING_SIZE 1024

/**
 * FIXME: document
 */
#define MAX_STREAMS 9

/**
 * FIXME: document
 */
enum
{
  MKV_Track_video = 1,
  MKV_Track_audio = 2,
  MKV_Track_subtitle = 3,
  MKV_Track_subtitle_orig = 0x11
};

/**
 * FIXME: document
 */
enum
{
  MKVID_OutputSamplingFrequency = 0x78B5,
  MKVID_FILE_BEGIN = 0x1A,
  MKVID_EBML = 0x1A45DFA3,
  MKVID_Segment = 0x18538067,
  MKVID_Info = 0x1549A966,
  MKVID_Tracks = 0x1654AE6B,
  MKVID_TrackEntry = 0xAE,
  MKVID_TrackType = 0x83,
  MKVID_DefaultDuration = 0x23E383,
  MKVID_Language = 0x22B59C,
  MKVID_CodecID = 0x86,
  MKVID_CodecPrivate = 0x63A2,
  MKVID_PixelWidth = 0xB0,
  MKVID_PixelHeight = 0xBA,
  MKVID_TimeCodeScale = 0x2AD7B1,
  MKVID_Duration = 0x4489,
  MKVID_Channels = 0x9F,
  MKVID_BitDepth = 0x6264,
  MKVID_SamplingFrequency = 0xB5,
  MKVID_Title = 0x7BA9,
  MKVID_Tags = 0x1254C367,
  MKVID_SeekHead = 0x114D9B74,
  MKVID_Video = 0xE0,
  MKVID_Audio = 0xE1,
  MKVID_CodecName = 0x258688,
  MKVID_DisplayHeight = 0x54BA,
  MKVID_DisplayWidth = 0x54B0
};


/**
 * FIXME: document 'flag', should 'temp'/'result' really be signed?
 *
 * @return 0 on error, otherwise number of bytes read from buffer
 */
static size_t
VINTparse (const unsigned char *buffer, size_t start, size_t end,
           int64_t * result, int flag)
{
  static const unsigned char mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };
  static const unsigned char imask[8] = { 0x7F, 0x3F, 0x1F, 0xF, 0x7, 0x3, 0x1, 00 };
  int vint_width;
  unsigned int c;
  int64_t temp;
  unsigned char tempc;

  if (end - start < 2)  
    return 0;                   /*ops */  
  
  vint_width = 0;
  for (c = 0; c < 8; c++)
    if (!(buffer[start] & mask[c]))
      vint_width++;
    else
      break;
  if ( (vint_width >= 8) || (vint_width + start + 1 >= end) )
    return 0;
  
  *result = 0;
  for (c = 0; c < vint_width; c++)
  {
    tempc = buffer[start + vint_width - c];
    temp = tempc << (c * 8);
    *result += temp;
  }
  if (flag)
    *result += (buffer[start] & imask[vint_width]) << (vint_width * 8);
  else
    *result += (buffer[start]) << (vint_width * 8);
  return vint_width + 1;
}


/**
 * FIXME: 'unsigned int' or 'uint32_t' for ID? 
 * FIXME: document arguments, return value...
 */
static int
elementRead (const unsigned char *buffer, size_t start, size_t end,
             unsigned int *id, int64_t * size)
{
  int64_t tempID;
  int64_t tempsize;
  size_t id_offset;
  size_t size_offset;

  tempID = 0;
  
  id_offset = VINTparse (buffer, start, end, &tempID, 0);
  if (!id_offset)
    return 0;
  size_offset = VINTparse (buffer, start + id_offset, end, &tempsize, 1);
  if (!size_offset)
    return 0;
  *id = (unsigned int) tempID;           /*id must be <4 and must fit in uint */
  *size = tempsize;
  return id_offset + size_offset;
}


/**
 * FIXME: signed or unsigned return value?
 */
static int64_t
getInt (const unsigned char *buffer, size_t start, size_t size)
{
  /*return a int [8-64], from buffer, Big Endian*/
  int64_t result;
  size_t c;

  result = 0;
  for (c = 1; c <= size; c++)
    result += buffer[start + c - 1] << (8 * (size - c));  
  return result;
}

static float
getFloat (const char *buffer, size_t start, size_t size)
{
  float result;
  char tmp[4];

  if (size != sizeof (float))
    return 0.0;
  if (size == sizeof (float))
  {
    tmp[0] = buffer[start + 3];
    tmp[1] = buffer[start + 2];
    tmp[2] = buffer[start + 1];
    tmp[3] = buffer[start];
    return *((float *) (tmp));
  }
  return result;
}

static const unsigned int MKV_Parse_list[] = { 
  /*Elements, containing requed information (sub-elements), see enum in mkv.h for values */
  MKVID_Segment,
  MKVID_Info,
  MKVID_Video,
  MKVID_Audio,
  MKVID_TrackEntry,
  MKVID_Tracks
};

static const char stream_type_letters[] = "?vat";      /*[0]-no, [1]-video,[2]-audio,[3]-text */


/* video/mkv */
int
EXTRACTOR_mkv_extract (const unsigned char *data, size_t size,
                       EXTRACTOR_MetaDataProcessor proc, void *proc_cls,
                       const char *options)
{
  int ret;
  char buffer[128];
  char temp[128];
  size_t p;                        /*pointer in buffer */
  int c, c2;                    /*counter in some loops */
  unsigned int eID;             /*element ID */
  int64_t eSize;                  /*Size of element content */
  int offs;
  int64_t timescale = 1000000;
  float duration = -1.0;
  int64_t DefaultDuration = 0;
  int TrackType = 0;
  int pvt_look_flag = 0;
  int curr_c = -1;
  int a_c = -1;
  int v_c = -1;
  int t_c = -1;
  int value_width = 0;
  int value_height = 0;
  int value = 0;
  size_t size1;
  const unsigned char *start;

  if (size > 32 * 1024)
    size1 = 32 * 1024;
  else
    size1 = size;
  start = memchr (data, MKVID_FILE_BEGIN, size1);
  if (NULL == start)
    return 0;
  p = start - data;
  
/*main loop*/
  ret = 0;
  do
  {
    offs = elementRead (data, p, size, &eID, &eSize);
    p += offs;
    if (!offs || p >= size)
      break;
    for (c = 0; c < sizeof (MKV_Parse_list) / sizeof (*MKV_Parse_list); c++)
      if (MKV_Parse_list[c] == eID)
      {
        break;
      }
    if (c < sizeof (MKV_Parse_list) / sizeof (*MKV_Parse_list))
      continue;
    if (p + eSize > size)
      break;                    /*TODO - add (if requied) suckup from file to data */
    if (eSize == 4 || eSize == 8 || eSize == 1 || eSize == 2)
      value = (int) getInt (data, p, eSize);

    switch (eID)
    {
    case MKVID_TrackType:      /*detect a stream type (video/audio/text) */
      TrackType = value;
      pvt_look_flag = 0;
      switch (TrackType)
      {
      case MKV_Track_video:
        v_c++;
        if (v_c > MAX_STREAMS)
          v_c = MAX_STREAMS;
	if (duration != -1.0)
	{
	  /* FIXME: duration might still be -1 here, defer until
	     end of function & check if duration is != -1 */
	  snprintf (buffer,
		    sizeof(buffer),
		    "%u s (video)",
		    (int) (duration / 1e+9 * (float) timescale));
	  ADD (buffer, EXTRACTOR_METATYPE_DURATION);
	}
        curr_c = v_c;
        break;
      case MKV_Track_audio:
        a_c++;
        if (a_c > MAX_STREAMS)
          a_c = MAX_STREAMS;
	if (duration != -1.0)
	{
	  /* FIXME: duration might still be -1 here, defer until
	     end of function & check if duration is != -1 */
	  snprintf (buffer,
		    sizeof (buffer),
		    "%u s (audio)",
		    (unsigned int) (duration / 1e+9 * (float) timescale));
	  ADD (buffer, EXTRACTOR_METATYPE_DURATION);
	}
        curr_c = a_c;
        break;
      case MKV_Track_subtitle_orig:
        t_c++;
        TrackType = MKV_Track_subtitle; /*for normal use in lang array */
        if (t_c > MAX_STREAMS)
          t_c = MAX_STREAMS;
        curr_c = t_c;
        break;
      }
      break;
    case MKVID_DefaultDuration:        /*fps detection */
      if (TrackType == MKV_Track_video && v_c >= 0)
      {
        DefaultDuration = value;
        if (DefaultDuration > 100)
        {
	  /* FIXME: integrate with codec name/id into 'METATYPE_FORMAT' */
          snprintf (buffer, 
		    sizeof (buffer),
		    "fps: %u", 1000000000 / DefaultDuration);
          ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
        }
      }
      break;
    case MKVID_Language:       /*stream language */
      if (curr_c >= 0 && TrackType < 4 && eSize < MAX_STRING_SIZE)
	  {
	          /* FIXME: why first temp, then buffer? */
		  snprintf (buffer, 
			    sizeof (buffer),
			    "%.*s",
			    (int) eSize,
			    data + p);
		  ADD (buffer, EXTRACTOR_METATYPE_LANGUAGE);
	  }
      break;
    case MKVID_CodecName:      /*passtrough */
    case MKVID_CodecID:        /*codec detection (if V_MS/VFW/FOURCC - set a fourcc code, else fill a vcodecs value) */
      if (curr_c >= 0 && TrackType < 4 && eSize < MAX_STRING_SIZE)
      {
	if (!strncmp (data + p, "V_MS/VFW/FOURCC", eSize))
          pvt_look_flag = 1;
        snprintf (buffer,
		  sizeof (buffer),
		  "codec: %.*s", 
		  (int) eSize,
		  data + p);
        ADD (buffer, 
	     EXTRACTOR_METATYPE_FORMAT);
      }
      break;
    case MKVID_CodecPrivate:
      if (pvt_look_flag && v_c >= 0 && eSize >= 24)
      {                         /*CodecPrivate contains a BITMAPINFOHEADER structure due CodecID==V_MS/VFW/FOURCC */
        pvt_look_flag = 0;
		//TODO
		/*
		video[v_c][V_cc]=(buffer[p+16]<<24)+(buffer[p+17]<<16)+(buffer[p+18]<<8)+buffer[p+19];
        if (codec[v_c][MKV_Track_video])
        {
          free (codec[v_c][MKV_Track_video]);
          codec[v_c][MKV_Track_video] = NULL;
        }
		*/
      }
      break;
    case MKVID_PixelWidth:     /*pasthough *//*bug with aspect differ from 1:1 */
    case MKVID_DisplayWidth:
	  value_width = value;
      break;
    case MKVID_PixelHeight:    /*pasthough */
    case MKVID_DisplayHeight:
      value_height = value;
      break;
    case MKVID_TimeCodeScale:
      timescale = getInt (data, p, eSize);
      snprintf (buffer, 
		sizeof (buffer),
		"TimeScale: %u", timescale);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
    case MKVID_Duration:
      duration = getFloat (data, p, eSize);
      snprintf (buffer, 
		sizeof (buffer),
		"%f ms", duration);
      ADD (buffer, EXTRACTOR_METATYPE_DURATION);
      break;
    case MKVID_Channels:
      snprintf (buffer,
		sizeof (buffer),
		"channels: %u", value);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
    case MKVID_BitDepth:
      snprintf (buffer,
		sizeof (buffer),
		"BitDepth: %u", value);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
    case MKVID_OutputSamplingFrequency:        /*pasthough */
    case MKVID_SamplingFrequency:
      /* FIXME: the resulting value seems wrong... Unit? */
      snprintf (buffer,
		sizeof (buffer),
		"Sampling Frequency: %u", value);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
      break;
    case MKVID_Title:
      if (eSize > MAX_STRING_SIZE)
        break;
      strncpy(temp,data+p,eSize);
      temp[eSize] = '\0';
      ADD (temp, EXTRACTOR_METATYPE_TITLE);
      break;
/*TODO			case MKVID_Tags:*/
    }
    p += eSize;                 /*skip unknown or uninteresting */
  }
  while (1);
  if ( (value_width != 0) && (value_height != 0) )    
    snprintf (buffer,
	      sizeof(buffer),
	      "%ux%u", 
	      value_width, value_height);
  ADD (buffer, EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
  
EXIT:

  return ret;

}
