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
 * @file plugins/test_dvi.c
 * @brief testcase for dvi plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the DVI testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData dvi_ora_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/x-dvi",
	strlen ("application/x-dvi") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PAGE_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"10",
	strlen ("10") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"Optimal Bitwise Register Allocation using Integer Linear Programming",
	strlen ("Optimal Bitwise Register Allocation using Integer Linear Programming") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUBJECT,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"Register Allocation",
	strlen ("Register Allocation") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	" TeX output 2005.02.06:0725",
	strlen (" TeX output 2005.02.06:0725") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"LaTeX with hyperref package",
	strlen ("LaTeX with hyperref package") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_AUTHOR_NAME,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"Rajkishore Barik and Christian Grothoff and Rahul Gupta and Vinayaka Pandit and Raghavendra Udupa",
	strlen ("Rajkishore Barik and Christian Grothoff and Rahul Gupta and Vinayaka Pandit and Raghavendra Udupa") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"dvips + Distiller",
	strlen ("dvips + Distiller") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_C_STRING,
	"text/plain",
	"register allocation integer linear programming bit-wise spilling coalesing rematerialization",
	strlen ("register allocation integer linear programming bit-wise spilling coalesing rematerialization") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/dvi_ora.dvi",
	dvi_ora_sol },
      { NULL, NULL }
    };
  return ET_main ("dvi", ps);
}

/* end of test_dvi.c */
