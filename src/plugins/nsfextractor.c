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


/* "extract" keyword from a Nes Sound Format file
 *
 *  NSF specification version 1.61 was used,
 *  while this piece of software was originally
 *  written.
 *
 * */
struct EXTRACTOR_Keywords *
libextractor_nsf_extract(const char * filename,
			      char * data,
			      size_t size,
			      struct EXTRACTOR_Keywords * prev) {
  int i;
  char name[33];
  char artist[33];
  char copyright[33];
  char songs[32];
  char startingsong[32];


  /* Check header size and "magic" id bytes */

  if
  (
     size < 0x80 ||
     data[0] != 'N' ||
     data[1] != 'E' ||
     data[2] != 'S' ||
     data[3] != 'M' ||
     data[4] != 0x1a
  )
  {
    return prev;
  }


  /* Mime-type */

  prev = addkword(prev, "audio/x-nsf", EXTRACTOR_MIMETYPE);


  /* Version of NSF format */

  sprintf( startingsong, "%d", data[5] );
  prev = addkword(prev, startingsong, EXTRACTOR_FORMAT_VERSION);


  /* Get song count */

  sprintf( songs, "%d", data[6] );
  prev = addkword(prev, songs, EXTRACTOR_SONG_COUNT);


  /* Get number of the first song to be played */

  sprintf( startingsong, "%d", data[7] );
  prev = addkword(prev, startingsong, EXTRACTOR_STARTING_SONG);


  /* Parse name, artist, copyright fields */

  for( i = 0; i < 32; i++ )
  {
    name[i] = data[ 0x0e + i ];
    artist[i] = data[ 0x2e + i ];
    copyright[i] = data[ 0x4e + i ];
  }

  name[32] = '\0';
  artist[32] = '\0';
  copyright[32] = '\0';

  prev = addkword(prev, name, EXTRACTOR_TITLE);
  prev = addkword(prev, artist, EXTRACTOR_ARTIST);
  prev = addkword(prev, copyright, EXTRACTOR_COPYRIGHT);


  /* PAL or NTSC */

  if( data[0x7a] & 2 )
  {
    prev = addkword(prev, "PAL/NTSC", EXTRACTOR_TELEVISION_SYSTEM);
  }
  else
  {
    if( data[0x7a] & 1 )
    {
      prev = addkword(prev, "PAL", EXTRACTOR_TELEVISION_SYSTEM);
    }
    else
    {
      prev = addkword(prev, "NTSC", EXTRACTOR_TELEVISION_SYSTEM);
    }
  }


  /* Detect Extra Sound Chips needed to play the files */

  if( data[0x7b] & 1 )
  {
    prev = addkword(prev, "uses VRCVI", EXTRACTOR_UNKNOWN);
  }
  if( data[0x7b] & 2 )
  {
    prev = addkword(prev, "uses VRCVII", EXTRACTOR_UNKNOWN);
  }
  if( data[0x7b] & 4 )
  {
    prev = addkword(prev, "uses FDS Sound", EXTRACTOR_UNKNOWN);
  }
  if( data[0x7b] & 8 )
  {
    prev = addkword(prev, "uses MMC5 audio", EXTRACTOR_UNKNOWN);
  }
  if( data[0x7b] & 16 )
  {
    prev = addkword(prev, "uses Namco 106", EXTRACTOR_UNKNOWN);
  }
  if( data[0x7b] & 32 )
  {
    prev = addkword(prev, "uses Sunsoft FME-07", EXTRACTOR_UNKNOWN);
  }

  return prev;
}
