/*
     This file is part of libextractor.
     Copyright (C) 2007, 2009 Heikki Lindholm

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
 * see http://osflash.org/flv
 *     http://osflash.org/documentation/amf
 */
#include "platform.h"
#include "extractor.h"
#include "convert_numeric.h"
#include <string.h>

#define DEBUG 0

#define FLV_SIGNATURE "FLV"

/*
 * AMF parser
 */

/* Actionscript types */
#define ASTYPE_NUMBER       0x00
#define ASTYPE_BOOLEAN      0x01
#define ASTYPE_STRING       0x02
#define ASTYPE_OBJECT       0x03
#define ASTYPE_MOVIECLIP    0x04
#define ASTYPE_NULL         0x05
#define ASTYPE_UNDEFINED    0x06
#define ASTYPE_REFERENCE    0x07
#define ASTYPE_MIXEDARRAY   0x08
#define ASTYPE_ENDOFOBJECT  0x09
#define ASTYPE_ARRAY        0x0a
#define ASTYPE_DATE         0x0b
#define ASTYPE_LONGSTRING   0x0c
#define ASTYPE_UNSUPPORTED  0x0d
#define ASTYPE_RECORDSET    0x0e
#define ASTYPE_XML          0x0f
#define ASTYPE_TYPEDOBJECT  0x10
#define ASTYPE_AMF3DATA     0x11

typedef struct {
  void * userdata;
  void (*as_begin_callback)(unsigned char type, void * userdata);
  void (*as_key_callback)(char * key, void * userdata);
  void (*as_end_callback)(unsigned char type, void * value, void * userdata);
} AMFParserHandler;

/* core datatypes */

static uint32_t readInt32(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  uint32_t val;

  val = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
  ptr += 4;
  *data = ptr;
  return val;
}

static uint32_t readInt24(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  uint32_t val;

  val = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
  ptr += 3;
  *data = ptr;
  return val;
}

static uint16_t readInt16(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  uint16_t val;

  val = (ptr[0] << 8) | ptr[1];
  ptr += 2;
  *data = ptr;
  return val;
}

static double readDouble(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  double val;

  EXTRACTOR_common_floatformat_to_double(&EXTRACTOR_floatformat_ieee_double_big,
                        (const void *)ptr,
                        &val);
  ptr += 8;
  *data = ptr;
  return val;
}


/* actionscript types */

static int readASNumber(const unsigned char **data,
                               size_t *len,
                               double *retval)
{
  const unsigned char *ptr = *data;
  double val;

  if (*len < 8)
    return -1;

  val = readDouble(&ptr);
  *len -= 8;

  *retval = val;
  *data = ptr;
  return 0;
}

static int readASBoolean(const unsigned char **data,
                                size_t *len,
                                int *retval)
{
  const unsigned char *ptr = *data;
  int val;

  if (*len < 1)
    return -1;

  val = (*ptr != 0x00);
  ptr += 1;
  *len -= 1;

  *retval = val;
  *data = ptr;
  return 0;
}

static int readASDate(const unsigned char **data,
                             size_t *len,
                             double *millis,
                             short *zone)
{
  const unsigned char *ptr = *data;

  if (*len < 10)
    return -1;

  *millis = readDouble(&ptr);
  *len -= 8;

  *zone = readInt16(&ptr);
  *len -= 2;

  *data = ptr;
  return 0;
}

static int readASString(const unsigned char **data,
                               size_t *len,
                               char **retval)
{
  const unsigned char *ptr = *data;
  char *ret;
  int slen;

  if (*len < 2)
    return -1;

  slen = readInt16(&ptr);

  if (*len < (2 + slen))
    return -1;

  ret = malloc(slen+1);
  if (ret == NULL)
    return -1;
  memcpy(ret, ptr, slen);
  ret[slen] = '\0';
  ptr += slen;
  *len -= (2 + slen);

  *retval = ret;
  *data = ptr;
  return 0;
}

