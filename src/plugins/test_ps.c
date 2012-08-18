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
 * @file plugins/test_ps.c
 * @brief testcase for ps plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the PS testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData ps_bloomfilter_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/postscript",
	strlen ("application/postscript") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"A Quick Introduction to Bloom Filters",
	strlen ("A Quick Introduction to Bloom Filters") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"dvips(k) 5.92b Copyright 2002 Radical Eye Software",
	strlen ("dvips(k) 5.92b Copyright 2002 Radical Eye Software") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PAGE_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1",
	strlen ("1") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PAGE_ORDER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Ascend",
	strlen ("Ascend") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct SolutionData ps_wallace_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/postscript",
	strlen ("application/postscript") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUBJECT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"PS preprint of JPEG article submitted to IEEE Trans on Consum. Elect",
	strlen ("PS preprint of JPEG article submitted to IEEE Trans on Consum. Elect") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"DECwrite V1.1 Copyright (c) 1990 DIGITAL EQUIPMENT CORPORATION.   All Rights Reserved.",
	strlen ("DECwrite V1.1 Copyright (c) 1990 DIGITAL EQUIPMENT CORPORATION.   All Rights Reserved.") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_AUTHOR_NAME,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Greg Wallace",
	strlen ("Greg Wallace") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UNKNOWN_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Tue, 17 Dec 91 14:49:50 PST",
	strlen ("Tue, 17 Dec 91 14:49:50 PST") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/ps_bloomfilter.ps",
	ps_bloomfilter_sol },
      { "testdata/ps_wallace.ps",
	ps_wallace_sol },
      { NULL, NULL }
    };
  return ET_main ("ps", ps);
}

/* end of test_ps.c */
