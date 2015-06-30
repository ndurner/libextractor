/*
     This file is part of libextractor.
     Copyright (C) 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/
/**
 * @file plugins/test_nsf.c
 * @brief testcase for nsf plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the NSF testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData nsf_arkanoid_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/x-nsf",
	strlen ("audio/x-nsf") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FORMAT_VERSION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1",
	strlen ("1") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SONG_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"26",
	strlen ("26") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_STARTING_SONG,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1",
	strlen ("1") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ALBUM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Arkanoid II - Revenge of Doh",
	strlen ("Arkanoid II - Revenge of Doh") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ARTIST,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"<?>",
	strlen ("<?>") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COPYRIGHT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1988 Taito",
	strlen ("1988 Taito") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"NTSC",
	strlen ("NTSC") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/nsf_arkanoid.nsf",
	nsf_arkanoid_sol },
      { NULL, NULL }
    };
  return ET_main ("nsf", ps);
}

/* end of test_nsf.c */
