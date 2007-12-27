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

/* AMF parser */
/*TODO*/

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
  int videoCodec;;
  int lastFrameType;

  int audioCodec;
  int audioChannels;
  int audioSampleBits;
  int audioRate;
} FLVStreamState;

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

  if (state->audioCodec != soundFormat)
  {
    if (FLVAudioCodecs[soundFormat] != NULL) 
    {
#if DEBUG
      printf("FLV: New AUDIO Codec: %s\n", FLVAudioCodecs[soundFormat]);
#endif
      prev = addKeyword (EXTRACTOR_FORMAT, 
                         strdup (FLVAudioCodecs[soundFormat]),
                         prev);
    }
  }
  state->audioCodec = soundFormat;

  if (state->audioRate != soundRate ||
        state->audioChannels != soundType ||
        state->audioSampleBits != soundSize) {
    char s[48];
#if DEBUG
    printf("FLV: New AUDIO Format\n");
#endif
    snprintf (s, 32, "%s Hz, %s, %s", FLVAudioSampleRates[soundRate], 
                                   FLVAudioSampleSizes[soundSize],
                                   FLVAudioChannels[soundType]);
    prev = addKeyword (EXTRACTOR_FORMAT, strdup (s), prev);
  }
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

  if (state->videoCodec != codecId)
  {
    if (FLVVideoCodecs[codecId] != NULL)
    {
#if DEBUG
      printf("FLV: New VIDEO Codec: %s\n", FLVVideoCodecs[codecId]);
#endif
      prev = addKeyword (EXTRACTOR_FORMAT, 
                         strdup (FLVVideoCodecs[codecId]),
                         prev);
    }
  }
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
      break;
  }

  ptr += header.bodyLength;
  
  *list = head;
  *data = ptr;
  return 0;
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

  if (readPreviousTagSize(&ptr, end, &prev_tag_size) == -1)
    return result;

  state.videoCodec = -1;
  state.audioCodec = -1;
  state.audioRate = -1;
  while (ptr < end) {
    if (readFLVTag(&ptr, end, &state, &result) == -1)
      break;
    if (readPreviousTagSize(&ptr, end, &prev_tag_size) == -1)
      break;
  }
  return result;
}
