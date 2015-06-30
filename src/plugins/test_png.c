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
 * @file plugins/test_png.c
 * @brief testcase for png plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the PNG testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData png_image_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"image/png",
	strlen ("image/png") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"4x4",
	strlen ("4x4") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Testing keyword extraction\n",
	strlen ("Testing keyword extraction\n") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"dc6c58c971715e8043baef058b675eec",
	strlen ("dc6c58c971715e8043baef058b675eec") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/png_image.png",
	png_image_sol },
      { NULL, NULL }
    };
  return ET_main ("png", ps);
}

/* end of test_png.c */
