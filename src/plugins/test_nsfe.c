/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
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
/**
 * @file plugins/test_nsfe.c
 * @brief testcase for nsfe plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the NSFE testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData nsfe_classics_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/x-nsfe",
	strlen ("audio/x-nsfe") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SONG_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2",
	strlen ("2") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_STARTING_SONG,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"0",
	strlen ("0") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"PAL",
	strlen ("PAL") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ALBUM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Adventures of Dr. Franken,The",
	strlen ("Adventures of Dr. Franken,The") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ARTIST,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Mark Cooksey",
	strlen ("Mark Cooksey") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COPYRIGHT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1993 Motivetime LTD.",
	strlen ("1993 Motivetime LTD.") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_RIPPER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Gil_Galad",
	strlen ("Gil_Galad") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Bach: Prelude & Fugue In C Minor",
	strlen ("Bach: Prelude & Fugue In C Minor") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Beethoven: Moonlight Sonata",
	strlen ("Beethoven: Moonlight Sonata") + 1,
	0 
      },
     { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/nsfe_classics.nsfe",
	nsfe_classics_sol },
      { NULL, NULL }
    };
  return ET_main ("nsfe", ps);
}

/* end of test_nsfe.c */
