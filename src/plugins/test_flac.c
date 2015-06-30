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
 * @file plugins/test_flac.c
 * @brief testcase for flac plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the FLAC testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData kraftwerk_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_RESOURCE_TYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"44100 Hz, 2 channels",
	strlen ("44100 Hz, 2 channels") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Test Title",
	strlen ("Test Title") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct SolutionData alien_sol[] =
    {
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/flac_kraftwerk.flac",
	kraftwerk_sol },
      { "testdata/mpeg_alien.mpg",
	alien_sol },
      { NULL, NULL }
    };
  return ET_main ("flac", ps);
}

/* end of test_flac.c */
