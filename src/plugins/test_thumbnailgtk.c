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
 * @file plugins/test_thumbnailgtkj.c
 * @brief testcase for thumbnailgtkj plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the THUMBNAILGTKJ testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  uint8_t thumbnail_data[] = { 137, 80, 78, 71 /* rest omitted */ };
  struct SolutionData thumbnail_torsten_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1600x1200",
	strlen ("1600x1200") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_THUMBNAIL,
	EXTRACTOR_METAFORMAT_BINARY,
	"image/png",
	(void *) thumbnail_data, 
	sizeof (thumbnail_data),
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/thumbnail_torsten.jpg",
	thumbnail_torsten_sol },
      { NULL, NULL }
    };
  return ET_main ("thumbnailgtk", ps);
}

/* end of test_thumbnailgtk.c */