static int parse_amf(const unsigned char **data,
              size_t *len,
              AMFParserHandler *handler)
{
  const unsigned char *ptr = *data;
  unsigned char astype;
  int ret;

  ret = 0;
  astype = *ptr++;
  (*(handler->as_begin_callback))(astype, handler->userdata);
  switch (astype) {
    case ASTYPE_NUMBER:
    {
      double val;
      ret = readASNumber(&ptr, len, &val);
      if (ret == 0)
        (*(handler->as_end_callback))(astype,
                                      &val,
                                      handler->userdata);
      break;
    }
    case ASTYPE_BOOLEAN:
    {
      int val;
      ret = readASBoolean(&ptr, len, &val);
      if (ret == 0)
        (*(handler->as_end_callback))(astype,
                                      &val,
                                      handler->userdata);
      break;
    }
    case ASTYPE_STRING:
    {
      char *val;
      ret = readASString(&ptr, len, &val);
      if (ret == 0) {
        (*(handler->as_end_callback))(astype,
                                      val,
                                      handler->userdata);
        free(val);
      }
      break;
    }
    case ASTYPE_DATE:
    {
      void *tmp[2];
      double millis;
      short tz;
      ret = readASDate(&ptr, len, &millis, &tz);
      tmp[0] = &millis;
      tmp[1] = &tz;
      if (ret == 0)
        (*(handler->as_end_callback))(astype,
                                      &tmp,
                                      handler->userdata);
      break;
    }
    case ASTYPE_NULL:
    case ASTYPE_UNDEFINED:
    case ASTYPE_UNSUPPORTED:
    case ASTYPE_ENDOFOBJECT:
      (*(handler->as_end_callback))(astype, NULL, handler->userdata);
      break;
    case ASTYPE_ARRAY:
    {
      long i, alen;
      if (*len < 4) {
        ret = -1;
        break;
      }
      alen = readInt32(&ptr);
      *len -= 4;
      for (i = 0; i < alen; i++) {
        ret = parse_amf(&ptr, len, handler);
        if (ret == -1)
	  break;
      }
      (*(handler->as_end_callback))(ASTYPE_ARRAY,
                                    NULL,
                                    handler->userdata);
      break;
    }
    case ASTYPE_OBJECT:
    {
      char *key;
      unsigned char type;

      ret = readASString(&ptr, len, &key);
      if (ret == -1)
        break;
      (*(handler->as_key_callback))(key,
                                    handler->userdata);
      free(key);
      type = *ptr;
      while (type != ASTYPE_ENDOFOBJECT) {
        ret = parse_amf(&ptr, len, handler);
        if (ret == -1)
          break;
        ret = readASString(&ptr, len, &key);
        if (ret == -1)
          break;
        (*(handler->as_key_callback))(key,
                                      handler->userdata);
        free(key);
        type = *ptr;
      }
      if (ret == 0)
        (*(handler->as_end_callback))(ASTYPE_OBJECT,
                                      NULL,
                                      handler->userdata);
      break;
    }
    case ASTYPE_MIXEDARRAY:
    {
      char *key;
      unsigned char type;
      
      if (*len < 4) {
        ret = -1;
        break;
      }
      /* max_index */ readInt32(&ptr);
      *len -= 4;
      ret = readASString(&ptr, len, &key);
      if (ret == -1)
        break;
      (*(handler->as_key_callback))(key,
                                    handler->userdata);
      free(key);
      type = *ptr;
      while (type != ASTYPE_ENDOFOBJECT) {
        ret = parse_amf(&ptr, len, handler);
        if (ret == -1)
          break;
        ret = readASString(&ptr, len, &key);
        if (ret == -1)
          break;
        (*(handler->as_key_callback))(key,
                                      handler->userdata);
        free(key);
        type = *ptr;
      }
      if (ret == 0)
        (*(handler->as_end_callback))(astype,
                                      NULL,
                                      handler->userdata);
      break;
    }
    default:
      ret = -1;
      (*(handler->as_end_callback))(astype,
                                    NULL,
                                    handler->userdata);
#if DEBUG
      printf("parse_amf: Unknown type %02x", astype);
#endif
      break;
  }

  *data = ptr;
  return ret;
}

/*
 * FLV parser
 */

