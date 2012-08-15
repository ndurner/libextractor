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
 * @file plugins/test_tiff.c
 * @brief testcase for tiff plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the TIFF testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData tiff_haute_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"image/tiff",
	strlen ("image/tiff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ARTIST,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Anders Espersen",
	strlen ("Anders Espersen") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2012:05:15 10:51:47",
	strlen ("2012:05:15 10:51:47") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COPYRIGHT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"© Anders Espersen",
	strlen ("© Anders Espersen") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CAMERA_MAKE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Hasselblad",
	strlen ("Hasselblad") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CAMERA_MODEL,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Hasselblad H4D-31",
	strlen ("Hasselblad H4D-31") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Adobe Photoshop CS5 Macintosh",
	strlen ("Adobe Photoshop CS5 Macintosh") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"4872x6496",
	strlen ("4872x6496") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      /* note that the original test image was almost
	 100 MB large; so for SVN it was cut down to 
	 only contain the first 64 KB, which still parse
	 fine and give use the meta data */
      { "testdata/tiff_haute.tiff",
	tiff_haute_sol },
      { NULL, NULL }
    };
  return ET_main ("tiff", ps);
}

/* end of test_tiff.c */
