/*
     This file is part of libextractor.
     Copyright (C) 2007 Heikki Lindholm

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
#include "amfparser.h"
#include <string.h>

#define DEBUG 0

#define FLV_SIGNATURE "FLV"

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

static inline unsigned long readBEInt32(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  unsigned long val;

  val = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
  ptr += 4;
  *data = ptr;
  return val;
}

static inline unsigned long readBEInt24(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  unsigned long val;

  val = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
  ptr += 3;
  *data = ptr;
  return val;
}

/* FLV parser */

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
  unsigned long timestamp;
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
  hdr->offset = readBEInt32(&ptr);
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

  *prev_size = readBEInt32(&ptr);

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
  hdr->bodyLength = readBEInt24(&ptr);
  hdr->timestamp = readBEInt24(&ptr);
  hdr->timestamp = (*ptr++ << 24) | hdr->timestamp;
  hdr->streamId = readBEInt24(&ptr);

  *data = ptr;
  return 0;
}

typedef struct {
  int videoCodec;
  int videoWidth;
  int videoHeight;
  double videoDataRate;
  double videoFrameRate;

  int audioCodec;
  double audioDataRate;
  int audioChannels;
  int audioSampleBits;
  int audioRate;
} FLVStreamState;

typedef enum {
  FLV_NONE = 0,
  FLV_WIDTH,
  FLV_HEIGHT,
  FLV_FRAMERATE,
  FLV_VDATARATE,
  FLV_ADATARATE,
} FLVStreamAttribute;

typedef struct {
  const char *key;
  FLVStreamAttribute attribute;
} MetaKeyToStreamAttribute;

static MetaKeyToStreamAttribute key_to_attribute_map[] = {
  { "width", FLV_WIDTH },
  { "height", FLV_HEIGHT },
  { "framerate", FLV_FRAMERATE },
  { "videodatarate", FLV_VDATARATE },
  { "audiodatarate", FLV_ADATARATE },
  { NULL, FLV_NONE }
};

typedef struct {
  const char *key;
  EXTRACTOR_KeywordType type;
} MetaKeyToExtractorItem;

static MetaKeyToExtractorItem key_to_extractor_map[] = {
  { "duration", EXTRACTOR_DURATION },
  { "creator", EXTRACTOR_CREATOR },
  { "metadatacreator", EXTRACTOR_CREATOR },
  { "creationdate", EXTRACTOR_CREATION_DATE },
  { "metadatadate", EXTRACTOR_MODIFICATION_DATE },
  { NULL, EXTRACTOR_UNKNOWN }
};

typedef struct {
  int onMetaData;
  int parsingDepth;
  /* mixed array keys mapped to something readily usable */
  EXTRACTOR_KeywordType currentKeyType;
  FLVStreamAttribute currentAttribute;

  struct EXTRACTOR_Keywords *keywords;
  FLVStreamState *streamState;
} FLVMetaParserState;

static void handleASBegin(unsigned char type, void * userdata)
{
  FLVMetaParserState *state = (FLVMetaParserState *)userdata;
#if DEBUG
  printf("handleASBeginCallback %p\n", state);
#endif
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
#if DEBUG
  printf("handleASKeyCallback %p [%s]\n", state, key);
#endif
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
  char *s;
#if DEBUG
  printf("handleASEndCallback %p %p\n", state, value);
#endif
  if ((state->parsingDepth == 0) && (type == ASTYPE_STRING)) {
    s = (char *)value;
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
      case FLV_NONE:
        break;
      case FLV_WIDTH:
        if (state->streamState->videoWidth == -1)
          state->streamState->videoWidth = n;
        break;
      case FLV_HEIGHT:
        if (state->streamState->videoHeight == -1)
          state->streamState->videoHeight = n;
        break;
      case FLV_FRAMERATE:
        state->streamState->videoFrameRate = n;
        break; 
      case FLV_VDATARATE:
        state->streamState->videoDataRate = n;
        break; 
      case FLV_ADATARATE:
        state->streamState->audioDataRate = n;
        break;
    }
  }

  /* metadata that maps straight to extractor keys */
  if (state->onMetaData && (state->parsingDepth == 1) && 
      (state->currentKeyType != EXTRACTOR_UNKNOWN))
  {
    s = NULL;
    switch (type) {
      case ASTYPE_NUMBER:
      {
        double n = *((double *)value);
        s = malloc(30);
	if (s == NULL)
	  break;
	if (state->currentKeyType == EXTRACTOR_DURATION)
          snprintf(s, 30, "%.4f s", n);
	else
          snprintf(s, 30, "%f", n);
	break;
      }
      case ASTYPE_STRING:
      {
        s = (char *)value;
	if (s != NULL)
	  s = strdup(s);
        break;
      }
      case ASTYPE_DATE:
      {
        void **tmp = (void **)value;
	double *millis;
	short *tz;
        millis = (double *)tmp[0];
	tz = (short *)tmp[1];
	s = malloc(30);
	if (s == NULL)
	  break;
	flv_to_iso_date(*millis, *tz, s, 30);
        break;
      }
    }

    if (s != NULL)
      state->keywords = addKeyword (state->currentKeyType, 
                                    s,
                                    state->keywords);
  }
  state->currentKeyType = EXTRACTOR_UNKNOWN;
  state->currentAttribute = FLV_NONE;

  if (type == ASTYPE_ARRAY || type == ASTYPE_MIXEDARRAY || 
      type == ASTYPE_OBJECT)
    state->parsingDepth--;
}

