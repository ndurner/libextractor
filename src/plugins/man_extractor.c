/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2009 Vidyut Samanta and Christian Grothoff

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
#include <ctype.h>

static char *
stndup (const char *str, size_t n)
{
  char *tmp;
  tmp = malloc (n + 1);
  if (tmp == NULL)
    return NULL;
  tmp[n] = '\0';
  memcpy (tmp, str, n);
  return tmp;
}

static int
addKeyword (enum EXTRACTOR_MetaType type,
            char *keyword, 
	    EXTRACTOR_MetaDataProcessor proc,
	    void *proc_cls)
{
  int ret;
  if (keyword == NULL)
    return 0;
  if (strlen (keyword) == 0)
    {
      free (keyword);
      return 0;
    }
  if ((keyword[0] == '\"') && (keyword[strlen (keyword) - 1] == '\"'))
    {
      char *tmp;

      keyword[strlen (keyword) - 1] = '\0';
      tmp = strdup (&keyword[1]);
      free (keyword);
      if (tmp == NULL)
	return 0;
      keyword = tmp;
    }
  if (strlen (keyword) == 0)
    {
      free (keyword);
      return 0;
    }
  ret = proc (proc_cls, 
	      "man",
	      type,
	      EXTRACTOR_METAFORMAT_UTF8,
	      "text/plain",
	      keyword,
	      strlen (keyword)+1);
  free (keyword);
  return ret;
}

static void
NEXT (size_t * end, const char *buf, const size_t size)
{
  int quot;

  quot = 0;
  while ((*end < size) && (((quot & 1) != 0) || ((buf[*end] != ' '))))
    {
      if (buf[*end] == '\"')
        quot++;
      (*end)++;
    }
  if ((quot & 1) == 1)
    (*end) = size + 1;
}

/**
 * How many bytes do we actually try to scan? (from the beginning
 * of the file).
 */
#define MAX_READ (16 * 1024)

#define ADD(t,s) do { if (0 != addKeyword (t, s, proc, proc_cls)) return 1; } while (0)

int 
EXTRACTOR_man_extract (const char *buf,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  int pos;
  size_t xsize;
  const size_t xlen = strlen (".TH ");

  if (size > MAX_READ)
    size = MAX_READ;
  pos = 0;
  if (size < xlen)
    return 0;
  while ((pos < size - xlen) &&
         ((0 != strncmp (".TH ",
                         &buf[pos],
                         xlen)) || ((pos != 0) && (buf[pos - 1] != '\n'))))
    {
      if (!isgraph ((unsigned char) buf[pos]) && 
	  !isspace ((unsigned char) buf[pos]))
        return 0;
      pos++;
    }
  xsize = pos;
  while ((xsize < size) && (buf[xsize] != '\n'))
    xsize++;
  size = xsize;

  if (0 == strncmp (".TH ", &buf[pos], xlen))
    {
      size_t end;

      pos += xlen;
      end = pos;
      NEXT (&end, buf, size);
      if (end > size)
        return 0;
      if (end - pos > 0)
        {
          ADD (EXTRACTOR_METATYPE_TITLE, stndup (&buf[pos], end - pos));
          pos = end + 1;
        }
      if (pos >= size)
        return 0;
      end = pos;
      NEXT (&end, buf, size);
      if (end > size)
        return 0;
      if (buf[pos] == '\"')
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
            }
          pos = end + 1;
        }
      end = pos;
      NEXT (&end, buf, size);
      if (end > size)
        return 0;
      if (end - pos > 0)
        {
          ADD (EXTRACTOR_METATYPE_MODIFICATION_DATE, stndup (&buf[pos], end - pos));
          pos = end + 1;
        }
      end = pos;
      NEXT (&end, buf, size);
      if (end > size)
        return 0;
      if (end - pos > 0)
        {
          ADD (EXTRACTOR_METATYPE_SOURCE,
	       stndup (&buf[pos], end - pos));
          pos = end + 1;
        }
      end = pos;
      NEXT (&end, buf, size);
      if (end > size)
        return 0;
      if (end - pos > 0)
        {
          ADD (EXTRACTOR_METATYPE_BOOK_TITLE,
	       stndup (&buf[pos], end - pos));
          pos = end + 1;
        }
    }

  return 0;
}

/* end of man_extractor.c */
