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
 * @file plugins/test_mpeg.c
 * @brief testcase for mpeg plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the MPEG testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData melt_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"video/mpeg",
	strlen ("video/mpeg") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"320x208",
	strlen ("320x208") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FORMAT_VERSION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"MPEG1",
	strlen ("MPEG1") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_DURATION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"00:00:03 (22 frames)",
	strlen ("00:00:03 (22 frames)") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct SolutionData alien_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"video/mpeg",
	strlen ("video/mpeg") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"320x240",
	strlen ("320x240") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FORMAT_VERSION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"MPEG1",
	strlen ("MPEG1") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/mpeg_melt.mpg",
	melt_sol },
      { "testdata/mpeg_alien.mpg",
	alien_sol },
      { NULL, NULL }
    };
  return ET_main ("mpeg", ps);
}

/* end of test_mpeg.c */