static struct EXTRACTOR_Keywords *
handleMetaBody(const unsigned char *data, size_t len, 
                FLVStreamState *state,
                struct EXTRACTOR_Keywords *prev) 
{
  AMFParserHandler handler;
  FLVMetaParserState pstate;
#if DEBUG
  printf("handleMetaBody()\n");
#endif

  pstate.onMetaData = 0;
  pstate.currentKeyType = EXTRACTOR_UNKNOWN;
  pstate.parsingDepth = 0;
  pstate.keywords = prev;
  pstate.streamState = state;
  handler.userdata = &pstate;
  handler.as_begin_callback = &handleASBegin;
  handler.as_key_callback = &handleASKey;
  handler.as_end_callback = &handleASEnd;

  while (len > 0 && parse_amf(&data, &len, &handler) == 0);

  return pstate.keywords;
}

static char *FLVAudioCodecs[] = {
  "Uncompressed",
  "ADPCM",
  "MP3",
  NULL,
  NULL,
  "Nellymoser 8kHz mono",
  "Nellymoser",
  NULL
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

static struct EXTRACTOR_Keywords *
handleAudioBody(const unsigned char *data, size_t len, 
                FLVStreamState *state,
                struct EXTRACTOR_Keywords *prev) 
{
  int soundType, soundSize, soundRate, soundFormat;

  soundType = *data & 0x01;
  soundSize = (*data & 0x02) >> 1;
  soundRate = (*data & 0x0C) >> 2;
  soundFormat = (*data & 0xF0) >> 4;

  state->audioCodec = soundFormat;
  state->audioRate = soundRate;
  state->audioChannels = soundType;
  state->audioSampleBits = soundSize;

  return prev;
}

static char *FLVVideoCodecs[] = {
  NULL,
  NULL,
  "Sorenson Spark",
  "ScreenVideo",
  "On2 TrueMotion VP6",
  "On2 TrueMotion VP6 Alpha",
  "ScreenVideo 2",
  NULL
};

static struct EXTRACTOR_Keywords *
handleVideoBody(const unsigned char *data, size_t len, 
                FLVStreamState *state,
                struct EXTRACTOR_Keywords *prev) 
{
  int codecId, frameType;

  codecId = *data & 0x0F;
  frameType = (*data & 0xF0) >> 4;

  state->videoCodec = codecId;
  return prev;
}

static int readFLVTag(const unsigned char **data,
                      const unsigned char *end,
                      FLVStreamState *state,
                      struct EXTRACTOR_Keywords **list)
{
  const unsigned char *ptr = *data;
  struct EXTRACTOR_Keywords *head = *list;
  FLVTagHeader header;

  if (readFLVTagHeader(&ptr, end, &header) == -1)
    return -1;

  if ((ptr + header.bodyLength) > end)
    return -1;

  switch (header.type) 
  {
    case FLV_TAG_TYPE_AUDIO:
      head = handleAudioBody(ptr, header.bodyLength, state, head);
      break;
    case FLV_TAG_TYPE_VIDEO:
      head = handleVideoBody(ptr, header.bodyLength, state, head);
      break;
    case FLV_TAG_TYPE_META:
      head = handleMetaBody(ptr, header.bodyLength, state, head);
      break;
    default:
      break;
  }

  ptr += header.bodyLength;
  
  *list = head;
  *data = ptr;
  return 0;
}

#define MAX_FLV_FORMAT_LINE 80
static char * printVideoFormat(FLVStreamState *state)
{
  char s[MAX_FLV_FORMAT_LINE+1];
  int n;
  size_t len = MAX_FLV_FORMAT_LINE;

  n = 0;
  /* some files seem to specify only the width or the height, print '?' for
   * the unknown dimension */
  if (state->videoWidth != -1 || state->videoHeight != -1) {
    if (n < len) {
      if (state->videoWidth != -1)
        n += snprintf(s+n, len-n, "%d", state->videoWidth);
      else
        n += snprintf(s+n, len-n, "?");
    }

    if (n < len) {
      if (state->videoHeight != -1)
        n += snprintf(s+n, len-n, "x%d", state->videoHeight);
      else
        n += snprintf(s+n, len-n, "x?");
    }
  }

  if (state->videoFrameRate != 0.0 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%0.2f fps", state->videoFrameRate);
  }

  if (state->videoCodec != -1 && FLVVideoCodecs[state->videoCodec] != NULL &&
      n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", FLVVideoCodecs[state->videoCodec]);
  }

  if (state->videoDataRate != 0.0 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%.4f kbps", state->videoDataRate);
  }

  if (n == 0) 
    return NULL;
  return strdup(s);
}

static char * printAudioFormat(FLVStreamState *state)
{
  char s[MAX_FLV_FORMAT_LINE+1];
  int n;
  size_t len = MAX_FLV_FORMAT_LINE;

  n = 0;
  if (state->audioRate != -1 && n < len) {
      n += snprintf(s+n, len-n, "%s Hz", FLVAudioSampleRates[state->audioRate]);
  }

  if (state->audioSampleBits != -1 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", 
                    FLVAudioSampleSizes[state->audioSampleBits]);
  }

  if (state->audioChannels != -1 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", 
                    FLVAudioChannels[state->audioChannels]);
  }

  if (state->audioCodec != -1 && FLVAudioCodecs[state->audioCodec] != NULL &&
      n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%s", FLVAudioCodecs[state->audioCodec]);
  }

  if (state->audioDataRate != 0.0 && n < len) {
    if (n > 0)
      n += snprintf(s+n, len-n, ", ");
    if (n < len)
      n += snprintf(s+n, len-n, "%.4f kbps", state->audioDataRate);
  }

  if (n == 0) 
    return NULL;
  return strdup(s);
}

