/*
Catlib Copyright Notice

The author of this software is Christopher Adam Telfer
Copyright (c) 1998, 1999, 2000, 2001, 2002
by Christopher Adam Telfer.  All Rights Reserved.

Permission to use, copy, modify, and distribute this software for any
purpose without fee is hereby granted, provided that the above copyright
notice, this paragraph, and the following two paragraphs appear in all
copies, modifications, and distributions.

IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE.  THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS".   THE AUTHOR HAS NO
OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

*/

#include "platform.h"
#include "pack.h"

typedef unsigned char byte;
typedef unsigned short half;
typedef unsigned int word;
typedef signed char sbyte;
typedef signed short shalf;
typedef signed int sword;



int
EXTRACTOR_common_cat_unpack (const void *buf, const char *fmt, ...)
{
  va_list ap;
  word maxlen, len, *wordp;
  void *arr;
  byte *bp, *bytep, *newbuf;
  half *halfp;
  long long *ll;
  sbyte *sbytep;
  shalf *shalfp;
  sword *swordp;
  int npacked;
  unsigned int nreps, i, isnonprefixed = 1;     /* used for 'a' types only */
  struct cat_bvec *cbvp;
  char *cp;

  bp = (byte *) buf;
  npacked = 0;

  va_start (ap, fmt);

  while (*fmt)
    {
      nreps = 1;

      if (isdigit ( (unsigned char) *fmt))
        {
          /* We use cp instead of format to keep the 'const' qualifier of fmt */
          nreps = strtoul (fmt, &cp, 0);
          fmt = cp;
          if (*fmt == 'a')
            isnonprefixed = 0;
        }

      switch (*fmt)
        {
        case 'B':
        case 'b':
          bytep = va_arg (ap, byte *);
          for (i = 0; i < nreps; ++i)
            {
              *bytep = *bp++;
              ++bytep;
              npacked += 1;
            }
          break;



        case 'h':
          halfp = va_arg (ap, half *);
          for (i = 0; i < nreps; ++i)
            {
              *halfp = *bp++;
              *halfp |= *bp++ << 8;
              ++halfp;
              npacked += 2;
            }
          break;

        case 'H':
          halfp = va_arg (ap, half *);
          for (i = 0; i < nreps; ++i)
            {
              *halfp = *bp++ << 8;
              *halfp |= *bp++;
              ++halfp;
              npacked += 2;
            }
          break;


        case 'w':
          wordp = va_arg (ap, word *);
          for (i = 0; i < nreps; ++i)
            {
              *wordp = *bp++;
              *wordp |= *bp++ << 8;
              *wordp |= *bp++ << 16;
              *wordp |= *bp++ << 24;
              ++wordp;
              npacked += 4;
            }
          break;

        case 'x':
          ll = va_arg (ap, long long *);
          for (i = 0; i < nreps; ++i)
            {
              *ll = ((long long) *bp++);
              *ll |= ((long long) *bp++) << 8;
              *ll |= ((long long) *bp++) << 16;
              *ll |= ((long long) *bp++) << 24;
              *ll |= ((long long) *bp++) << 32;
              *ll |= ((long long) *bp++) << 40;
              *ll |= ((long long) *bp++) << 48;
              *ll |= ((long long) *bp++) << 56;
              ++ll;
              npacked += 8;
            }
          break;

        case 'W':
          wordp = va_arg (ap, word *);
          for (i = 0; i < nreps; ++i)
            {
              *wordp = *bp++ << 24;
              *wordp |= *bp++ << 16;
              *wordp |= *bp++ << 8;
              *wordp |= *bp++;
              ++wordp;
              npacked += 4;
            }
          break;

        case 'X':
          ll = va_arg (ap, long long *);
          for (i = 0; i < nreps; ++i)
            {
              *ll = ((long long) *bp++) << 56;
              *ll |= ((long long) *bp++) << 48;
              *ll |= ((long long) *bp++) << 40;
              *ll |= ((long long) *bp++) << 32;
              *ll |= ((long long) *bp++) << 24;
              *ll |= ((long long) *bp++) << 18;
              *ll |= ((long long) *bp++) << 8;
              *ll |= ((long long) *bp++);
              ++ll;
              npacked += 8;
            }
          break;


        case 'A':
          if (isnonprefixed)
            {
              maxlen = va_arg (ap, word);
              arr = va_arg (ap, void *);

              len = *bp++ << 24;
              len |= *bp++ << 16;
              len |= *bp++ << 8;
              len |= *bp++;

              if (len > maxlen)
		{
		  va_end (ap);
		  return -1;
		}

              memmove (arr, bp, len);
              bp += len;

              npacked += len;
            }
          else
            {
              cbvp = va_arg (ap, struct cat_bvec *);
              for (i = 0; i < nreps; ++i)
                {
                  maxlen = cbvp->len;
                  arr = cbvp->data;

                  len = *bp++ << 24;
                  len |= *bp++ << 16;
                  len |= *bp++ << 8;
                  len |= *bp++;

                  if (len > maxlen)
                    return -1;

                  memmove (arr, bp, len);
                  cbvp->len = len;
                  bp += len;

                  ++cbvp;
                  npacked += len;
                }
              isnonprefixed = 1;
            }
          break;

        case 'C':
        case 'c':
          sbytep = va_arg (ap, sbyte *);
          for (i = 0; i < nreps; ++i)
            {
              *sbytep = *bp++;

              if ((sizeof (sbyte) > 1) && (*sbytep & 0x80))
                *sbytep |= (~0) << ((sizeof (sbyte) - 1) * 8);

              ++sbytep;
              npacked += 1;
            }
          break;


        case 's':
          shalfp = va_arg (ap, shalf *);
          for (i = 0; i < nreps; ++i)
            {
              *shalfp = *bp++;
              *shalfp |= *bp++ << 8;

              if ((sizeof (shalf) > 2) && (*shalfp & 0x8000))
                *shalfp |= (~0) << ((sizeof (shalf) - 2) * 8);

              ++shalfp;
              npacked += 2;
            }
          break;

        case 'S':
          shalfp = va_arg (ap, shalf *);
          for (i = 0; i < nreps; ++i)
            {
              *shalfp = *bp++ << 8;
              *shalfp |= *bp++;

              if ((sizeof (shalf) > 2) && (*shalfp & 0x8000))
                *shalfp |= (~0) << ((sizeof (shalf) - 2) * 8);

              ++shalfp;
              npacked += 2;
            }
          break;

        case 'l':
          swordp = va_arg (ap, sword *);
          for (i = 0; i < nreps; ++i)
            {
              *swordp = *bp++;
              *swordp |= *bp++ << 8;
              *swordp |= *bp++ << 16;
              *swordp |= *bp++ << 24;

              if ((sizeof (swordp) > 4) && (*swordp & 0x80000000))
                *swordp |= (~0) << ((sizeof (sword) - 4) * 8);

              ++swordp;
              npacked += 4;
            }
          break;

        case 'L':
          swordp = va_arg (ap, sword *);
          for (i = 0; i < nreps; ++i)
            {
              *swordp = *bp++ << 24;
              *swordp |= *bp++ << 16;
              *swordp |= *bp++ << 8;
              *swordp |= *bp++;

              if ((sizeof (swordp) > 4) && (*swordp & 0x80000000))
                *swordp |= (~0) << ((sizeof (sword) - 4) * 8);

              ++swordp;
              npacked += 4;
            }
          break;

        case 'P':
          cbvp = va_arg (ap, struct cat_bvec *);
          for (i = 0; i < nreps; ++i)
            {
              len = *bp++ << 24;
              len |= *bp++ << 16;
              len |= *bp++ << 8;
              len |= *bp++;

              newbuf = (byte *) malloc (len);

              if (!newbuf)
                {
                  int j;
                  for (j = 0; j < i; j++)
                    free (cbvp[i].data);
		  va_end (ap);
                  return -1;
                }

              memmove (newbuf, bp, len);
              cbvp[i].data = newbuf;
              cbvp[i].len = len;

              bp += len;
              npacked += len;
            }
          break;

        default:
          va_end (ap);
          return -1;
        }

      ++fmt;
    }

  va_end (ap);
  return 0;
}
