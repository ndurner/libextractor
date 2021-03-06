/*
     This file is part of libextractor.
     Copyright (C) 2004 Vidyut Samanta and Christian Grothoff

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

#include "platform.h"
#include "extractor.h"
#include "convert.h"

/**
 * Convert the len characters long character sequence
 * given in input that is in the given charset
 * to UTF-8.
 *
 * @param input string to convert
 * @param len number of bytes in input
 * @param charset input character set
 * @return the converted string (0-terminated), NULL on error
 * @return the converted string (0-terminated),
 *  if conversion fails, a copy of the orignal
 *  string is returned.
 */
char *
EXTRACTOR_common_convert_to_utf8 (const char *input, 
				  size_t len, 
				  const char *charset)
{
#if HAVE_ICONV
  size_t tmpSize;
  size_t finSize;
  char *tmp;
  char *ret;
  char *itmp;
  const char *i;
  iconv_t cd;

  i = input;
  cd = iconv_open ("UTF-8", charset);
  if (cd == (iconv_t) - 1)
    return strdup (i);
  if (len > 1024 * 1024)
    {
      iconv_close (cd);
      return NULL; /* too big for meta data */
    }
  tmpSize = 3 * len + 4;
  tmp = malloc (tmpSize);
  if (tmp == NULL)
    {
      iconv_close (cd);
      return NULL;
    }
  itmp = tmp;
  finSize = tmpSize;
  if (iconv (cd, (char **) &input, &len, &itmp, &finSize) == SIZE_MAX)
    {
      iconv_close (cd);
      free (tmp);
      return strdup (i);
    }
  ret = malloc (tmpSize - finSize + 1);
  if (ret == NULL)
    {
      iconv_close (cd);
      free (tmp);
      return NULL;
    }
  memcpy (ret, tmp, tmpSize - finSize);
  ret[tmpSize - finSize] = '\0';
  free (tmp);
  iconv_close (cd);
  return ret;
#else
  char *ret;

  ret = malloc (len + 1);
  memcpy (ret, input, len);
  ret[len] = '\0';
  return ret;
#endif
}

/* end of convert.c */
