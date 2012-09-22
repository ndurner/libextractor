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

/**
 * @file main/iconv.c
 * @brief convenience functions for character conversion
 * @author Christian Grothoff
 */

/**
 * Convert the given input using the given converter
 * and return as a 0-terminated string.
 *
 * @param cd converter to use
 * @param in input string
 * @param inSize number of bytes in 'in'
 * @return NULL on error, otherwise the converted string (to be free'd by caller)
 */
static char * 
iconv_helper (iconv_t cd,
	      const char *in,
	      size_t inSize) 
{
#if HAVE_ICONV
  char *buf;
  char *ibuf;
  const char *i;
  size_t outSize;
  size_t outLeft;

  if (inSize > 1024 * 1024)
    return NULL; /* too big to be meta data */
  i = in;
  /* reset iconv */
  iconv (cd, NULL, NULL, NULL, NULL);
  outSize = 4 * inSize + 2;
  outLeft = outSize - 2; /* make sure we have 2 0-terminations! */
  if (NULL == (buf = malloc (outSize)))
    return NULL;
  ibuf = buf;
  memset (buf, 0, outSize);
  if (iconv (cd,
	     (char**) &in,
	     &inSize,
	     &ibuf,
	     &outLeft) == SIZE_MAX)
    {
      /* conversion failed */
      free (buf);
      return strdup (i);
    }
  return buf;
#else
  /* good luck, just copying string... */
  char *buf;
  
  buf = malloc (inSize + 1);
  memcpy (buf, in, inSize);
  buf[inSize] = '\0';
#endif
}

/* end of iconv.c */
