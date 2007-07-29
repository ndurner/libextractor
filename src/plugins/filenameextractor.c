/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
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

#include "platform.h"
#include "extractor.h"
#include "convert.h"


/* "extract" the 'filename' as a keyword */
struct EXTRACTOR_Keywords *
libextractor_filename_extract (const char *filename,
                               char *date,
                               size_t size, struct EXTRACTOR_Keywords *prev)
{
  EXTRACTOR_KeywordList *keyword;
  const char *filenameRoot = filename;
  int res;

  /* get filename */
  if (filename != NULL)
    {
      for (res = strlen (filename) - 1; res >= 0; res--)
        if (filename[res] == DIR_SEPARATOR)
          {
            filenameRoot = &filename[res + 1];
            break;
          }
      keyword = malloc (sizeof (EXTRACTOR_KeywordList));
      keyword->next = prev;
      keyword->keyword = convertToUtf8 (filenameRoot,
                                        strlen (filenameRoot),
                                        nl_langinfo (CODESET));
      keyword->keywordType = EXTRACTOR_FILENAME;
      prev = keyword;
    }

  /* get file size */
  if (size > 0)
    {
      keyword = malloc (sizeof (EXTRACTOR_KeywordList));
      keyword->next = prev;
      keyword->keyword = malloc (14);
      keyword->keywordType = EXTRACTOR_FILE_SIZE;

      if (size >= 1000000000)
        snprintf (keyword->keyword, 13, "%.2f %s", size / 1000000000.0,
                  _("GB"));
      else if (size >= 1000000)
        snprintf (keyword->keyword, 13, "%.2f %s", size / 1000000.0, _("MB"));
      else if (size >= 1000)
        snprintf (keyword->keyword, 13, "%.2f %s", size / 1000.0, _("KB"));
      else
        snprintf (keyword->keyword, 13, "%.2f %s", (double) size, _("Bytes"));

      prev = keyword;
    }

  return prev;
}