/* from tarextractor, modified to take timezone */
/* TODO: check that the output date is correct */
static int
flv_to_iso_date (double timeval, short timezone,
                 char *rtime, unsigned int rsize)
{
  int retval = 0;

  /*
   * shift epoch to proleptic times
   * to make subsequent modulo operations safer.
   */
  long long my_timeval = (timeval/1000)
    + ((long long) ((1970 * 365) + 478) * (long long) 86400);

  unsigned int seconds = (unsigned int) (my_timeval % 60);
  unsigned int minutes = (unsigned int) ((my_timeval / 60) % 60);
  unsigned int hours = (unsigned int) ((my_timeval / 3600) % 24);

  int zone_sign;
  int zone_hours;
  unsigned int zone_minutes;

  unsigned int year = 0;
  unsigned int month = 1;

  unsigned int days = (unsigned int) (my_timeval / (24 * 3600));

  unsigned int days_in_month[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  unsigned int diff = 0;

  if ((long long) 0 > my_timeval)
    return EDOM;

  /*
   * 400-year periods
   */
  year += (400 * (days / ((365 * 400) + 97)));
  days %= ((365 * 400) + 97);

  /*
   * 100-year periods
   */
  diff = (days / ((365 * 100) + 24));
  if (4 <= diff)
    {
      year += 399;
      days = 364;
    }
  else
    {
      year += (100 * diff);
      days %= ((365 * 100) + 24);
    }

  /*
   * remaining leap years
   */
  year += (4 * (days / ((365 * 4) + 1)));
  days %= ((365 * 4) + 1);

  while (1)
    {
      if ((0 == (year % 400)) || ((0 == (year % 4)) && (0 != (year % 100))))
        {
          if (366 > days)
            {
              break;
            }
          else
            {
              days -= 366;
              year++;
            }
        }
      else
        {
          if (365 > days)
            {
              break;
            }
          else
            {
              days -= 365;
              year++;
            }
        }
    }

  if ((0 == (year % 400)) || ((0 == (year % 4)) && (0 != (year % 100))))
    days_in_month[1] = 29;

  for (month = 0; (month < 12) && (days >= days_in_month[month]); month += 1)
    days -= days_in_month[month];

  zone_sign = 0;
  if (timezone < 0)
    {
      zone_sign = -1;
      timezone = -timezone;
    }
  zone_hours = timezone/60;
  zone_minutes = timezone - zone_hours*60;

  retval = snprintf (rtime, rsize, "%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02u",
                     year, month + 1, days + 1, hours, minutes, seconds,
                     ((zone_sign < 0) ? '-' : '+'), zone_hours, zone_minutes);

  return (retval < rsize) ? 0 : EOVERFLOW;
}

typedef struct
{
  char signature[3];
  unsigned char version;
  unsigned char flags;
  unsigned long offset;
} FLVHeader;

#define FLV_HEADER_SIZE 9

#define FLV_TAG_TYPE_AUDIO 0x08
#define FLV_TAG_TYPE_VIDEO 0x09
#define FLV_TAG_TYPE_META  0x12

typedef struct
{
  unsigned char type;
  unsigned long bodyLength;
  uint32_t timestamp;
  unsigned long streamId;
} FLVTagHeader;

#define FLV_TAG_HEADER_SIZE 11

static int readFLVHeader(const unsigned char **data,
			 const unsigned char *end,
			 FLVHeader *hdr)
{
  const unsigned char *ptr = *data;

  if ((ptr + FLV_HEADER_SIZE) > end)
    return -1;

  memcpy(hdr->signature, ptr, 3);
  ptr += 3;
  hdr->version = *ptr++;
  hdr->flags = *ptr++;
  hdr->offset = readInt32(&ptr);
  if (hdr->offset != FLV_HEADER_SIZE)
    return -1;

  *data = ptr;
  return 0;
}

static int readPreviousTagSize(const unsigned char **data,
                                const unsigned char *end,
                                unsigned long *prev_size)
{
  const unsigned char *ptr = *data;

  if ((ptr + 4) > end)
    return -1;

  *prev_size = readInt32(&ptr);

  *data = ptr;
  return 0;
}

static int readFLVTagHeader(const unsigned char **data,
                          const unsigned char *end,
                          FLVTagHeader *hdr)
{
  const unsigned char *ptr = *data;

  if ((ptr + FLV_TAG_HEADER_SIZE) > end)
    return -1;

  hdr->type = *ptr++;
  hdr->bodyLength = readInt24(&ptr);
  hdr->timestamp = readInt32(&ptr);
  hdr->streamId = readInt24(&ptr);

  *data = ptr;
  return 0;
}

typedef struct {
  int videoCodec;
  char *videoCodecStr;
  int videoWidth;
  int videoHeight;
  double videoDataRate;
  double videoFrameRate;

  int audioCodec;
  char *audioCodecStr;
  double audioDataRate;
  int audioChannels;
  int audioSampleBits;
  int audioRate;
} FLVStreamInfo;

typedef enum {
  FLV_NONE = 0,
  FLV_WIDTH,
  FLV_HEIGHT,
  FLV_FRAMERATE,
  FLV_STEREO,
  FLV_ACHANNELS,
  FLV_VDATARATE,
  FLV_ADATARATE,
  FLV_VCODECID,
  FLV_ACODECID
} FLVStreamAttribute;

typedef struct {
  const char *key;
  FLVStreamAttribute attribute;
} MetaKeyToStreamAttribute;

static MetaKeyToStreamAttribute key_to_attribute_map[] = {
  { "width", FLV_WIDTH },
  { "height", FLV_HEIGHT },
  { "framerate", FLV_FRAMERATE },
  { "videoframerate", FLV_FRAMERATE },
  { "stereo", FLV_STEREO },
  { "audiochannels", FLV_ACHANNELS },
  { "videodatarate", FLV_VDATARATE },
  { "audiodatarate", FLV_ADATARATE },
  { "videocodecid", FLV_VCODECID },
  { "audiocodecid", FLV_ACODECID },
  { NULL, FLV_NONE }
};

typedef struct {
  const char *key;
  enum EXTRACTOR_MetaType type;
} MetaKeyToExtractorItem;

static MetaKeyToExtractorItem key_to_extractor_map[] = {
  { "duration", EXTRACTOR_METATYPE_DURATION },
  { "creator", EXTRACTOR_METATYPE_CREATOR },
  { "metadatacreator", EXTRACTOR_METATYPE_CREATOR },
  { "creationdate", EXTRACTOR_METATYPE_CREATION_DATE },
  { "metadatadate", EXTRACTOR_METATYPE_MODIFICATION_DATE },
  { NULL, EXTRACTOR_METATYPE_RESERVED }
};

typedef struct {
  int onMetaData;
  int parsingDepth;
  int ret;
  /* mixed array keys mapped to something readily usable */
  enum EXTRACTOR_MetaType currentKeyType;
  FLVStreamAttribute currentAttribute;

  EXTRACTOR_MetaDataProcessor proc;
  void *proc_cls;
  FLVStreamInfo *streamInfo;
} FLVMetaParserState;

static void handleASBegin(unsigned char type, void * userdata)
{
  FLVMetaParserState *state = (FLVMetaParserState *)userdata;

  if (state->onMetaData && state->parsingDepth == 0 &&
      type != ASTYPE_MIXEDARRAY)
    state->onMetaData = 0;

  if (type == ASTYPE_ARRAY || type == ASTYPE_MIXEDARRAY ||
      type == ASTYPE_OBJECT)
    state->parsingDepth++;
}

static void handleASKey(char * key, void * userdata)
{
  FLVMetaParserState *state = (FLVMetaParserState *)userdata;
  int i;

  if (key == NULL)
    return;

  i = 0;
  while ((key_to_extractor_map[i].key != NULL) &&
         (strcasecmp(key, key_to_extractor_map[i].key) != 0))
    i++;
  state->currentKeyType = key_to_extractor_map[i].type;

  i = 0;
  while ((key_to_attribute_map[i].key != NULL) &&
         (strcasecmp(key, key_to_attribute_map[i].key) != 0))
    i++;
  state->currentAttribute = key_to_attribute_map[i].attribute;
}

static void handleASEnd(unsigned char type, void * value, void * userdata)
{
  FLVMetaParserState *state = (FLVMetaParserState *)userdata;
  const char *s;
  char tmpstr[30];

  if ((state->parsingDepth == 0) && (type == ASTYPE_STRING)) {
    s = (const char *)value;
    if (!strcmp(s, "onMetaData"))
      state->onMetaData = 1;
  }

  /* we expect usable metadata to reside in a MIXEDARRAY container
   * right after a "onMetaData" STRING */

  /* stream info related metadata */
  if (state->onMetaData && (state->parsingDepth == 1) &&
      (state->currentAttribute != FLV_NONE) &&
      (type == ASTYPE_NUMBER))
  {
    double n = *((double *)value);
    switch (state->currentAttribute) {
      case FLV_NONE: /* make gcc happy */
	break;
      case FLV_STEREO:
        break;
      case FLV_ACHANNELS:
        state->streamInfo->audioChannels = n;
        break;
      case FLV_WIDTH:
        if (state->streamInfo->videoWidth == -1)
          state->streamInfo->videoWidth = n;
        break;
      case FLV_HEIGHT:
        if (state->streamInfo->videoHeight == -1)
          state->streamInfo->videoHeight = n;
        break;
      case FLV_FRAMERATE:
        state->streamInfo->videoFrameRate = n;
        break;
      case FLV_VDATARATE:
        state->streamInfo->videoDataRate = n;
        break;
      case FLV_ADATARATE:
        state->streamInfo->audioDataRate = n;
        break;
      case FLV_VCODECID:
        if (state->streamInfo->videoCodec == -1)
          state->streamInfo->videoCodec = n;
        /* prefer codec ids to fourcc codes */
        if (state->streamInfo->videoCodecStr != NULL) {
          free(state->streamInfo->videoCodecStr);
          state->streamInfo->videoCodecStr = NULL;
        }
        break;
      case FLV_ACODECID:
        if (state->streamInfo->audioCodec == -1)
          state->streamInfo->audioCodec = n;
        /* prefer codec ids to fourcc codes */
        if (state->streamInfo->audioCodecStr != NULL) {
          free(state->streamInfo->audioCodecStr);
          state->streamInfo->audioCodecStr = NULL;
        }
        break;
    }
  }

  /* sometimes a/v codecs are as fourcc strings */
  if (state->onMetaData && (state->parsingDepth == 1) &&
      (state->currentAttribute != FLV_NONE) &&
      (type == ASTYPE_STRING))
  {
    s = (const char *)value;
    switch (state->currentAttribute) {
      case FLV_VCODECID:
        if (s != NULL && state->streamInfo->videoCodecStr == NULL &&
            state->streamInfo->videoCodec == -1)
          state->streamInfo->videoCodecStr = strdup(s);
        break;
      case FLV_ACODECID:
        if (s != NULL && state->streamInfo->audioCodecStr == NULL &&
            state->streamInfo->audioCodec == -1)
          state->streamInfo->audioCodecStr = strdup(s);
        break;
      default:
        break;
    }
  }

  if (state->onMetaData && (state->parsingDepth == 1) &&
      (state->currentAttribute == FLV_STEREO) &&
      (type == ASTYPE_BOOLEAN))
  {
    int n = *((int *)value);
    if (state->streamInfo->audioChannels == -1)
      state->streamInfo->audioChannels = (n == 0) ? 1 : 2;
  }

  /* metadata that maps straight to extractor keys */
  if (state->onMetaData && (state->parsingDepth == 1) &&
      (state->currentKeyType != EXTRACTOR_METATYPE_RESERVED))
  {
    s = NULL;
    switch (type) {
      case ASTYPE_NUMBER:
      {
        double n = *((double *)value);
        s = tmpstr;
	if (state->currentKeyType == EXTRACTOR_METATYPE_DURATION)
          snprintf(tmpstr, sizeof(tmpstr), "%.2f s", n);
	else
          snprintf(tmpstr, sizeof(tmpstr), "%f", n);
	break;
      }
      case ASTYPE_STRING:
      {
        s = (char *)value;
        break;
      }
      case ASTYPE_DATE:
      {
        void **tmp = (void **)value;
	double *millis;
	short *tz;
        millis = (double *)tmp[0];
	tz = (short *)tmp[1];
	if (0 == flv_to_iso_date(*millis, *tz, tmpstr, sizeof(tmpstr)))
	  s = tmpstr;
        break;
      }
    }
    if ( (s != NULL) &&
	 (state->ret == 0) )
      state->ret = state->proc (state->proc_cls,
				"flv",
				state->currentKeyType,
				EXTRACTOR_METAFORMAT_UTF8,
				"text/plain",
				s,
				strlen (s) + 1);
  }
  state->currentKeyType = EXTRACTOR_METATYPE_RESERVED;
  state->currentAttribute = FLV_NONE;

  if (type == ASTYPE_ARRAY || type == ASTYPE_MIXEDARRAY ||
      type == ASTYPE_OBJECT)
    state->parsingDepth--;
}

static int
handleMetaBody(const unsigned char *data, size_t len,
	       FLVStreamInfo *stinfo,
	       EXTRACTOR_MetaDataProcessor proc,
	       void *proc_cls)
{
  AMFParserHandler handler;
  FLVMetaParserState pstate;

  pstate.onMetaData = 0;
  pstate.currentKeyType = EXTRACTOR_METATYPE_RESERVED;
  pstate.parsingDepth = 0;
  pstate.streamInfo = stinfo;
  pstate.ret = 0;
  pstate.proc = proc;
  pstate.proc_cls = proc_cls;
  handler.userdata = &pstate;
  handler.as_begin_callback = &handleASBegin;
  handler.as_key_callback = &handleASKey;
  handler.as_end_callback = &handleASEnd;

  while (len > 0 && parse_amf(&data, &len, &handler) == 0);
  if (pstate.ret != 0)
    return 1;
  return 0;
}

static char *FLVAudioCodecs[] = {
  "Uncompressed",
  "ADPCM",
  "MP3",
  NULL,
  NULL,
  "Nellymoser 8kHz mono",
  "Nellymoser",
  NULL,
  NULL,
  NULL,
  "AAC",
  "Speex"
};

static char *FLVAudioChannels[] = {
  "mono",
  "stereo"
};

static char *FLVAudioSampleSizes[] = {
  "8-bit",
  "16-bit"
};

static char *FLVAudioSampleRates[] = {
  "5512.5",
  "11025",
  "22050",
  "44100"
};

static void
handleAudioBody(const unsigned char *data, size_t len,
                FLVStreamInfo *stinfo)
{
  stinfo->audioChannels = (*data & 0x01) + 1;
  stinfo->audioSampleBits = (*data & 0x02) >> 1;
  stinfo->audioRate = (*data & 0x0C) >> 2;
  stinfo->audioCodec = (*data & 0xF0) >> 4;
  if (stinfo->audioCodecStr != NULL) {
    free(stinfo->audioCodecStr);
    stinfo->audioCodecStr = NULL;
  }
}

static char *FLVVideoCodecs[] = {
  NULL,
  NULL,
  "Sorenson Spark",
  "ScreenVideo",
  "On2 TrueMotion VP6",
  "On2 TrueMotion VP6 Alpha",
  "ScreenVideo 2",
  "H.264" /* XXX not found in docs */
};

static int sorenson_predefined_res[][2] = {
  { -1, -1 },
  { -1, -1 },
  { 352, 288 },
  { 176, 144 },
  { 128, 96 },
  { 320, 240 },
  { 160, 120 },
  { -1, -1 }
};

static void
handleVideoBody(const unsigned char *data, size_t len,
                FLVStreamInfo *stinfo)
{
  int codecId, frameType;

  codecId = *data & 0x0F;
  frameType = (*data & 0xF0) >> 4;
  data++;

  /* try to get video dimensions */
  switch (codecId) {
    case 0x02: /* Sorenson */
      if (len < 9)
        break;
      if (frameType == 1) {
        int start_code = (data[0] << 9) | (data[1] << 1) | ((data[2] >> 7)&0x1);
        int version = (data[2] & 0x7C) >> 2;
        int frame_size = ((data[3] & 0x03) << 1) | (data[4] >> 7);
        if (start_code != 0x00000001)
          break;
        if (!(version == 0 || version == 1))
          break;
        if (frame_size == 0) {
          stinfo->videoWidth = ((data[4] & 0x7F) >> 1) | (data[5] >> 7);
          stinfo->videoHeight = ((data[5] & 0x7F) >> 1) | (data[6] >> 7);
        }
        else if (frame_size == 1) {
          stinfo->videoWidth = ((data[4] & 0x7F) << 9) | (data[5] << 1) |
                               (data[6] >> 7);
          stinfo->videoHeight = ((data[6] & 0x7F) << 9) | (data[7] << 1) |
                                (data[8] >> 7);
        }
        else {
          stinfo->videoWidth = sorenson_predefined_res[frame_size][0];
          stinfo->videoHeight = sorenson_predefined_res[frame_size][1];
        }
      }
      break;
    case 0x03: /* ScreenVideo */
      if (len < 5)
        break;
      stinfo->videoWidth = readInt16(&data) & 0x0FFF;
      stinfo->videoHeight = readInt16(&data) & 0x0FFF;
      break;
    case 0x04: /* On2 VP6 */
    case 0x05:
    {
      unsigned char dim_adj;
      if (len < 10)
        break;
      dim_adj = *data++;
      if ((frameType == 1) && ((data[0] & 0x80) == 0)) {
        /* see ffmpeg vp6 decoder */
        int separated_coeff = data[0] & 0x01;
        int filter_header = data[1] & 0x06;
        /*int interlaced = data[1] & 0x01; TODO: used in flv ever? */
        if (separated_coeff || !filter_header) {
          data += 2;
        }
        /* XXX encoded/displayed dimensions might vary, but which are the
         * right ones? */
        stinfo->videoWidth = (data[3]*16) - (dim_adj>>4);
        stinfo->videoHeight = (data[2]*16) - (dim_adj&0x0F);
      }
      break;
    }
    default:
      break;
  }

  stinfo->videoCodec = codecId;
  if (stinfo->videoCodecStr != NULL) {
    free(stinfo->videoCodecStr);
    stinfo->videoCodecStr = NULL;
  }
}

static int readFLVTag(const unsigned char **data,
                      const unsigned char *end,
                      FLVStreamInfo *stinfo,
		      EXTRACTOR_MetaDataProcessor proc,
		      void *proc_cls)
{
  const unsigned char *ptr = *data;
  FLVTagHeader header;
  int ret = 0;

  if (readFLVTagHeader(&ptr, end, &header) == -1)
    return -1;

  if ((ptr + header.bodyLength) > end)
    return -1;

  switch (header.type)
  {
    case FLV_TAG_TYPE_AUDIO:
      handleAudioBody(ptr, header.bodyLength, stinfo);
      break;
    case FLV_TAG_TYPE_VIDEO:
      handleVideoBody(ptr, header.bodyLength, stinfo);
      break;
    case FLV_TAG_TYPE_META:
      ret = handleMetaBody(ptr, header.bodyLength, stinfo, proc, proc_cls);
      break;
    default:
      break;
  }

  ptr += header.bodyLength;
  *data = ptr;
  return ret;
}

#define MAX_FLV_FORMAT_LINE 80
static char * printVideoFormat(FLVStreamInfo *stinfo)
{
  char s[MAX_FLV_FORMAT_LINE+1];
  int n;
  size_t len = MAX_FLV_FORMAT_LINE;

  n = 0;
  /* some files seem to specify only the width or the height, print '?' for
   * the unknown dimension */
  if (stinfo->videoWidth != -1 || stinfo->videoHeight != -1) {
    if (n < len) {
      if (stinfo->videoWidth != -1)
        n += snprintf(s+n, len-n, "%d", stinfo->videoWidth);
      else
        n += snprintf(s+n, len-n, "?");
    }

    if (n < len) {
      if (stinfo->videoHeight != -1)
        n += snprintf(s+n, len-n, "x%d", stinfo->videoHeight);
      else
        n += snprintf(s+n, len-n, "x?");
    }
  }

  if (stinfo->videoFrameRate != 0.0 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%0.2f fps", stinfo->videoFrameRate);
  }

  if (stinfo->videoCodec > -1 && stinfo->videoCodec < 8 &&
      FLVVideoCodecs[stinfo->videoCodec] != NULL && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", FLVVideoCodecs[stinfo->videoCodec]);
  }
  else if (stinfo->videoCodecStr != NULL && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", stinfo->videoCodecStr);    
  }

  if (stinfo->videoDataRate != 0.0 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%.4f kbps", stinfo->videoDataRate);
  }

  if (n == 0)
    return NULL;
  return strdup(s);
}

static char * printAudioFormat(FLVStreamInfo *stinfo)
{
  char s[MAX_FLV_FORMAT_LINE+1];
  int n;
  size_t len = MAX_FLV_FORMAT_LINE;

  n = 0;
  if ( (stinfo->audioRate != -1) && (n < len)) {
      n += snprintf(s+n, len-n, "%s Hz", FLVAudioSampleRates[stinfo->audioRate]);
  }

  if ((stinfo->audioSampleBits != -1) && (n < len)) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s",
                    FLVAudioSampleSizes[stinfo->audioSampleBits]);
  }

  if ((stinfo->audioChannels != -1) && (n < len)) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len) {
      if (stinfo->audioChannels >= 1 && stinfo->audioChannels <= 2)
        n += snprintf(s+n, len-n, "%s",
                      FLVAudioChannels[stinfo->audioChannels-1]);
      else
        n += snprintf(s+n, len-n, "%d",
                      stinfo->audioChannels);
    }
  }

  if ((stinfo->audioCodec > -1) && (stinfo->audioCodec < 12) &&
      (FLVAudioCodecs[stinfo->audioCodec] != NULL) && (n < len)) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", FLVAudioCodecs[stinfo->audioCodec]);
  }
  else if ((stinfo->audioCodecStr != NULL) && (n < len)) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", stinfo->audioCodecStr);    
  }

  if ((stinfo->audioDataRate != 0.0) && (n < len)) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%.4f kbps", stinfo->audioDataRate);
  }

  if (n == 0)
    return NULL;
  return strdup(s);
}

