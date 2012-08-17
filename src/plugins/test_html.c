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
 * @file plugins/test_html.c
 * @brief testcase for html plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the HTML testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData html_grothoff_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Christian Grothoff",
	strlen ("Christian Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_DESCRIPTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Homepage of Christian Grothoff",
	strlen ("Homepage of Christian Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_AUTHOR_NAME,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Christian Grothoff",
	strlen ("Christian Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Christian,Grothoff",
	strlen ("Christian,Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Welcome to Christian Grothoff",
	strlen ("Welcome to Christian Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LANGUAGE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"en",
	strlen ("en") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PUBLISHER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Christian Grothoff",
	strlen ("Christian Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UNKNOWN_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2000-08-20",
	strlen ("2000-08-20") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_RIGHTS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"(C) 2000 by Christian Grothoff",
	strlen ("(C) 2000 by Christian Grothoff") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/html_grothoff.html",
	html_grothoff_sol },
      { NULL, NULL }
    };
  return ET_main ("html", ps);
}

/* end of test_html.c */
