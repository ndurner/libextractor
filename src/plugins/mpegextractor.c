
/*
     This file is part of libextractor.
     (C) 2004, 2005, 2006 Vidyut Samanta and Christian Grothoff

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
#include <mpeg2dec/mpeg2.h>

static void
addKeyword (struct EXTRACTOR_Keywords **list,
            char *keyword, EXTRACTOR_KeywordType type)
{
  EXTRACTOR_KeywordList *next;
  next = malloc (sizeof (EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = strdup (keyword);
  next->keywordType = type;
  *list = next;
}

/* video/mpeg */
struct EXTRACTOR_Keywords *
libextractor_mpeg_extract (const char *filename,
                           const unsigned char *data,
                           size_t size, struct EXTRACTOR_Keywords *prev)
{
  mpeg2dec_t *handle;
  uint8_t *start;
  uint8_t *end;
  const mpeg2_info_t *info;
  mpeg2_state_t state;
  char format[256];

  if ((size < 4) ||
      (!((data[0] == 0x00) &&
         (data[1] == 0x00) &&
         (data[2] == 0x01) && ((data[3] == 0xB3) || (data[3] == 0xBA)))))
    return prev;

  handle = mpeg2_init ();
  if (handle == NULL)
    return prev;
  start = (uint8_t *) data;
  end = (uint8_t *) & data[size];
  mpeg2_buffer (handle, start, end);
  state = mpeg2_parse (handle);
  if (state != STATE_SEQUENCE)
    {
      mpeg2_close (handle);
      return prev;
    }
  info = mpeg2_info (handle);
  if (info == NULL)
    {
      mpeg2_close (handle);
      return prev;
    }
  addKeyword (&prev, "video/mpeg", EXTRACTOR_MIMETYPE);
  if (info->sequence != NULL)
    {
      snprintf (format, 256, "%ux%u",
                info->sequence->width, info->sequence->height);
      addKeyword (&prev, format, EXTRACTOR_SIZE);
      switch (info->sequence->flags & SEQ_VIDEO_FORMAT_UNSPECIFIED)
        {
        case SEQ_VIDEO_FORMAT_PAL:
          addKeyword (&prev, "PAL", EXTRACTOR_FORMAT);
          break;
        case SEQ_VIDEO_FORMAT_NTSC:
          addKeyword (&prev, "NTSC", EXTRACTOR_FORMAT);
          break;
        case SEQ_VIDEO_FORMAT_SECAM:
          addKeyword (&prev, "SECAM", EXTRACTOR_FORMAT);
          break;
        case SEQ_VIDEO_FORMAT_MAC:
          addKeyword (&prev, "MAC", EXTRACTOR_FORMAT);
          break;
        default:
          break;
        }
      if ((info->sequence->flags & SEQ_FLAG_MPEG2) > 0)
        addKeyword (&prev, "MPEG2", EXTRACTOR_RESOURCE_TYPE);
      else
        addKeyword (&prev, "MPEG1", EXTRACTOR_RESOURCE_TYPE);
    }
  if (info->gop != NULL)
    {
      /* this usually does not work yet, since gop's are not
         often at the beginning of the stream (and we
         don't iterate over the stream hoping to find one).
         Hence we usually don't get the size.  Not sure how
         to *efficiently* get the gop (without scanning
         through the entire file) */
      snprintf (format, 256, "%u:%u:%u (%u frames)",
                info->gop->hours,
                info->gop->minutes, info->gop->seconds, info->gop->pictures);
      addKeyword (&prev, format, EXTRACTOR_DURATION);
    }
  mpeg2_close (handle);
  return prev;
}
