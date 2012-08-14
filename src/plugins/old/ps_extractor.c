/*
     This file is part of libextractor.
     (C) 2002, 2003, 2009 Vidyut Samanta and Christian Grothoff

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


static char *
readline (const char *data, size_t size, size_t pos)
{
  size_t end;
  char *res;

  while ((pos < size) &&
         ((data[pos] == (char) 0x0d) || (data[pos] == (char) 0x0a)))
    pos++;

  if (pos >= size)
    return NULL;                /* end of file */
  end = pos;
  while ((end < size) &&
         (data[end] != (char) 0x0d) && (data[end] != (char) 0x0a))
    end++;
  res = malloc (end - pos + 1);
  if (res == NULL)
    return NULL;
  memcpy (res, &data[pos], end - pos);
  res[end - pos] = '\0';

  return res;
}


static int
testmeta (char *line,
          const char *match,
          enum EXTRACTOR_MetaType type, 
	  EXTRACTOR_MetaDataProcessor proc,
	  void *proc_cls)
{
  char *key;

  if ( (strncmp (line, match, strlen (match)) == 0) &&
       (strlen (line) > strlen (match)) )
    {
      if ((line[strlen (line) - 1] == ')') && (line[strlen (match)] == '('))
        {
          key = &line[strlen (match) + 1];
          key[strlen (key) - 1] = '\0'; /* remove ")" */
        }
      else
        {
          key = &line[strlen (match)];
        }
      if (0 != proc (proc_cls,
		     "ps",
		     type,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     key,
		     strlen (key)+1))
	return 1;
    }
  return 0;
}

typedef struct
{
  const char *prefix;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tests[] = {
  {"%%Title: ", EXTRACTOR_METATYPE_TITLE},
  {"%%Author: ", EXTRACTOR_METATYPE_AUTHOR_NAME},
  {"%%Version: ", EXTRACTOR_METATYPE_REVISION_NUMBER},
  {"%%Creator: ", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE},
  {"%%CreationDate: ", EXTRACTOR_METATYPE_CREATION_DATE},
  {"%%Pages: ", EXTRACTOR_METATYPE_PAGE_COUNT},
  {"%%Orientation: ", EXTRACTOR_METATYPE_PAGE_ORIENTATION},
  {"%%DocumentPaperSizes: ", EXTRACTOR_METATYPE_PAPER_SIZE},
  {"%%PageOrder: ", EXTRACTOR_METATYPE_PAGE_ORDER},
  {"%%LanguageLevel: ", EXTRACTOR_METATYPE_FORMAT_VERSION},
  {"%%Magnification: ", EXTRACTOR_METATYPE_MAGNIFICATION},

  /* Also widely used but not supported since they
     probably make no sense:
     "%%BoundingBox: ",
     "%%DocumentNeededResources: ",
     "%%DocumentSuppliedResources: ",
     "%%DocumentProcSets: ",
     "%%DocumentData: ", */

  {NULL, 0}
};

#define PS_HEADER "%!PS-Adobe"

/* mimetype = application/postscript */
int 
EXTRACTOR_ps_extract (const char *data,
		      size_t size,
		      EXTRACTOR_MetaDataProcessor proc,
		      void *proc_cls,
		      const char *options)
{
  size_t pos;
  char *line;
  int i;
  int lastLine;
  int ret;

  pos = strlen (PS_HEADER);
  if ( (size < pos) ||
       (0 != strncmp (PS_HEADER,
		      data,
		      pos)) )
    return 0;
  ret = 0;

  if (0 != proc (proc_cls,
		 "ps",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "application/postscript",
		 strlen ("application/postscript")+1))
    return 1;
  /* skip rest of first line */
  while ((pos < size) && (data[pos] != '\n'))
    pos++;

  lastLine = -1;
  line = NULL;
  /* while Windows-PostScript does not seem to (always?) put
     "%%EndComments", this should allow us to not read through most of
     the file for all the sane applications... For Windows-generated
     PS files, we will bail out at the end of the file. */
  while ( (line == NULL) ||
	  (0 != strncmp ("%%EndComments", line, strlen ("%%EndComments"))) )
    {
      if (line != NULL)
	free (line);
      line = readline (data, size, pos);
      if (line == NULL)
        break;
      i = 0;
      while (tests[i].prefix != NULL)
        {
          ret = testmeta (line, tests[i].prefix, tests[i].type, proc, proc_cls);
	  if (ret != 0)
	    break;
          i++;
        }
      if (ret != 0)
	break;

      /* %%+ continues previous meta-data type... */
      if ( (lastLine != -1) && (0 == strncmp (line, "%%+ ", strlen ("%%+ "))))
        {
          ret = testmeta (line, "%%+ ", tests[lastLine].type, proc, proc_cls);
        }
      else
        {
          /* update "previous" type */
          if (tests[i].prefix == NULL)
            lastLine = -1;
          else
            lastLine = i;
        }
      if (pos + strlen (line) + 1 <= pos)
	break; /* overflow */
      pos += strlen (line) + 1; /* skip newline, too; guarantee progress! */      
    }
  if (line != NULL)
    free (line);
  return ret;
}

/* end of ps_extractor.c */
