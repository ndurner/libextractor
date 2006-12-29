/*
     This file is part of libextractor.
     (C) 2006 Toni Ruottu

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
#include "convert.h"


static struct EXTRACTOR_Keywords *
addkword(EXTRACTOR_KeywordList *oldhead,
         const char * phrase,
         EXTRACTOR_KeywordType type) {
   EXTRACTOR_KeywordList * keyword;

   keyword = malloc(sizeof(EXTRACTOR_KeywordList));
   keyword->next = oldhead;
   keyword->keyword = strdup(phrase);
   keyword->keywordType = type;
   return keyword;
}


/* "extract" keyword from a SID file
 *
 *  This plugin is based on the nsf extractor
 *
 * */
struct EXTRACTOR_Keywords *
libextractor_sid_extract(const char * filename,
			      char * data,
			      size_t size,
			      struct EXTRACTOR_Keywords * prev) {
  int i, version;
  char album[33];
  char artist[33];
  char copyright[33];
  char songs[32];
  char startingsong[32];


  /* Check header size and "magic" id bytes */

  if
  (
     size < 0x76 ||
     ( data[0] != 'P' && data[0] != 'R' ) ||
     data[1] != 'S' ||
     data[2] != 'I' ||
     data[3] != 'D'
  )
  {
    return prev;
  }


  /* Mime-type */

  prev = addkword(prev, "audio/prs.sid", EXTRACTOR_MIMETYPE);

  /* Version of SID format */

  version = data[4] * 0x100 + data[5];
  sprintf( startingsong, "%d", version );
  prev = addkword(prev, startingsong, EXTRACTOR_FORMAT_VERSION);


  /* Get song count */

  sprintf( songs, "%d", data[0x0e] * 0x100 + data[0x0f] );
  prev = addkword(prev, songs, EXTRACTOR_SONG_COUNT);


  /* Get number of the first song to be played */

  sprintf( startingsong, "%d", data[0x10] * 0x100 + data[0x11] );
  prev = addkword(prev, startingsong, EXTRACTOR_STARTING_SONG);


  /* Parse album, artist, copyright fields */

  for( i = 0; i < 32; i++ )
  {
    album[i] = data[ 0x16 + i ];
    artist[i] = data[ 0x36 + i ];
    copyright[i] = data[ 0x56 + i ];
  }

  album[32] = '\0';
  artist[32] = '\0';
  copyright[32] = '\0';

  prev = addkword(prev, album, EXTRACTOR_ALBUM);
  prev = addkword(prev, artist, EXTRACTOR_ARTIST);
  prev = addkword(prev, copyright, EXTRACTOR_COPYRIGHT);


  if( version < 2 || size < 0x7c )
  {
    return prev;
  }

  /* Version 2 specific options follow
   *
   * Note: Had some troubles understanding specification
   * on the flags in version 2. I hope this is correct.
   *
   */

  /* PAL or NTSC */

  if( data[0x77] & 16 )
  {
    if( data[0x77] & 32 )
    {
      prev = addkword(prev, "PAL/NTSC", EXTRACTOR_TELEVISION_SYSTEM);
    }
    else
    {
      prev = addkword(prev, "PAL", EXTRACTOR_TELEVISION_SYSTEM);
    }
  }
  else
  {
    if( data[0x77] & 32 )
    {
      prev = addkword(prev, "NTSC", EXTRACTOR_TELEVISION_SYSTEM);
    }
  }

  /* Detect SID Chips suitable for play the files */

  if( data[0x77] & 4 )
  {
    if( data[0x77] & 8 )
    {
      prev = addkword(prev, "MOS6581/MOS8580", EXTRACTOR_HARDWARE_DEPENDENCY);
    }
    else
    {
      prev = addkword(prev, "MOS6581", EXTRACTOR_HARDWARE_DEPENDENCY);
    }
  }
  else
  {
    if( data[0x77] & 8 )
    {
      prev = addkword(prev, "MOS8580", EXTRACTOR_HARDWARE_DEPENDENCY);
    }
  }

  return prev;
}