int 
EXTRACTOR_flv_extract (const unsigned char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  const unsigned char *ptr;
  const unsigned char *end;
  FLVStreamInfo stinfo;
  FLVHeader header;
  unsigned long prev_tag_size;
  char *s;
  int ret;

  ptr = data;
  end = ptr + size;

  if (readFLVHeader(&ptr, end, &header) == -1)
    return 0;

  if (memcmp(header.signature, FLV_SIGNATURE, 3) != 0)
    return 0;

  if (0 != proc (proc_cls,
		 "flv",
		 EXTRACTOR_METATYPE_MIMETYPE, 
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "video/x-flv",
		 strlen ("video/x-flv") + 1))
    return 0;
  if (header.version != 1)
    return 0;
  if (readPreviousTagSize (&ptr, end, &prev_tag_size) == -1)
    return 0;

  stinfo.videoCodec = -1;
  stinfo.videoCodecStr = NULL;
  stinfo.videoWidth = -1;
  stinfo.videoHeight = -1;
  stinfo.videoFrameRate = 0.0;
  stinfo.videoDataRate = 0.0;
  stinfo.audioCodec = -1;
  stinfo.audioCodecStr = NULL;
  stinfo.audioRate = -1;
  stinfo.audioSampleBits = -1;
  stinfo.audioChannels = -1;
  stinfo.audioDataRate = 0.0;
  ret = 0;
  while (ptr < end) {
    if (-1 == (ret = readFLVTag (&ptr, end, &stinfo, proc, proc_cls)))
      break;
    if (readPreviousTagSize (&ptr, end, &prev_tag_size) == -1)
      break;
  }
  if (1 == ret)
    return 1;
  s = printVideoFormat (&stinfo);
  if (s != NULL)
    {
      if (0 != proc (proc_cls, 
		     "flv",
		     EXTRACTOR_METATYPE_RESOURCE_TYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     s,
		     strlen (s)+1))
	{
	  free (s);
	  return 1;
	}
      free (s);
    }
  s = printAudioFormat (&stinfo);
  if (s != NULL)
    {
      if (0 != proc (proc_cls, 
		     "flv",
		     EXTRACTOR_METATYPE_RESOURCE_TYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     s,
		     strlen (s)+1))
	{
	  free (s);
	  return 1;
	}
      free (s);
    }
  return 0;
}
