/*
 * This file is part of libextractor.
 * (C) 2006, 2007, 2012 Vidyut Samanta and Christian Grothoff
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
/**
 * @file plugins/sid_extractor.c
 * @brief plugin to support Scream Tracker (S3M) files
 * @author Toni Ruottu
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"


/* SID flags */
#define MUSPLAYER_FLAG   0x01
#define PLAYSID_FLAG     0x02
#define PAL_FLAG         0x04
#define NTSC_FLAG        0x08
#define MOS6581_FLAG     0x10
#define MOS8580_FLAG     0x20

/**
 * A "SID word".
 */
typedef char sidwrd[2];

/**
 * A "SID long".
 */
typedef char sidlongwrd[4];

/**
 * Header of a SID file.
 */
struct header
{
  /**
   * Magic string.
   */
  char magicid[4];

  /**
   * Version number.
   */
  sidwrd sidversion;

  /**
   * Unknown.
   */
  sidwrd dataoffset;

  /**
   * Unknown.
   */
  sidwrd loadaddr;

  /**
   * Unknown.
   */
  sidwrd initaddr;

  /**
   * Unknown.
   */
  sidwrd playaddr;

  /**
   * Number of songs in file.
   */
  sidwrd songs;

  /**
   * Starting song.
   */
  sidwrd firstsong;

  /**
   * Unknown.
   */
  sidlongwrd speed;

  /**
   * Title of the album.
   */
  char title[32];

  /**
   * Name of the artist.
   */
  char artist[32];

  /**
   * Copyright information.
   */
  char copyright[32];

  /* version 2 specific fields start */

  /**
   * Flags
   */
  sidwrd flags;                 

  /**
   * Unknown.
   */
  char startpage;

  /**
   * Unknown.
   */
  char pagelength;

  /**
   * Unknown.
   */
  sidwrd reserved;
};


/**
 * Convert a 'sidword' to an integer.
 *
 * @param data the sidword
 * @return corresponding integer value
 */
static int
sidword (const sidwrd data)
{
  return (unsigned char) data[0] * 0x100 + (unsigned char) data[1];
}


/**
 * Give metadata to LE; return if 'proc' returns non-zero.
 *
 * @param s metadata value as UTF8
 * @param t metadata type to use
 */
#define ADD(s,t) do { if (0 != ec->proc (ec->cls, "sid", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return; } while (0)


/**
 * Extract metadata from SID files.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_sid_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  unsigned int flags;
  int version;
  char album[33];
  char artist[33];
  char copyright[33];
  char songs[32];
  char startingsong[32];
  char sidversion[32];
  const struct header *head;
  void *data;

  if (sizeof (struct header) >
      ec->read (ec->cls,
		&data,
		sizeof (struct header)))
    return;
  head = data;

  /* Check "magic" id bytes */
  if ( (0 != memcmp (head->magicid, "PSID", 4)) &&
       (0 != memcmp (head->magicid, "RSID", 4)) )
    return;

  /* Mime-type */
  ADD ("audio/prs.sid", EXTRACTOR_METATYPE_MIMETYPE);

  /* Version of SID format */
  version = sidword (head->sidversion);
  snprintf (sidversion, 
	    sizeof (sidversion),
	    "%d",
	    version);
  ADD (sidversion, EXTRACTOR_METATYPE_FORMAT_VERSION);

  /* Get song count */
  snprintf (songs,
	    sizeof (songs),
	    "%d", sidword (head->songs));
  ADD (songs, EXTRACTOR_METATYPE_SONG_COUNT);

  /* Get number of the first song to be played */
  snprintf (startingsong,
	    sizeof (startingsong),
	    "%d", 
	    sidword (head->firstsong));
  ADD (startingsong, EXTRACTOR_METATYPE_STARTING_SONG);

  /* name, artist, copyright fields */
  memcpy (&album, head->title, 32);
  album[32] = '\0';
  ADD (album, EXTRACTOR_METATYPE_ALBUM);

  memcpy (&artist, head->artist, 32);
  artist[32] = '\0'; 
  ADD (artist, EXTRACTOR_METATYPE_ARTIST);
  
  memcpy (&copyright, head->copyright, 32);
  copyright[32] = '\0';
  ADD (copyright, EXTRACTOR_METATYPE_COPYRIGHT);

  if (version < 2)
    return;

  /* Version 2 specific options follow
   *
   * Note: Had some troubles understanding specification
   * on the flags in version 2. I hope this is correct.
   */
  flags = sidword (head->flags);
  /* MUS data */
  if (0 != (flags & MUSPLAYER_FLAG))
    ADD ("Compute!'s Sidplayer", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);    

  /* PlaySID data */
  if (0 != (flags & PLAYSID_FLAG))
    ADD ("PlaySID", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);    


  /* PAL or NTSC */
  if (0 != (flags & NTSC_FLAG))
    ADD ("PAL/NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
  else if (0 != (flags & PAL_FLAG))
    ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        

  /* Detect SID Chips suitable for play the files */
  if (0 != (flags & MOS8580_FLAG))
    ADD ("MOS6581/MOS8580", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);        
  else if (0 != (flags & MOS6581_FLAG))
    ADD ("MOS6581", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);        
}

/* end of sid_extractor.c */
