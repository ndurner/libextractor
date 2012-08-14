/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2009 Vidyut Samanta and Christian Grothoff

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

/*
 * Note that this code is not complete!
 *
 * References:
 *
 * http://www.mkssoftware.com/docs/man4/tar.4.asp
 * (does document USTAR format common nowadays,
 *  but not other extended formats such as the one produced
 *  by GNU tar 1.13 when very long filenames are met.)
 *
 * http://gd.tuwien.ac.at/utils/archivers/star/README.otherbugs
 * (J. Schilling's remarks on TAR formats compatibility issues.)
 */

/*
 * Define known TAR archive member variants.
 * In theory different variants
 * can coexist within a single TAR archive file
 * although this will be uncommon.
 */
#define TAR_V7ORIGINAL_FORMAT    (1)
#define TAR_V7EXTENDED_FORMAT    (1 << 1)
#define TAR_SCHILLING1985_FORMAT (1 << 2)
#define TAR_POSIX1988_FORMAT     (1 << 3)
#define TAR_GNU1991_FORMAT       (1 << 4)
#define TAR_SCHILLING1994_FORMAT (1 << 5)
#define TAR_GNU1997_FORMAT       (1 << 6)
#define TAR_POSIX2001_FORMAT     (1 << 7)
#define TAR_SCHILLING2001_FORMAT (1 << 8)
#define TAR_SOLARIS2001_FORMAT   (1 << 9)
#define TAR_GNU2004_FORMAT       (1 << 10)

/*
 * TAR header structure, modelled after POSIX.1-1988
 */
typedef struct
{
  char fileName[100];
  char mode[8];
  char userId[8];
  char groupId[8];
  char fileSize[12];
  char lastModTime[12];
  char chksum[8];
  char link;
  char linkName[100];
  /*
   * All fields below are a
   * either zero-filled or undefined
   * for UNIX V7 TAR archive members ;
   * their header is always 512 octets long nevertheless.
   */
  char ustarMagic[6];
  char version[2];
  char userName[32];
  char groupName[32];
  char devMajor[8];
  char devMinor[8];
  char prefix[155];
  char filler[12];
} TarHeader;

#define TAR_HEADER_SIZE (sizeof(TarHeader))
#define TAR_TIME_FENCE  ((long long) (-(1LL << 62)))

static size_t
tar_roundup (size_t size)
{
  size_t diff = (size % TAR_HEADER_SIZE);

  return (0 == diff) ? size : (size + (TAR_HEADER_SIZE - diff));
}

static int
tar_isnonzero (const char *data, unsigned int length)
{
  unsigned int total = 0;

  while (total < length)
    {
      if (0 != data[total])
        return 1;
      total++;
    }

  return 0;
}

static unsigned int
tar_octalvalue (const char *data, size_t size, unsigned long long *valueptr)
{
  unsigned int result = 0;

  if (NULL != data && 0 < size)
    {
      const char *p = data;
      int found = 0;
      unsigned long long value = 0;

      while ((p < data + size) && (' ' == *p))
        p += 1;

      while ((p < data + size) && ('0' <= *p) && (*p < '8'))
        {
          found = 1;
          value *= 8;
          value += (*p - '0');
          p += 1;
        }

      if (0 != found)
        {
          while ((p < data + size) && ((0 == *p) || (' ' == *p)))
            p += 1;

          result = (p - data);
        }

      if ((0 < result) && (NULL != valueptr))
        *valueptr = value;
    }

  return result;
}

#ifndef EOVERFLOW
#define EOVERFLOW -1
#endif