struct EXTRACTOR_Keywords *
libextractor_flv_extract (const char *filename,
                          const unsigned char *data,
                          const size_t size, struct EXTRACTOR_Keywords *prev)
{
  struct EXTRACTOR_Keywords *result;
  const unsigned char *ptr;
  const unsigned char *end;

  FLVStreamState state;
  FLVHeader header;
  unsigned long prev_tag_size;
  char *s;

  ptr = data;
  end = ptr + size;

  if (readFLVHeader(&ptr, end, &header) == -1)
    return prev;

  if (memcmp(header.signature, FLV_SIGNATURE, 3) != 0)
    return prev;

  result = prev;
  result = addKeyword (EXTRACTOR_MIMETYPE, strdup ("video/x-flv"), result);

  if (header.version != 1)
    return result;

  if (readPreviousTagSize (&ptr, end, &prev_tag_size) == -1)
    return result;

  state.videoCodec = -1;
  state.videoWidth = -1;
  state.videoHeight = -1;
  state.videoFrameRate = 0.0;
  state.videoDataRate = 0.0;
  state.audioCodec = -1;
  state.audioRate = -1;
  state.audioSampleBits = -1;
  state.audioChannels = -1;
  state.audioDataRate = 0.0;
  while (ptr < end) {
    if (readFLVTag (&ptr, end, &state, &result) == -1)
      break;
    if (readPreviousTagSize (&ptr, end, &prev_tag_size) == -1)
      break;
  }

  s = printVideoFormat (&state);
  if (s != NULL)
    result = addKeyword (EXTRACTOR_FORMAT, s, result);
  s = printAudioFormat (&state);
  if (s != NULL)
    result = addKeyword (EXTRACTOR_FORMAT, s, result);

  return result;
}
