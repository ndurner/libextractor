/*
 * This file is part of libextractor.
 * (C) 2007 Toni Ruottu
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

#define HEADER_SIZE  0x04

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
	char magicid[ 4 ];
};

struct infochunk
{
	UINT16 loadaddr;
	UINT16 initaddr;
	UINT16 playaddr;
	char tvflags;
	char chipflags;
	char songs;
	char firstsong;
};

static int nsfeuint(const char * data )
{
	int i, value = 0;

	for( i = 3; i > 0 ; i-- )
	{
		value += ( unsigned char ) data[ i ];
		value *= 0x100;
	}

	value += ( unsigned char ) data[ 0 ];

	return( value );
}

static char * nsfestring(const char * data, int size )
{
	char *s;
	int length;

	if( size < strlen( data ) )
	{
		length = size;
	}
	else
	{
		length = strlen( data );
	}

	s = malloc( length + 1 );

	if( data != NULL )
	{
		strncpy( s, data, length );
	}

	s[ strlen( data ) ] = '\0';

	return( s );
}

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

static struct EXTRACTOR_Keywords * libextractor_nsfe_info_extract
(
	const char * data,
	size_t size,
	struct EXTRACTOR_Keywords * prev
)
{
        const struct infochunk *ichunk;
	char songs[ 32 ];

	ichunk = (const struct infochunk * ) data;

	if( size < 8 )
	{
		return( prev );
	}


	/* PAL or NTSC */

	if( ichunk->tvflags & DUAL_FLAG )
	{
		prev = addkword( prev, "PAL/NTSC", EXTRACTOR_TELEVISION_SYSTEM );
	}
	else
	{
		if( ichunk->tvflags & PAL_FLAG )
		{
			prev = addkword( prev, "PAL", EXTRACTOR_TELEVISION_SYSTEM );
		}
		else
		{
			prev = addkword( prev, "NTSC", EXTRACTOR_TELEVISION_SYSTEM );
		}
	}


	/* Detect Extra Sound Chips needed to play the files */

	if( ichunk->chipflags & VRCVI_FLAG )
	{
		prev = addkword( prev, "VRCVI", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( ichunk->chipflags & VRCVII_FLAG )
	{
		prev = addkword( prev, "VRCVII", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( ichunk->chipflags & FDS_FLAG )
	{
		prev = addkword( prev, "FDS Sound", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( ichunk->chipflags & MMC5_FLAG )
	{
		prev = addkword( prev, "MMC5 audio", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( ichunk->chipflags & NAMCO_FLAG )
	{
		prev = addkword( prev, "Namco 106", EXTRACTOR_HARDWARE_DEPENDENCY );
	}
	if( ichunk->chipflags & SUNSOFT_FLAG )
	{
		prev = addkword( prev, "Sunsoft FME-07", EXTRACTOR_HARDWARE_DEPENDENCY );
	}

	if( size < 9 )
	{
		prev = addkword( prev, "1", EXTRACTOR_SONG_COUNT );
		return( prev );
	}

	sprintf( songs, "%d", ichunk->songs );
	prev = addkword( prev, songs, EXTRACTOR_SONG_COUNT );


	return( prev );
}


static struct EXTRACTOR_Keywords * libextractor_nsfe_tlbl_extract
(
	const char * data,
	size_t size,
	struct EXTRACTOR_Keywords * prev
)
{
	char * title;
	int left, length;


	for( left = size; left > 0; left -= length )
	{
		title = nsfestring( &data[ size - left ], left );
		prev = addkword( prev, title, EXTRACTOR_TITLE );
		length = strlen( title ) + 1;

		free(title);
	}

	return( prev );
}

static struct EXTRACTOR_Keywords * libextractor_nsfe_auth_extract
(
        const char * data,
	size_t size,
	struct EXTRACTOR_Keywords * prev
)
{
	char * album;
	char * artist;
	char * copyright;
	char * ripper;
	int left = size;

	if( left < 1 )
	{
		return( prev );
	}

	album = nsfestring( &data[ size - left ], left );
	prev = addkword( prev, album, EXTRACTOR_ALBUM );
	
	left -= ( strlen( album ) + 1 );
	free(album);

	if( left < 1 )
	{
		return( prev );
	}

	artist = nsfestring( &data[ size - left ], left );
	prev = addkword( prev, artist, EXTRACTOR_ARTIST );

	left -= ( strlen( artist ) + 1 );
	free(artist);

	if( left < 1 )
	{
		return( prev );
	}

	copyright = nsfestring( &data[ size - left ], left );
	prev = addkword( prev, copyright, EXTRACTOR_COPYRIGHT );

	left -= ( strlen( copyright ) + 1 );
	free(copyright);

	if( left < 1 )
	{
		return( prev );
	}

	ripper = nsfestring( &data[ size - left ], left );
	prev = addkword( prev, ripper, EXTRACTOR_RIPPER );
	free(ripper);

	return( prev );
}


/* "extract" keyword from an Extended Nintendo Sound Format file
 *
 * NSFE specification revision 2 (Sep. 3, 2003)
 * was used, while this piece of software was
 * originally written.
 *
 */
struct EXTRACTOR_Keywords * libextractor_nsfe_extract
(
	const char * filename,
	const char * data,
	size_t size,
	struct EXTRACTOR_Keywords * prev
)
{
	const struct header *head;
	int i;
	char chunkid[ 5 ] = "     ";

	/* Check header size */

	if( size < HEADER_SIZE )
	{
		return( prev );
	}

	head = (const struct header * ) data;

	/* Check "magic" id bytes */

	if( memcmp( head->magicid, "NSFE", 4 ) )
	{
		return( prev );
	}


	/* Mime-type */

	prev = addkword( prev, "audio/x-nsfe", EXTRACTOR_MIMETYPE );

	i = 4; /* Jump over magic id */

	while( i + 7 < size && strncmp( chunkid, "NEND", 4 ) ) /* CHECK */
	{

		unsigned int chunksize = nsfeuint( &data[ i ] );

		i += 4; /* Jump over chunk size */

		memcpy( &chunkid, data + i, 4 );
		chunkid[ 4 ] = '\0';

		i += 4; /* Jump over chunk id */

		if( ! strncmp( chunkid, "INFO", 4 ) )
		{
			prev = libextractor_nsfe_info_extract
			(
				data + i,
				chunksize,
				prev
			);
		}

		if( ! strncmp( chunkid, "auth", 4 ) )
		{
			prev = libextractor_nsfe_auth_extract
			(
				data + i,
				chunksize,
				prev
			);
		}

		if( ! strncmp( chunkid, "tlbl", 4 ) )
		{
			prev = libextractor_nsfe_tlbl_extract
			(
				data + i,
				chunksize,
				prev
			);
		}

		/* Ignored chunks: DATA, NEND, plst, time, fade, BANK */

		i += chunksize;
		
	}

	return( prev );

}
