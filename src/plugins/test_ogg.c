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
 * @file plugins/test_ogg.c
 * @brief testcase for ogg plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the OGG testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData courseclear_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/ogg",
	strlen ("application/ogg") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_VENDOR,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Xiphophorus libVorbis I 20010813",
	strlen ("Xiphophorus libVorbis I 20010813") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"xoc_SMW_06_courseclear",
	strlen ("xoc_SMW_06_courseclear") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ARTIST,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"XOC",
	strlen ("XOC") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TRACK_NUMBER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"6",
	strlen ("6") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/ogg_courseclear.ogg",
	courseclear_sol },
      { NULL, NULL }
    };
  return ET_main ("ogg", ps);
}

/* end of test_ogg.c */