static int
tar_time (long long timeval, char *rtime, unsigned int rsize)
{
  int retval = 0;

  /*
   * shift epoch to proleptic times
   * to make subsequent modulo operations safer.
   */
  long long my_timeval = timeval
    + ((long long) ((1970 * 365) + 478) * (long long) 86400);

  unsigned int seconds = (unsigned int) (my_timeval % 60);
  unsigned int minutes = (unsigned int) ((my_timeval / 60) % 60);
  unsigned int hours = (unsigned int) ((my_timeval / 3600) % 24);

  unsigned int year = 0;
  unsigned int month = 1;

  unsigned int days = (unsigned int) (my_timeval / (24 * 3600));

  unsigned int days_in_month[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  unsigned int diff = 0;

  if ((long long) 0 > my_timeval)
    return EDOM;

  /*
   * 400-year periods
   */
  year += (400 * (days / ((365 * 400) + 97)));
  days %= ((365 * 400) + 97);

  /*
   * 100-year periods
   */
  diff = (days / ((365 * 100) + 24));
  if (4 <= diff)
    {
      year += 399;
      days = 364;
    }
  else
    {
      year += (100 * diff);
      days %= ((365 * 100) + 24);
    }

  /*
   * remaining leap years
   */
  year += (4 * (days / ((365 * 4) + 1)));
  days %= ((365 * 4) + 1);

  while (1)
    {
      if ((0 == (year % 400)) || ((0 == (year % 4)) && (0 != (year % 100))))
        {
          if (366 > days)
            {
              break;
            }
          else
            {
              days -= 366;
              year++;
            }
        }
      else
        {
          if (365 > days)
            {
              break;
            }
          else
            {
              days -= 365;
              year++;
            }
        }
    }

  if ((0 == (year % 400)) || ((0 == (year % 4)) && (0 != (year % 100))))
    days_in_month[1] = 29;

  for (month = 0; (month < 12) && (days >= days_in_month[month]); month += 1)
    days -= days_in_month[month];

  retval = snprintf (rtime, rsize, "%04u-%02u-%02uT%02u:%02u:%02uZ",
                     year, month + 1, days + 1, hours, minutes, seconds);

  return (retval < rsize) ? 0 : EOVERFLOW;
}

#define ADD(t,s) do { if (0 != (ret = proc (proc_cls, "tar", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto FINISH; } while (0)
#define ADDF(t,s) do { if (0 != (ret = proc (proc_cls, "tar", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) { free(s); goto FINISH; } free (s); } while (0)

int
EXTRACTOR_tar_extract (const char *data,
                       size_t size,
                       EXTRACTOR_MetaDataProcessor proc,
                       void *proc_cls, const char *options)
{
  char *fname = NULL;
  size_t pos;
  int contents_are_empty = 1;
  long long maxftime = TAR_TIME_FENCE;
  unsigned int format_archive = 0;
  int ret;

  if (512 != TAR_HEADER_SIZE)
    return 0;                   /* compiler should remove this when optimising */
  if (0 != (size % TAR_HEADER_SIZE))
    return 0;                   /* cannot be tar! */
  if (size < TAR_HEADER_SIZE)
    return 0;                   /* too short, or somehow truncated */

  ret = 0;
  pos = 0;
  while ((pos + TAR_HEADER_SIZE) <= size)
    {
      const TarHeader *tar = NULL;
      unsigned format_member = 0;
      unsigned long long fmode;
      unsigned long long fsize;
      long long ftime = TAR_TIME_FENCE;
      char typeFlag = -1;
      const char *nul_pos;
      unsigned int tar_prefix_length = 0;
      unsigned int tar_name_length = 0;
      unsigned int checksum_offset;
      int checksum_computed_500s = 0;
      int checksum_computed_512s = 0;
      unsigned int checksum_computed_500u = 0;
      unsigned int checksum_computed_512u = 0;
      unsigned long long checksum_stored = 0;

      /*
       * Compute TAR header checksum and compare with stored value.
       * Allow for non-conformant checksums computed with signed values,
       * such as those produced by early Solaris tar.
       * Allow for non-conformant checksums computed on first 500 octets,
       * such as those produced by SunOS 4.x tar according to J. Schilling.
       * This will also detect EOF marks, since a zero-filled block
       * cannot possibly hold octal values.
       */
      for (checksum_offset = 0; checksum_offset < 148; checksum_offset += 1)
        {
          checksum_computed_500u +=
            (unsigned char) data[pos + checksum_offset];
          checksum_computed_500s += (signed char) data[pos + checksum_offset];
        }
      if (8 >
          tar_octalvalue (data + pos + checksum_offset, 8, &checksum_stored))
        break;
      for (; checksum_offset < 156; checksum_offset += 1)
        {
          checksum_computed_500u += (unsigned char) ' ';
          checksum_computed_500s += (signed char) ' ';
        }
      for (; checksum_offset < 500; checksum_offset += 1)
        {
          checksum_computed_500u +=
            (unsigned char) data[pos + checksum_offset];
          checksum_computed_500s += (signed char) data[pos + checksum_offset];
        }

      checksum_computed_512u = checksum_computed_500u;
      checksum_computed_512s = checksum_computed_500s;
      for (; checksum_offset < TAR_HEADER_SIZE; checksum_offset += 1)
        {
          checksum_computed_512u +=
            (unsigned char) data[pos + checksum_offset];
          checksum_computed_512s += (signed char) data[pos + checksum_offset];
        }

      /*
       * Suggestion: use signed checksum matches to refine
       * TAR format detection.
       */
      if ((checksum_stored != (unsigned long long) checksum_computed_512u)
          && (checksum_stored != (unsigned long long) checksum_computed_512s)
          && (checksum_stored != (unsigned long long) checksum_computed_500s)
          && (checksum_stored != (unsigned long long) checksum_computed_500u))
        break;

      tar = (const TarHeader *) &data[pos];
      typeFlag = tar->link;
      pos += TAR_HEADER_SIZE;

      /*
       * Checking all octal fields helps reduce
       * the possibility of false positives ;
       * only the file size, time and mode are used for now.
       *
       * This will fail over GNU and Schilling TAR huge size fields
       * using non-octal encodings used for very large file lengths (> 8 GB).
       */
      if ((12 > tar_octalvalue (tar->fileSize, 12,
                                &fsize))
          || (12 > tar_octalvalue (tar->lastModTime, 12,
                                   (unsigned long long *) &ftime))
          || (8 > tar_octalvalue (tar->mode, 8,
                                  (unsigned long long *) &fmode))
          || (8 > tar_octalvalue (tar->userId, 8, NULL))
          || (8 > tar_octalvalue (tar->groupId, 8, NULL)))
        break;

      /*
       * Find out which TAR variant is here.
       */
      if (0 == memcmp (tar->ustarMagic, "ustar  ", 7))
        {

          if (' ' == tar->mode[6])
            format_member = TAR_GNU1991_FORMAT;
          else if (('K' == typeFlag) || ('L' == typeFlag))
            {
              format_member = TAR_GNU1997_FORMAT;
              ftime = TAR_TIME_FENCE;
            }
          else
            format_member =
              (((unsigned) fmode) !=
               (((unsigned) fmode) & 03777)) ? TAR_GNU1997_FORMAT :
              TAR_GNU2004_FORMAT;

        }
      else if (0 == memcmp (tar->ustarMagic, "ustar", 6))
        {

          /*
           * It is important to perform test for SCHILLING1994 before GNU1997
           * because certain extension type flags ('L' and 'S' for instance)
           * are used by both.
           */
          if ((0 == tar->prefix[130])
              && (12 <= tar_octalvalue (tar->prefix + 131, 12, NULL))
              && (12 <= tar_octalvalue (tar->prefix + 143, 12, NULL))
              && (0 == tar_isnonzero (tar->filler, 8))
              && (0 == memcmp (tar->filler + 8, "tar", 4)))
            {

              format_member = TAR_SCHILLING1994_FORMAT;

            }
          else if (('D' == typeFlag) || ('K' == typeFlag)
                   || ('L' == typeFlag) || ('M' == typeFlag)
                   || ('N' == typeFlag) || ('S' == typeFlag)
                   || ('V' == typeFlag))
            {

              format_member = TAR_GNU1997_FORMAT;

            }
          else if (('g' == typeFlag)
                   || ('x' == typeFlag) || ('X' == typeFlag))
            {

              format_member = TAR_POSIX2001_FORMAT;
              ftime = TAR_TIME_FENCE;

            }
          else
            {

              format_member = TAR_POSIX1988_FORMAT;

            }
        }
      else if ((0 == memcmp (tar->filler + 8, "tar", 4))
               && (0 == tar_isnonzero (tar->filler, 8)))
        {

          format_member = TAR_SCHILLING1985_FORMAT;

        }
      else if (('0' <= typeFlag) && (typeFlag <= '2'))
        {

          format_member = TAR_V7ORIGINAL_FORMAT;

        }
      else
        {

          format_member = TAR_V7EXTENDED_FORMAT;

        }

      /*
       * Locate the file names.
       */
      if ((0 != (format_member & TAR_POSIX2001_FORMAT))
	  && (('x' == typeFlag) || ('X' == typeFlag)))
	{
	  
	  if (size <= pos)
	    break;
	  
	  else if ((8 <= fsize) && fsize <= (unsigned long long) (size - pos))
	    {
	      const char *keyptr = data + pos;
	      const char *valptr = NULL;
	      const char *nameptr = NULL;
	      unsigned int keylength = 0;
	      unsigned int namelength = 0;
	      
	      while (keyptr < data + pos + (size_t) fsize)
		{
		  if (('0' > *keyptr) || ('9' < *keyptr))
		    {
		      keyptr += 1;
		      continue;
		    }
		  
		  keylength =
		    (unsigned int) strtoul (keyptr, (char **) &valptr, 10);
		  if ((0 < keylength) && (NULL != valptr)
		      && (keyptr != valptr))
		    {
		      while ((valptr < data + pos + (size_t) fsize)
			     && (' ' == *valptr))
			valptr += 1;
		      if (0 == memcmp (valptr, "path=", 5))
			{
			  nameptr = valptr + 5;
			  namelength = keylength - (nameptr - keyptr);
			}
		      else
			{
			  
			  if ((keylength > (valptr - keyptr) + 4 + 2)
			      && (0 == memcmp (valptr, "GNU.", 4)))
			    format_archive |= TAR_GNU2004_FORMAT;
			  
			  else if ((keylength > (valptr - keyptr) + 7 + 2)
				   && (0 == memcmp (valptr, "SCHILY.", 7)))
			    format_archive |= TAR_SCHILLING2001_FORMAT;
			  
			  else if ((keylength > (valptr - keyptr) + 4 + 2)
				   && (0 == memcmp (valptr, "SUN.", 4)))
			    format_archive |= TAR_SOLARIS2001_FORMAT;
			}
		      
		      keyptr += keylength;
		    }
		  else
		    {
		      nameptr = NULL;
		      break;
		    }
                }
	      
              if ((NULL != nameptr) && (0 != *nameptr)
                  && ((size - (nameptr - data)) >= namelength)
                  && (1 < namelength) )
                {
                  /*
                   * There is an 1-offset because POSIX.1-2001
                   * field separator is counted in field length.
                   */
		  if (fname != NULL)
		    free (fname);
                  fname = malloc (namelength);
                  if (NULL != fname)
                    {
                      memcpy (fname, nameptr, namelength - 1);
                      fname[namelength - 1] = '\0';

                      pos += tar_roundup ((size_t) fsize);
                      format_archive |= format_member;
                      continue;
                    }
                }
            }
        }

      else if ((0 != (format_member
                      & (TAR_SCHILLING1994_FORMAT
                         | TAR_GNU1997_FORMAT | TAR_GNU2004_FORMAT)))
               && ('L' == typeFlag))
        {

          if (size <= pos)
            break;

          else if ((0 < fsize) && fsize <= (unsigned long long) (size - pos))
            {

              size_t length = (size_t) fsize;

              nul_pos = memchr (data + pos, 0, length);
              if (NULL != nul_pos)
                length = (nul_pos - (data + pos));

              if (0 < length)
                {
		  if (fname != NULL)
		    free (fname);
                  fname = malloc (1 + length);
                  if (NULL != fname)
                    {
                      memcpy (fname, data + pos, length);
                      fname[length] = '\0';
                    }

                  pos += tar_roundup ((size_t) fsize);
                  format_archive |= format_member;
                  continue;
                }
            }
        }
      else
        {

          nul_pos = memchr (tar->fileName, 0, sizeof tar->fileName);
          tar_name_length = (0 == nul_pos)
            ? sizeof (tar->fileName) : (nul_pos - tar->fileName);

          if ((0 !=
               (format_member & (TAR_GNU1997_FORMAT | TAR_GNU2004_FORMAT)))
              && ('S' == typeFlag))
            {

              if ((0 == tar->prefix[40])
                  && (0 != tar->prefix[137])
                  && (12 <= tar_octalvalue (tar->prefix + 41, 12, NULL))
                  && (12 <= tar_octalvalue (tar->prefix + 53, 12, NULL)))
                {
                  /*
                   * fsize needs adjustment when there are more than 4 sparse blocks
                   */
                  size_t diffpos = 0;
                  fsize += TAR_HEADER_SIZE;

                  while ((pos + diffpos + TAR_HEADER_SIZE < size)
                         && (0 != *(data + pos + diffpos + 504)))
                    {
                      diffpos += TAR_HEADER_SIZE;
                      fsize += TAR_HEADER_SIZE;
                    }
                }

              typeFlag = '0';

            }
          else if (0 != (format_member & TAR_SCHILLING1994_FORMAT))
            {

              nul_pos = memchr (tar->prefix, 0, 130);
              tar_prefix_length = (0 == nul_pos)
                ? 130 : (nul_pos - tar->prefix);

              if ('S' == typeFlag)
                typeFlag = '0';

            }
          else if (0 != (format_member & TAR_SCHILLING1985_FORMAT))
            {

              nul_pos = memchr (tar->prefix, 0, 155);
              tar_prefix_length = (0 == nul_pos)
                ? 155 : (nul_pos - tar->prefix);


              if ('S' == typeFlag)
                typeFlag = '0';

            }
          else if (0 != (format_member & TAR_POSIX1988_FORMAT))
            {

              nul_pos = memchr (tar->prefix, 0, sizeof tar->prefix);
              tar_prefix_length = (0 == nul_pos)
                ? sizeof tar->prefix : nul_pos - tar->prefix;

            }
        }

      /*
       * Update position so that next loop iteration will find
       * either a TAR header or TAR EOF mark or just EOF.
       *
       * Consider archive member size to be zero
       * with no data following the header in the following cases :
       * '1' : hard link, '2' : soft link,
       * '3' : character device, '4' : block device,
       * '5' : directory, '6' : named pipe.
       */
      if ('1' != typeFlag && '2' != typeFlag
          && '3' != typeFlag && '4' != typeFlag
          && '5' != typeFlag && '6' != typeFlag)
        {
          if ((fsize > (unsigned long long) size)
              || (fsize + (unsigned long long) pos >
                  (unsigned long long) size))
            break;

          pos += tar_roundup ((size_t) fsize);
        }
      if (pos - 1 > size)
        break;

      format_archive |= format_member;

      /*
       * Store the file name in libextractor list.
       *
       * For the time being, only file types listed in POSIX.1-1988 ('0'..'7')
       * are retained, leaving out labels, access control lists, etc.
       */
      if ((0 == typeFlag) || (('0' <= typeFlag) && (typeFlag <= '7')))
        {
          if (NULL == fname)
            {
              if (0 < tar_prefix_length + tar_name_length)
                {
                  fname = malloc (2 + tar_prefix_length + tar_name_length);

                  if (NULL != fname)
                    {
                      if (0 < tar_prefix_length)
                        {
                          memcpy (fname, tar->prefix, tar_prefix_length);

                          if (('/' != tar->prefix[tar_prefix_length - 1])
                              && (0 < tar_name_length)
                              && ('/' != tar->fileName[0]))
                            {
                              fname[tar_prefix_length] = '/';
                              tar_prefix_length += 1;
                            }
                        }

                      if (0 < tar_name_length)
                        memcpy (fname + tar_prefix_length, tar->fileName,
                                tar_name_length);

                      fname[tar_prefix_length + tar_name_length] = '\0';
                    }
                }
            }

          if ((NULL != fname) && (0 != *fname))
            {
#if 0
              fprintf (stdout,
                       "(%u) flag = %c, size = %u, tname = (%s), fname = (%s)\n",
                       __LINE__, typeFlag, (unsigned int) fsize,
                       (NULL == tar->fileName) ? "" : tar->fileName,
                       (NULL == fname) ? "" : fname);
#endif

              ADDF (EXTRACTOR_METATYPE_FILENAME, fname);
              fname = NULL;
              if (ftime > maxftime)
                maxftime = ftime;
              contents_are_empty = 0;
            }
        }

      if (NULL != fname)
        {
          free (fname);
          fname = NULL;
        }
    }

  if (NULL != fname)
    {
      free (fname);
      fname = NULL;
    }

  /*
   * Report mimetype; report also format(s) and most recent date
   * when at least one archive member was found.
   */
  if (0 == format_archive)
    return ret;
  if (0 == contents_are_empty)
    {

      const char *formats[5] = { NULL, NULL, NULL, NULL, NULL };
      unsigned int formats_count = 0;
      unsigned int formats_u = 0;
      unsigned int format_length = 0;
      char *format = NULL;

      if (TAR_TIME_FENCE < maxftime)
        {
          char iso8601_time[24];

          if (0 == tar_time (maxftime, iso8601_time, sizeof (iso8601_time)))
            ADD (EXTRACTOR_METATYPE_CREATION_DATE, iso8601_time);
        }

      /*
       * We only keep the most recent POSIX format.
       */
      if (0 != (format_archive & TAR_POSIX2001_FORMAT))
        formats[formats_count++] = "POSIX 2001";

      else if (0 != (format_archive & TAR_POSIX1988_FORMAT))
        formats[formats_count++] = "POSIX 1988";

      /*
       * We only keep the most recent GNU format.
       */
      if (0 != (format_archive & TAR_GNU2004_FORMAT))
        formats[formats_count++] = "GNU 2004";

      else if (0 != (format_archive & TAR_GNU1997_FORMAT))
        formats[formats_count++] = "GNU 1997";

      else if (0 != (format_archive & TAR_GNU1991_FORMAT))
        formats[formats_count++] = "GNU 1991";

      /*
       * We only keep the most recent Schilling format.
       */
      if (0 != (format_archive & TAR_SCHILLING2001_FORMAT))
        formats[formats_count++] = "Schilling 2001";

      else if (0 != (format_archive & TAR_SCHILLING1994_FORMAT))
        formats[formats_count++] = "Schilling 1994";

      else if (0 != (format_archive & TAR_SCHILLING1985_FORMAT))
        formats[formats_count++] = "Schilling 1985";

      /*
       * We only keep the most recent Solaris format.
       */
      if (0 != (format_archive & TAR_SOLARIS2001_FORMAT))
        formats[formats_count++] = "Solaris 2001";

      /*
       * We only keep the (supposedly) most recent UNIX V7 format.
       */
      if (0 != (format_archive & TAR_V7EXTENDED_FORMAT))
        formats[formats_count++] = "UNIX extended V7";

      else if (0 != (format_archive & TAR_V7ORIGINAL_FORMAT))
        formats[formats_count++] = "UNIX original V7";

      /*
       * Build the format string
       */
      for (formats_u = 0; formats_u < formats_count; formats_u += 1)
        {
          if ((NULL != formats[formats_u]) && (0 != *formats[formats_u]))
            {
              if (0 < format_length)
                format_length += 3;
              format_length += strlen (formats[formats_u]);
            }
        }

      if (0 < format_length)
        {
	  if (fname != NULL)
	    free (fname);
          format = malloc (format_length + 5);

          if (NULL != format)
            {

              format_length = 0;

              for (formats_u = 0; formats_u < formats_count; formats_u += 1)
                {
                  if ((NULL != formats[formats_u])
                      && (0 != *formats[formats_u]))
                    {
                      if (0 < format_length)
                        {
                          strcpy (format + format_length, " + ");
                          format_length += 3;
                        }
                      strcpy (format + format_length, formats[formats_u]);
                      format_length += strlen (formats[formats_u]);
                    }
                }

              if (0 < format_length)
                {
                  strcpy (format + format_length, " TAR");
                  ADDF (EXTRACTOR_METATYPE_FORMAT_VERSION, format);
                }
              else
                {
                  free (format);
                }
            }
        }
    }

  ADD (EXTRACTOR_METATYPE_MIMETYPE, "application/x-tar");
FINISH:
  return ret;
}
