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
 * @file plugins/test_ole2.c
 * @brief testcase for ole2 plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the OLE2 testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData ole2_msword_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_CREATOR,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Nils Durner",
	strlen ("Nils Durner") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UNKNOWN_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2005-03-21T06:11:12Z",
	strlen ("2005-03-21T06:11:12Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_DESCRIPTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"This is a small document to test meta data extraction by GNU libextractor.",
	strlen ("This is a small document to test meta data extraction by GNU libextractor.") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"ole ole2 eole2extractor",
	strlen ("ole ole2 eole2extractor") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUBJECT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"GNU libextractor",
	strlen ("GNU libextractor") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Testcase for the ole2 extractor",
	strlen ("Testcase for the ole2 extractor") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LAST_SAVED_BY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Nils Durner",
	strlen ("Nils Durner") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2005-03-21T06:10:19Z",
	strlen ("2005-03-21T06:10:19Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_EDITING_CYCLES,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2",
	strlen ("2") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };

  struct SolutionData ole2_starwriter_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_CREATOR,
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
	"2004-09-24T02:54:31Z",
	strlen ("2004-09-24T02:54:31Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_DESCRIPTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The comments",
	strlen ("The comments") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The Keywords",
	strlen ("The Keywords") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUBJECT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The Subject",
	strlen ("The Subject") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The Title",
	strlen ("The Title") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LAST_SAVED_BY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Christian Grothoff",
	strlen ("Christian Grothoff") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2004-09-24T02:53:15Z",
	strlen ("2004-09-24T02:53:15Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_EDITING_CYCLES,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"4",
	strlen ("4") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The Title",
	strlen ("The Title") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUBJECT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The Subject",
	strlen ("The Subject") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The comments",
	strlen ("The comments") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"The Keywords",
	strlen ("The Keywords") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };

  struct SolutionData ole2_blair_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_LANGUAGE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"U.S. English",
	strlen ("U.S. English") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATOR,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"default",
	strlen ("default") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UNKNOWN_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2003-02-03T11:18:00Z",
	strlen ("2003-02-03T11:18:00Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Iraq- ITS INFRASTRUCTURE OF CONCEALMENT, DECEPTION AND INTIMIDATION",
	strlen ("Iraq- ITS INFRASTRUCTURE OF CONCEALMENT, DECEPTION AND INTIMIDATION") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CHARACTER_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"22090",
	strlen ("22090") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LAST_SAVED_BY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"MKhan",
	strlen ("MKhan") + 1,
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
	EXTRACTOR_METATYPE_WORD_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"3875",
	strlen ("3875") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2003-02-03T09:31:00Z",
	strlen ("2003-02-03T09:31:00Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_EDITING_CYCLES,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"4",
	strlen ("4") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/vnd.ms-files",
	strlen ("application/vnd.ms-files") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Microsoft Word 8.0",
	strlen ("Microsoft Word 8.0") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TEMPLATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Normal.dot",
	strlen ("Normal.dot") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LINE_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"184",
	strlen ("184") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PARAGRAPH_COUNT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"44",
	strlen ("44") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #0: Author `cic22' worked on `C:\\DOCUME~1\\phamill\\LOCALS~1\\Temp\\AutoRecovery save of Iraq - security.asd'",
	strlen ("Revision #0: Author `cic22' worked on `C:\\DOCUME~1\\phamill\\LOCALS~1\\Temp\\AutoRecovery save of Iraq - security.asd'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #1: Author `cic22' worked on `C:\\DOCUME~1\\phamill\\LOCALS~1\\Temp\\AutoRecovery save of Iraq - security.asd'",
	strlen ("Revision #1: Author `cic22' worked on `C:\\DOCUME~1\\phamill\\LOCALS~1\\Temp\\AutoRecovery save of Iraq - security.asd'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #2: Author `cic22' worked on `C:\\DOCUME~1\\phamill\\LOCALS~1\\Temp\\AutoRecovery save of Iraq - security.asd'",
	strlen ("Revision #2: Author `cic22' worked on `C:\\DOCUME~1\\phamill\\LOCALS~1\\Temp\\AutoRecovery save of Iraq - security.asd'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #3: Author `JPratt' worked on `C:\\TEMP\\Iraq - security.doc'",
	strlen ("Revision #3: Author `JPratt' worked on `C:\\TEMP\\Iraq - security.doc'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #4: Author `JPratt' worked on `A:\\Iraq - security.doc'",
	strlen ("Revision #4: Author `JPratt' worked on `A:\\Iraq - security.doc'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #5: Author `ablackshaw' worked on `C:\\ABlackshaw\\Iraq - security.doc'",
	strlen ("Revision #5: Author `ablackshaw' worked on `C:\\ABlackshaw\\Iraq - security.doc'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #6: Author `ablackshaw' worked on `C:\\ABlackshaw\\A;Iraq - security.doc'",
	strlen ("Revision #6: Author `ablackshaw' worked on `C:\\ABlackshaw\\A;Iraq - security.doc'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #7: Author `ablackshaw' worked on `A:\\Iraq - security.doc'",
	strlen ("Revision #7: Author `ablackshaw' worked on `A:\\Iraq - security.doc'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #8: Author `MKhan' worked on `C:\\TEMP\\Iraq - security.doc'",
	strlen ("Revision #8: Author `MKhan' worked on `C:\\TEMP\\Iraq - security.doc'") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_REVISION_HISTORY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Revision #9: Author `MKhan' worked on `C:\\WINNT\\Profiles\\mkhan\\Desktop\\Iraq.doc'",
	strlen ("Revision #9: Author `MKhan' worked on `C:\\WINNT\\Profiles\\mkhan\\Desktop\\Iraq.doc'") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };

  struct SolutionData ole2_excel_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_CREATOR,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"JV",
	strlen ("JV") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LAST_SAVED_BY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"JV",
	strlen ("JV") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2002-03-20T21:26:28Z",
	strlen ("2002-03-20T21:26:28Z") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/vnd.ms-files",
	strlen ("application/vnd.ms-files") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Microsoft Excel",
	strlen ("Microsoft Excel") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };

  struct ProblemSet ps[] =
    {
      { "testdata/ole2_msword.doc",
	ole2_msword_sol },
      { "testdata/ole2_starwriter40.sdw",
	ole2_starwriter_sol },
      { "testdata/ole2_blair.doc",
	ole2_blair_sol },
      { "testdata/ole2_excel.xls",
	ole2_excel_sol },
      { NULL, NULL }
    };
  return ET_main ("ole2", ps);
}

/* end of test_ole2.c */
