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
 * @file plugins/test_odf.c
 * @brief testcase for odf plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the ODF testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData odf_cg_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/vnd.oasis.opendocument.text",
	strlen ("application/vnd.oasis.opendocument.text") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"OpenOffice.org/3.2$Unix OpenOffice.org_project/320m12$Build-9483",
	strlen ("OpenOffice.org/3.2$Unix OpenOffice.org_project/320m12$Build-9483") + 1,
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
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2005-11-22T11:44:00",
	strlen ("2005-11-22T11:44:00") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UNKNOWN_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2010-06-09T13:09:34",
	strlen ("2010-06-09T13:09:34") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Anhang 1: Profile der beteiligten Wissenschaftler",
	strlen ("Anhang 1: Profile der beteiligten Wissenschaftler") + 1,
	0  
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/odf_cg.odt",
	odf_cg_sol },
      { NULL, NULL }
    };
  return ET_main ("odf", ps);
}

/* end of test_odf.c */
