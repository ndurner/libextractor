/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/ps_extractor.c
 * @brief plugin to support PostScript files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"


/**
 * Maximum length of a single line in the PostScript file we're
 * willing to look at.  While the body of the file can have longer
 * lines, this should be a sane limit for the lines in the header with
 * the meta data.
 */
#define MAX_LINE (1024)

/**
 * Header of a PostScript file.
 */
#define PS_HEADER "%!PS-Adobe"


/**
 * Pair with prefix in the PS header and corresponding LE type.
 */
struct Matches
{
  /**
   * PS header prefix.
   */
  const char *prefix;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * Map of PS prefixes to LE types.
 */
static struct Matches tests[] = {
  { "%%Title: ", EXTRACTOR_METATYPE_TITLE },
  { "% Subject: ", EXTRACTOR_METATYPE_SUBJECT },
  { "%%Author: ", EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "% From: ", EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "%%Version: ", EXTRACTOR_METATYPE_REVISION_NUMBER },
  { "%%Creator: ", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  { "%%CreationDate: ", EXTRACTOR_METATYPE_CREATION_DATE },
  { "% Date: ", EXTRACTOR_METATYPE_UNKNOWN_DATE },
  { "%%Pages: ", EXTRACTOR_METATYPE_PAGE_COUNT },
  { "%%Orientation: ", EXTRACTOR_METATYPE_PAGE_ORIENTATION },
  { "%%DocumentPaperSizes: ", EXTRACTOR_METATYPE_PAPER_SIZE },
  { "%%PageOrder: ", EXTRACTOR_METATYPE_PAGE_ORDER },
  { "%%LanguageLevel: ", EXTRACTOR_METATYPE_FORMAT_VERSION },
  { "%%Magnification: ", EXTRACTOR_METATYPE_MAGNIFICATION },

  /* Also widely used but not supported since they
     probably make no sense:
     "%%BoundingBox: ",
     "%%DocumentNeededResources: ",
     "%%DocumentSuppliedResources: ",
     "%%DocumentProcSets: ",
     "%%DocumentData: ", */

  { NULL, 0 }
};


/**
 * Read a single ('\n'-terminated) line of input.
 *
 * @param ec context for IO
 * @return NULL on end-of-file (or if next line exceeds limit)
 */
static char *
readline (struct EXTRACTOR_ExtractContext *ec)
{
  int64_t pos;
  ssize_t ret;
  char *res;
  void *data;
  const char *cdata;
  const char *eol;

  pos = ec->seek (ec->cls, 0, SEEK_CUR);
  if (0 >= (ret = ec->read (ec->cls, &data, MAX_LINE)))
    return NULL;
  cdata = data;
  if (NULL == (eol = memchr (cdata, '\n', ret)))
    return NULL; /* no end-of-line found */
  if (NULL == (res = malloc (eol - cdata + 1)))
    return NULL;
  memcpy (res, cdata, eol - cdata);
  res[eol - cdata] = '\0';
  ec->seek (ec->cls, pos + eol - cdata + 1, SEEK_SET);
  return res;
}


/**
 * Main entry method for the 'application/postscript' extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_ps_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  unsigned int i;
  char *line;
  char *next;
  char *acc;
  const char *match;

  if (NULL == (line = readline (ec)))
    return;
  if ( (strlen (line) < strlen (PS_HEADER)) ||
       (0 != memcmp (PS_HEADER,
		     line,
		     strlen (PS_HEADER))) )
    {
      free (line);
      return;
    }
  free (line);
  if (0 != ec->proc (ec->cls,
		     "ps",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "application/postscript",
		     strlen ("application/postscript") + 1))
    return;

  line = NULL;
  next = readline (ec);
  while ( (NULL != next) &&
	  ('%' == next[0]) )
    {
      line = next;
      next = readline (ec);
      for (i = 0; NULL != tests[i].prefix; i++)
        {
	  match = tests[i].prefix;
	  if ( (strlen (line) < strlen (match)) ||
	       (0 != strncmp (line, match, strlen (match))) )
	    continue;
	  /* %%+ continues previous meta-data type... */
	  while ( (NULL != next) &&
		  (0 == strncmp (next, "%%+", strlen ("%%+"))) )
	    {
	      if (NULL == (acc = malloc (strlen (line) + strlen (next) - 1)))
		break;
	      strcpy (acc, line);
	      strcat (acc, " ");
	      strcat (acc, next + 3);
	      free (line);
	      line = acc;
	      free (next);
	      next = readline (ec);
	    }
	  if ( (line[strlen (line) - 1] == ')') &&
	       (line[strlen (match)] == '(') )
	    {
	      acc = &line[strlen (match) + 1];
	      acc[strlen (acc) - 1] = '\0'; /* remove ")" */
	    }
	  else
	    {
	      acc = &line[strlen (match)];
	    }
	  while (isspace ((unsigned char) acc[0]))
	    acc++;
	  if ( (strlen (acc) > 0) &&
	       (0 != ec->proc (ec->cls,
			       "ps",
			       tests[i].type,
			       EXTRACTOR_METAFORMAT_UTF8,
			       "text/plain",
			       acc,
			       strlen (acc) + 1)) )
	    {
	      free (line);
	      if (NULL != next)
		free (next);
	      return;
	    }
	  break;
	}
      free (line);
    }
  if (NULL != next)
    free (next);
}

/* end of ps_extractor.c */
