/*
 * This file is part of libextractor.
 * (C) 2006 Toni Ruottu
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
	char magicid[ 5 ];
	char nsfversion;
	char songs;
	char firstsong;
	UINT16 loadaddr;
	UINT16 initaddr;
	UINT16 playaddr;
	char title[ 32 ];
	char artist[ 32 ];
	char copyright[ 32 ];
	UINT16 ntscspeed;
	char bankswitch[ 8 ];
	UINT16 palspeed;
	char tvflags;
	char chipflags;
};


static struct EXTRACTOR_Keywords * addkword
(
	EXTRACTOR_KeywordList *oldhead,
	const char * phrase,
	EXTRACTOR_KeywordType type
)
{
	EXTRACTOR_KeywordList * keyword;

	keyword = malloc( sizeof( EXTRACTOR_KeywordList ) );
	keyword->next = oldhead;
	keyword->keyword = strdup( phrase );
	keyword->keywordType = type;
	return( keyword );
}


/* "extract" keyword from a Nes Sound Format file
 *
 * NSF specification version 1.61 was used,
 * while this piece of software was originally
 * written.
 *
 */
struct EXTRACTOR_Keywords * libextractor_nsf_extract
(
	const char * filename,
	char * data,
	size_t size,
	struct EXTRACTOR_Keywords * prev
)
{
	char album[ 33 ];
	char artist[ 33 ];
	char copyright[ 33 ];
	char songs[ 32 ];
	char startingsong[ 32 ];
	char nsfversion[ 32 ];
	struct header *head;

	/* Check header size */

	if( size < HEADER_SIZE )
	{
		return( prev );
	}

	head = ( struct header * ) data;

	/* Check "magic" id bytes */

	if( memcmp( head->magicid, "NESM\x1a", 5 ) )
	{
		return( prev );
	}


	/* Mime-type */

	prev = addkword( prev, "audio/x-nsf", EXTRACTOR_MIMETYPE );


	/* Version of NSF format */

	sprintf( nsfversion, "%d", head->nsfversion );
	prev = addkword( prev, nsfversion, EXTRACTOR_FORMAT_VERSION );


	/* Get song count */

	sprintf( songs, "%d", head->songs );
	prev = addkword( prev, songs, EXTRACTOR_SONG_COUNT );


	/* Get number of the first song to be played */

	sprintf( startingsong, "%d", head->firstsong );
	prev = addkword( prev, startingsong, EXTRACTOR_STARTING_SONG );


	/* name, artist, copyright fields */

	memcpy( &album, head->title, 32 );
	memcpy( &artist, head->artist, 32 );
	memcpy( &copyright, head->copyright, 32 );

	album[ 32 ] = '\0';
	artist[ 32 ] = '\0';
	copyright[ 32 ] = '\0';

	prev = addkword( prev, album, EXTRACTOR_ALBUM );
	prev = addkword( prev, artist, EXTRACTOR_ARTIST );
	prev = addkword( prev, copyright, EXTRACTOR_COPYRIGHT );


	/* PAL or NTSC */

	if( head->tvflags & DUAL_FLAG )
	{
		prev = addkword( prev, "PAL/NTSC", EXTRACTOR_TELEVISION_SYSTEM );
	}
	else
	{
		if( head->tvflags & PAL_FLAG )
		{
			prev = addkword( prev, "PAL", EXTRACTOR_TELEVISION_SYSTEM );
		}
		else
		{
			prev = addkword( prev, "NTSC", EXTRACTOR_TELEVISION_SYSTEM );
		}
	}


	/* Detect Extra Sound Chips needed to play the files */

	if( head->chipflags & VRCVI_FLAG )
	{
		prev = addkword( prev, "VRCVI", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( head->chipflags & VRCVII_FLAG )
	{
		prev = addkword( prev, "VRCVII", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( head->chipflags & FDS_FLAG )
	{
		prev = addkword( prev, "FDS Sound", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( head->chipflags & MMC5_FLAG )
	{
		prev = addkword( prev, "MMC5 audio", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( head->chipflags & NAMCO_FLAG )
	{
		prev = addkword( prev, "Namco 106", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( head->chipflags & SUNSOFT_FLAG )
	{
		prev = addkword( prev, "Sunsoft FME-07", EXTRACTOR_HARDWARE_DEPENDENCY );
	}

	return( prev );

}
