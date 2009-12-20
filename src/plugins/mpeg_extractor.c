
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

#include "platform.h"
#include "extractor.h"
#include <mpeg2dec/mpeg2.h>

#define ADD(s,t) do { if (0 != (ret = proc (proc_cls, "mpeg", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto EXIT; } while (0)


/* video/mpeg */
int 
EXTRACTOR_mpeg_extract (const unsigned char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  mpeg2dec_t *handle;
  uint8_t *start;
  uint8_t *end;
  const mpeg2_info_t *info;
  mpeg2_state_t state;
  int ret;
  char format[256];

  if ((size < 4) ||
      (!((data[0] == 0x00) &&
         (data[1] == 0x00) &&
         (data[2] == 0x01) && ((data[3] == 0xB3) || (data[3] == 0xBA)))))
    return 0;

  handle = mpeg2_init ();
  if (handle == NULL)
    return 0;
  start = (uint8_t *) data;
  end = (uint8_t *) & data[size];
  mpeg2_buffer (handle, start, end);
  state = mpeg2_parse (handle);
  if (state != STATE_SEQUENCE)
    {
      mpeg2_close (handle);
      return 0;
    }
  info = mpeg2_info (handle);
  if (info == NULL)
    {
      mpeg2_close (handle);
      return 0;
    }
  ret = 0;
  ADD ("video/mpeg", EXTRACTOR_METATYPE_MIMETYPE);
  if (info->sequence != NULL)
    {
      snprintf (format, 
		sizeof(format), "%ux%u",
                info->sequence->width, info->sequence->height);
      ADD (format, EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
      switch (info->sequence->flags & SEQ_VIDEO_FORMAT_UNSPECIFIED)
        {
        case SEQ_VIDEO_FORMAT_PAL:
          ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
          break;
        case SEQ_VIDEO_FORMAT_NTSC:
          ADD ("NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
          break;
        case SEQ_VIDEO_FORMAT_SECAM:
          ADD ("SECAM", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
          break;
        case SEQ_VIDEO_FORMAT_MAC:
          ADD ("MAC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
          break;
        default:
          break;
        }
      if ((info->sequence->flags & SEQ_FLAG_MPEG2) > 0)
        ADD ("MPEG2", EXTRACTOR_METATYPE_FORMAT_VERSION);
      else
        ADD ("MPEG1", EXTRACTOR_METATYPE_FORMAT_VERSION);
    }
  if (info->gop != NULL)
    {
      /* this usually does not work yet, since gop's are not
         often at the beginning of the stream (and we
         don't iterate over the stream hoping to find one).
         Hence we usually don't get the size.  Not sure how
         to *efficiently* get the gop (without scanning
         through the entire file) */
      snprintf (format, 
		sizeof(format), "%u:%u:%u (%u frames)",
                info->gop->hours,
                info->gop->minutes, info->gop->seconds, info->gop->pictures);
      ADD (format, EXTRACTOR_METATYPE_DURATION);
    }
 EXIT:
  mpeg2_close (handle);
  return ret;
}
