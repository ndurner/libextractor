/*
 * This file is part of libextractor.
 * Copyright (C) 2006, 2009, 2012 Toni Ruottu and Christian Grothoff
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3, or (at your
 * option) any later version.
 *
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/**
 * @file plugins/nsf_extractor.c
 * @brief plugin to support Nes Sound Format files
 * @author Toni Ruottu
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"



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


/**
 * Header of an NSF file.
 */
struct header
{
  /**
   * Magic code.
   */
  char magicid[5];

  /**
   * NSF version number.
   */
  char nsfversion;

  /**
   * Number of songs.
   */
  unsigned char songs;

  /**
   * Starting song.
   */
  unsigned char firstsong;

  /**
   * Unknown.
   */
  uint16_t loadaddr;

  /**
   * Unknown.
   */
  uint16_t initaddr;

  /**
   * Unknown.
   */
  uint16_t playaddr;

  /**
   * Album title.
   */
  char title[32];

  /**
   * Artist name.
   */
  char artist[32];

  /**
   * Copyright information.
   */
  char copyright[32];

  /**
   * Unknown.
   */
  uint16_t ntscspeed;

  /**
   * Unknown.
   */
  char bankswitch[8];

  /**
   * Unknown.
   */
  uint16_t palspeed;

  /**
   * Flags for TV encoding.
   */
  char tvflags;

  /**
   * Flags about the decoder chip.
   */
  char chipflags;
};


/**
 * Give metadata to LE; return if 'proc' returns non-zero.
 *
 * @param s metadata value as UTF8
 * @param t metadata type to use
 */
#define ADD(s,t) do { if (0 != ec->proc (ec->cls, "nsf", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return; } while (0)


/**
 * "extract" meta data from a Nes Sound Format file
 *
 * NSF specification version 1.61 was used, while this piece of
 * software was originally written.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_nsf_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  char album[33];
  char artist[33];
  char copyright[33];
  char songs[32];
  char startingsong[32];
  char nsfversion[32];
  const struct header *head;
  void *data;
  ssize_t ds;

  ds = ec->read (ec->cls,
                 &data,
                 sizeof (struct header));
  if ( (-1 == ds) ||
       (sizeof (struct header) > ds) )
    return;
  head = data;

  /* Check "magic" id bytes */
  if (memcmp (head->magicid, "NESM\x1a", 5))
    return;
  ADD ("audio/x-nsf", EXTRACTOR_METATYPE_MIMETYPE);
  snprintf (nsfversion,
	    sizeof(nsfversion),
	    "%d",
	    head->nsfversion);
  ADD (nsfversion, EXTRACTOR_METATYPE_FORMAT_VERSION);
  snprintf (songs,
	    sizeof(songs),
	    "%d",
	    (int) head->songs);
  ADD (songs, EXTRACTOR_METATYPE_SONG_COUNT);
  snprintf (startingsong,
	    sizeof(startingsong),
	    "%d",
	    (int) head->firstsong);
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

  if (0 != (head->tvflags & DUAL_FLAG))
    {
      ADD ("PAL/NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
    }
  else
    {
      if (0 != (head->tvflags & PAL_FLAG))
	ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
      else
        ADD ("NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
    }

  /* Detect Extra Sound Chips needed to play the files */
  if (0 != (head->chipflags & VRCVI_FLAG))
    ADD ("VRCVI", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (head->chipflags & VRCVII_FLAG))
    ADD ("VRCVII", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (head->chipflags & FDS_FLAG))
    ADD ("FDS Sound", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (head->chipflags & MMC5_FLAG))
    ADD ("MMC5 audio", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (head->chipflags & NAMCO_FLAG))
    ADD ("Namco 106", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (head->chipflags & SUNSOFT_FLAG))
    ADD ("Sunsoft FME-07", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
}

/* end of nsf_extractor.c */
