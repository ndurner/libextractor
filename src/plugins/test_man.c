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
 * @file plugins/test_man.c
 * @brief testcase for man plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the MAN testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData man_extract_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"EXTRACT",
	strlen ("EXTRACT") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SECTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	_("Commands"),
	strlen (_("Commands")) + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_MODIFICATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Aug 7, 2012",
	strlen ("Aug 7, 2012") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SOURCE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	_("libextractor 0.7.0"),
	strlen (_("libextractor 0.7.0")) + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/man_extract.1",
	man_extract_sol },
      { NULL, NULL }
    };
  return ET_main ("man", ps);
}

/* end of test_man.c */
