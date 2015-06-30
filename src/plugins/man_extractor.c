/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/man_extractor.c
 * @brief plugin to support man pages
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <ctype.h>


/**
 * Create string from first 'n' characters of 'str'.  See 'strndup'.
 *
 * @param str input string
 * @param n desired output length (plus 0-termination)
 * @return copy of first 'n' bytes from 'str' plus 0-terminator, NULL on error
 */
static char *
stndup (const char *str, size_t n)
{
  char *tmp;

  if (NULL == (tmp = malloc (n + 1)))
    return NULL;
  tmp[n] = '\0';
  memcpy (tmp, str, n);
  return tmp;
}


/**
 * Give a metadata item to LE.  Removes double-quotes and
 * makes sure we don't pass empty strings or NULL pointers.
 *
 * @param type metadata type to use
 * @param keyword metdata value; freed in the process
 * @param proc function to call with meta data
 * @param proc_cls closure for 'proc'
 * @return 0 to continue extracting, 1 if we are done
 */
static int
add_keyword (enum EXTRACTOR_MetaType type,
	     char *keyword, 
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  int ret;
  char *value;
  
  if (NULL == keyword)
    return 0;
  if ( (keyword[0] == '\"') && 
       (keyword[strlen (keyword) - 1] == '\"') )
    {
      keyword[strlen (keyword) - 1] = '\0';
      value = &keyword[1];
    }
  else
    value = keyword;
  if (0 == strlen (value))
    {
      free (keyword);
      return 0;
    }
  ret = proc (proc_cls, 
	      "man",
	      type,
	      EXTRACTOR_METAFORMAT_UTF8,
	      "text/plain",
	      value,
	      strlen (value)+1);
  free (keyword);
  return ret;
}


/**
 * Find the end of the current token (which may be quoted).
 *
 * @param end beginning of the current token, updated to its end; set to size + 1 if the token does not end properly
 * @param buf input buffer with the characters
 * @param size number of bytes in buf
 */
static void
find_end_of_token (size_t *end,
		   const char *buf, 
		   const size_t size)
{
  int quot;

  quot = 0;
  while ( (*end < size) &&
	  ( (0 != (quot & 1)) ||
	    ((' ' != buf[*end])) ) )
    {
      if ('\"' == buf[*end])
        quot++;
      (*end)++;
    }
  if (1 == (quot & 1))
    (*end) = size + 1;
}


/**
 * How many bytes do we actually try to scan? (from the beginning
 * of the file).
 */
#define MAX_READ (16 * 1024)


/**
 * Add a keyword to LE.
 * 
 * @param t type to use
 * @param s keyword to give to LE
 */
#define ADD(t,s) do { if (0 != add_keyword (t, s, ec->proc, ec->cls)) return; } while (0)


/**
 * Main entry method for the man page extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_man_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  const size_t xlen = strlen (".TH ");
  size_t pos;
  size_t xsize;
  size_t end;
  void *data;
  ssize_t size;
  char *buf;
  
  if (0 >= (size = ec->read (ec->cls, &data, MAX_READ)))
    return;
  buf = data;
  pos = 0;
  if (size < xlen)
    return;
  /* find actual beginning of the man page (.TH);
     abort if we find non-printable characters */
  while ( (pos < size - xlen) &&
	  ( (0 != strncmp (".TH ",
			   &buf[pos],
			   xlen)) || 
	    ( (0 != pos) && 
	      (buf[pos - 1] != '\n') ) ) )
    {
      if ( (! isgraph ((unsigned char) buf[pos])) && 
	   (! isspace ((unsigned char) buf[pos])) )
        return;
      pos++;
    }
  if (0 != strncmp (".TH ", &buf[pos], xlen))
    return;

  /* find end of ".TH"-line */
  xsize = pos;
  while ( (xsize < size) && ('\n' != buf[xsize]) )
    xsize++;
  /* limit processing to ".TH" line */
  size = xsize;

  /* skip over ".TH" */
  pos += xlen;

  /* first token is the title */
  end = pos;
  find_end_of_token (&end, buf, size);
  if (end > size)
    return;
  if (end > pos)
    {
      ADD (EXTRACTOR_METATYPE_TITLE, stndup (&buf[pos], end - pos));
      pos = end + 1;
    }
  if (pos >= size)
    return;
  
  /* next token is the section */
  end = pos;
  find_end_of_token (&end, buf, size);
  if (end > size)
    return;
  if ('\"' == buf[pos])
    pos++;
  if ((end - pos >= 1) && (end - pos <= 4))
    {
      switch (buf[pos])
	{
	case '1':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("Commands")));
	  break;
	case '2':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("System calls")));
	  break;
	case '3':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("Library calls")));
	  break;
	case '4':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("Special files")));
	  break;
	case '5':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("File formats and conventions")));
	  break;
	case '6':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("Games")));
	  break;
	case '7':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("Conventions and miscellaneous")));
	  break;
	case '8':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("System management commands")));
	  break;
	case '9':
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       strdup (_("Kernel routines")));
	  break;
	default:
	  ADD (EXTRACTOR_METATYPE_SECTION,
	       stndup (&buf[pos], 1));
	}
      pos = end + 1;
    }
  end = pos;

  /* next token is the modification date */
  find_end_of_token (&end, buf, size);
  if (end > size)
    return;  
  if (end > pos)
    {
      ADD (EXTRACTOR_METATYPE_MODIFICATION_DATE, stndup (&buf[pos], end - pos));
      pos = end + 1;
    }

  /* next token is the source of the man page */
  end = pos;
  find_end_of_token (&end, buf, size);
  if (end > size)
    return;
  if (end > pos)
    {
      ADD (EXTRACTOR_METATYPE_SOURCE,
	   stndup (&buf[pos], end - pos));
      pos = end + 1;
    }

  /* last token is the title of the book the man page belongs to */
  end = pos;
  find_end_of_token (&end, buf, size);
  if (end > size)
    return;
  if (end > pos)
    {
      ADD (EXTRACTOR_METATYPE_BOOK_TITLE,
	   stndup (&buf[pos], end - pos));
      pos = end + 1;
    }
}

/* end of man_extractor.c */
