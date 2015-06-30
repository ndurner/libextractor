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
 * @file plugins/test_zip.c
 * @brief testcase for zip plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the ZIP testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData zip_test_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/zip",
	strlen ("application/zip") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"global zipfile comment",
	strlen ("global zipfile comment") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FILENAME,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"ChangeLog",
	strlen ("ChangeLog") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FILENAME,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"test.png",
	strlen ("test.png") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"comment for test.png",
	strlen ("comment for test.png") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FILENAME,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"test.jpg",
	strlen ("test.jpg") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"comment for test.jpg",
	strlen ("comment for test.jpg") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/zip_test.zip",
	zip_test_sol },
      { NULL, NULL }
    };
  return ET_main ("zip", ps);
}

/* end of test_zip.c */
