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


#define ADD(s,t) do { if (0 != (ret = proc (proc_cls, "mkv", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto EXIT; } while (0)

#define BUFFER_SIZE 0x4000
#define MAX_STRING_SIZE 1024
#define MAX_STREAMS 9
typedef long long int64;
enum
{
  MKV_Track_video = 1,
  MKV_Track_audio = 2,
  MKV_Track_subtitle = 3,
  MKV_Track_subtitle_orig = 0x11
};
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


static int
VINTparse (const unsigned char *buffer, const int start, const int end,
           int64 * result, const int flag)
{
  unsigned const char mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };
  unsigned const char imask[8] = { 0x7F, 0x3F, 0x1F, 0xF, 0x7, 0x3, 0x1, 00 };
  int VINT_WIDTH;
  int c;

  VINT_WIDTH = 0;
  int64 temp;
  unsigned char tempc;
  *result = 0;

  if (end - start < 2)
  {
    return 0;                   /*ops */
  }
  
  for (c = 0; c < 8; c++)
    if (!(buffer[start] & mask[c]))
      VINT_WIDTH++;
    else
      break;
  if (VINT_WIDTH >= 8 || VINT_WIDTH + start + 1 >= end)
  {
    return 0;
  }
  
  for (c = 0; c < VINT_WIDTH; c++)
  {
    tempc = buffer[start + VINT_WIDTH - c];
    temp = tempc << (c * 8);
    *result += temp;
  }

  if (flag)
    *result += (buffer[start] & imask[VINT_WIDTH]) << (VINT_WIDTH * 8);
  else
    *result += (buffer[start]) << (VINT_WIDTH * 8);

  return VINT_WIDTH + 1;
}

static int
elementRead (const char *buffer, const int start, const int end,
             unsigned int *ID, int64 * size)
{
  int64 tempID;
  int64 tempsize;
  int ID_offset, size_offset;

  tempID = 0;
  
  ID_offset = VINTparse (buffer, start, end, &tempID, 0);
  if (!ID_offset)
    return 0;
  size_offset = VINTparse (buffer, start + ID_offset, end, &tempsize, 1);
  if (!size_offset)
    return 0;
  *ID = (int) tempID;           /*id must be <4 and must to feet in uint */
  *size = tempsize;
  return ID_offset + size_offset;
}

static int64
getInt (const char *buffer, const int start, const int size)
{
/*return a int [8-64], from buffer, Big Endian*/
  int64 result = 0;
  int c;

  for (c = 1; c <= size; c++)
  {
    result += buffer[start + c - 1] << (8 * (size - c));
  }
  return result;
}

static float
getFloat (const char *buffer, const int start, const int size)
{
  float result = 0;
  char tmp[4];

  if (size == sizeof (float))
  {
    tmp[0] = buffer[start + 3];
    tmp[1] = buffer[start + 2];
    tmp[2] = buffer[start + 1];
    tmp[3] = buffer[start];
    result = *((float *) (tmp));
  }
  return result;
}

const unsigned int MKV_Parse_list[] = { /*Elements, containing requed information (sub-elements), see enum in mkv.h for values */
  MKVID_Segment,
  MKVID_Info,
  MKVID_Video,
  MKVID_Audio,
  MKVID_TrackEntry,
  MKVID_Tracks
};

const char stream_type_letters[] = "?vat";      /*[0]-no, [1]-video,[2]-audio,[3]-text */


/* video/mkv */
int
EXTRACTOR_mkv_extract (const unsigned char *data, size_t size1,
                       EXTRACTOR_MetaDataProcessor proc, void *proc_cls,
                       const char *options)
{
  int ret;
  char buffer[128],temp[128];
  int p;                        /*pointer in buffer */
  int c, c2;                    /*counter in some loops */

  unsigned int eID;             /*element ID */
  int64 eSize;                  /*Size of element content */
  int offs;
  int64 timescale = 1000000;
  float Duration = 0;
  int64 DefaultDuration = 0;
  int TrackType = 0;
  int pvt_look_flag = 0;
  int curr_c = -1;
  int a_c = -1;
  int v_c = -1;
  int t_c = -1;
  int value_width = 0;
  int value_height = 0;
  int value = 0;
  int size;

  ret = 0;
  p = 0;
 
  if (size1 > 16777216)
  {
	size = 16777216;
  }else{
	size = size1;
  }

  if (!size)
  {
    return 0;
  }

 
  while (data[p] != MKVID_FILE_BEGIN)
  {
    p++;
    if (p >= size)
    {
      return 0;
    }
  };                            /*skip text while EBML begin */

/*main loop*/
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
        sprintf (buffer, "%u(video)",
                 (int) (Duration / 1e+9 * (float) timescale));
        ADD (buffer, EXTRACTOR_METATYPE_DURATION);
        curr_c = v_c;
        break;
      case MKV_Track_audio:
        a_c++;
        if (a_c > MAX_STREAMS)
          a_c = MAX_STREAMS;
        sprintf (buffer, "%u(audio)",
                 (int) (Duration / 1e+9 * (float) timescale));
        ADD (buffer, EXTRACTOR_METATYPE_DURATION);
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
          sprintf (buffer, "fps: %u", 1000000000 / DefaultDuration);
          ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
        }
      }
      break;
    case MKVID_Language:       /*stream language */
      if (curr_c >= 0 && TrackType < 4 && eSize < MAX_STRING_SIZE)
	  {
		  strncpy(temp,data+p,eSize);
		  temp[eSize] = '\0';
		  sprintf (buffer, "%s", temp);
		  ADD (buffer, EXTRACTOR_METATYPE_LANGUAGE);
	  }
      break;
    case MKVID_CodecName:      /*passtrough */
    case MKVID_CodecID:        /*codec detection (if V_MS/VFW/FOURCC - set a fourcc code, else fill a vcodecs value) */
      if (curr_c >= 0 && TrackType < 4 && eSize < MAX_STRING_SIZE)
      {
       	  strncpy(temp,data+p,eSize);
		  temp[eSize] = '\0';
        if (!strcmp (temp, "V_MS/VFW/FOURCC"))
          pvt_look_flag = 1;
        sprintf (buffer, "codec: %s", temp);
        ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
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
      sprintf (buffer, "TimeScale: %u", timescale);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
    case MKVID_Duration:
      Duration = getFloat (data, p, eSize);
      sprintf (buffer, "duration: %u s", Duration);
      ADD (buffer, EXTRACTOR_METATYPE_DURATION);
      break;
    case MKVID_Channels:
      sprintf (buffer, "channels: %u", value);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
    case MKVID_BitDepth:
      sprintf (buffer, "BitDepth: %u", value);
      ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
      break;
    case MKVID_OutputSamplingFrequency:        /*pasthough */
    case MKVID_SamplingFrequency:
      sprintf (buffer, "Sampling Frequency: %u", value);
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
  
  sprintf (buffer, "Image dimensions: %u X %u", value_width, value_height);
  ADD (buffer, EXTRACTOR_METATYPE_UNKNOWN);
  
EXIT:

  return ret;

}
