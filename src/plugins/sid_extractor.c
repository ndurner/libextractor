/*
 * This file is part of libextractor.
 * (C) 2006, 2007 Toni Ruottu
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


#define SID1_HEADER_SIZE 0x76
#define SID2_HEADER_SIZE 0x7c

/* flags */

#define MUSPLAYER_FLAG   0x01
#define PLAYSID_FLAG     0x02
#define PAL_FLAG         0x04
#define NTSC_FLAG        0x08
#define MOS6581_FLAG     0x10
#define MOS8580_FLAG     0x20

typedef char sidwrd[2];
typedef char sidlongwrd[4];

struct header
{
  char magicid[4];
  sidwrd sidversion;
  sidwrd dataoffset;
  sidwrd loadaddr;
  sidwrd initaddr;
  sidwrd playaddr;
  sidwrd songs;
  sidwrd firstsong;
  sidlongwrd speed;
  char title[32];
  char artist[32];
  char copyright[32];
  sidwrd flags;                 /* version 2 specific fields start */
  char startpage;
  char pagelength;
  sidwrd reserved;
};

static int
sidword (const sidwrd data)
{
  int value = (unsigned char) data[0] * 0x100 + (unsigned char) data[1];
  return value;

}

#define ADD(s,t) do { if (0 != proc (proc_cls, "sid", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)

/* "extract" keyword from a SID file
 *
 *  This plugin is based on the nsf extractor
 *
 */

int 
EXTRACTOR_sid_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
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

  /* Check header size */

  if (size < SID1_HEADER_SIZE)
    return 0;
  head = (const struct header *) data;

  /* Check "magic" id bytes */
  if (memcmp (head->magicid, "PSID", 4) &&
      memcmp (head->magicid, "RSID", 4))
    return 0;

  /* Mime-type */
  ADD ("audio/prs.sid", EXTRACTOR_METATYPE_MIMETYPE);

  /* Version of SID format */
  version = sidword (head->sidversion);
  snprintf (sidversion, 
	    sizeof(sidversion),
	    "%d",
	    version);
  ADD (sidversion, EXTRACTOR_METATYPE_FORMAT_VERSION);

  /* Get song count */
  snprintf (songs,
	    sizeof(songs),
	    "%d", sidword (head->songs));
  ADD (songs, EXTRACTOR_METATYPE_SONG_COUNT);

  /* Get number of the first song to be played */
  snprintf (startingsong,
	    sizeof(startingsong),
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


  if ( (version < 2) || (size < SID2_HEADER_SIZE))
    return 0;

  /* Version 2 specific options follow
   *
   * Note: Had some troubles understanding specification
   * on the flags in version 2. I hope this is correct.
   */
  flags = sidword (head->flags);
  /* MUS data */
  if (flags & MUSPLAYER_FLAG)    
    ADD ("Compute!'s Sidplayer", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);    

  /* PlaySID data */
  if (flags & PLAYSID_FLAG)    
    ADD ("PlaySID", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);    


  /* PAL or NTSC */

  if (flags & PAL_FLAG)
    {
      if (flags & NTSC_FLAG)
	ADD ("PAL/NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
      else        
	ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
    }
  else
    {
      if (flags & NTSC_FLAG)
	ADD ("NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
    }

  /* Detect SID Chips suitable for play the files */
  if (flags & MOS6581_FLAG)
    {
      if (flags & MOS8580_FLAG)
	ADD ("MOS6581/MOS8580", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);        
      else
	ADD ("MOS6581", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);        
    }
  else
    {
      if (flags & MOS8580_FLAG)
	ADD ("MOS8580", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);        
    }

  return 0;
}
