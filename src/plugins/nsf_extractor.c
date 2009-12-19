/*
 * This file is part of libextractor.
 * (C) 2006, 2009 Toni Ruottu
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "platform.h"
#include "extractor.h"
#include "convert.h"


#define HEADER_SIZE  0x80

/* television system flags */

#define PAL_FLAG     0x01
#define DUAL_FLAG    0x02

/* sound chip flags */

#define VRCVI_FLAG   0x01
#define VRCVII_FLAG  0x02
#define FDS_FLAG     0x04
#define MMC5_FLAG    0x08
#define NAMCO_FLAG   0x10
#define SUNSOFT_FLAG 0x20

#define UINT16 unsigned short

struct header
{
  char magicid[5];
  char nsfversion;
  char songs;
  char firstsong;
  UINT16 loadaddr;
  UINT16 initaddr;
  UINT16 playaddr;
  char title[32];
  char artist[32];
  char copyright[32];
  UINT16 ntscspeed;
  char bankswitch[8];
  UINT16 palspeed;
  char tvflags;
  char chipflags;
};

#define ADD(s,t) do { if (0 != proc (proc_cls, "nsf", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)


/* "extract" keyword from a Nes Sound Format file
 *
 * NSF specification version 1.61 was used,
 * while this piece of software was originally
 * written.
 *
 */
int 
EXTRACTOR_nsf_extract (const unsigned char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  char album[33];
  char artist[33];
  char copyright[33];
  char songs[32];
  char startingsong[32];
  char nsfversion[32];
  const struct header *head;
  
  if (size < HEADER_SIZE)    
    return 0;    
  head = (const struct header *) data;
  if (memcmp (head->magicid, "NESM\x1a", 5))
    return 0;
  ADD ("audio/x-nsf", EXTRACTOR_METATYPE_MIMETYPE);
  snprintf (nsfversion,
	    sizeof(nsfversion),
	    "%d", 
	    head->nsfversion);
  ADD (nsfversion, EXTRACTOR_METATYPE_FORMAT_VERSION);
  snprintf (songs, 
	    sizeof(songs),
	    "%d",
	    head->songs);
  ADD (songs, EXTRACTOR_METATYPE_SONG_COUNT);
  snprintf (startingsong, 
	    sizeof(startingsong),
	    "%d", 
	    head->firstsong);
  ADD (startingsong, EXTRACTOR_METATYPE_STARTING_SONG);

  memcpy (&album, head->title, 32);
  album[32] = '\0';
  ADD (album, EXTRACTOR_METATYPE_ALBUM);

  memcpy (&artist, head->artist, 32);
  artist[32] = '\0';
  ADD (artist, EXTRACTOR_METATYPE_ARTIST);

  memcpy (&copyright, head->copyright, 32);
  copyright[32] = '\0';
  ADD (copyright, EXTRACTOR_METATYPE_COPYRIGHT);

  if (head->tvflags & DUAL_FLAG)
    {
      ADD ("PAL/NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
    }
  else
    {
      if (head->tvflags & PAL_FLAG)
	ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
      else
        ADD ("NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
    }

  /* Detect Extra Sound Chips needed to play the files */
  if (head->chipflags & VRCVI_FLAG)    
    ADD ("VRCVI", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);    
  if (head->chipflags & VRCVII_FLAG)
    ADD ("VRCVII", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (head->chipflags & FDS_FLAG)
    ADD ("FDS Sound", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (head->chipflags & MMC5_FLAG)
    ADD ("MMC5 audio", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (head->chipflags & NAMCO_FLAG)
    ADD ("Namco 106", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (head->chipflags & SUNSOFT_FLAG)
    ADD ("Sunsoft FME-07", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  return 0;
}
