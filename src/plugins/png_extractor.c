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
#include <zlib.h>
#include "convert.h"

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

/**
 * strnlen is GNU specific, let's redo it here to be
 * POSIX compliant.
 */
static size_t
stnlen (const char *str, size_t maxlen)
{
  size_t ret;
  ret = 0;
  while ((ret < maxlen) && (str[ret] != '\0'))
    ret++;
  return ret;
}


static int
getIntAt (const void *pos)
{
  char p[4];

  memcpy (p, pos, 4);           /* ensure alignment! */
  return *(int *) &p[0];
}


static struct
{
  char *name;
  enum EXTRACTOR_MetaType type;
} tagmap[] =
{
  { "Author", EXTRACTOR_METATYPE_AUTHOR_NAME},
  { "Description", EXTRACTOR_METATYPE_DESCRIPTION},
  { "Comment", EXTRACTOR_METATYPE_COMMENT},
  { "Copyright", EXTRACTOR_METATYPE_COPYRIGHT},
  { "Source", EXTRACTOR_METATYPE_SOURCE_DEVICE },
  { "Creation Time", EXTRACTOR_METATYPE_CREATION_DATE},
  { "Title", EXTRACTOR_METATYPE_TITLE},
  { "Software", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE},
  { "Disclaimer", EXTRACTOR_METATYPE_DISCLAIMER},
  { "Warning", EXTRACTOR_METATYPE_WARNING},
  { NULL, EXTRACTOR_METATYPE_RESERVED }
};


#define ADD(t,s) do { if (0 != (ret = proc (proc_cls, "png", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) goto FINISH; } while (0)
#define ADDF(t,s) do { if ( (s != NULL) && (0 != (ret = proc (proc_cls, "png", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) ) { free(s); goto FINISH; } if (s != NULL) free (s); } while (0)


static int
processtEXt (const char *data,
             unsigned int length,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  char *keyword;
  unsigned int off;
  int i;
  int ret;

  data += 4;
  off = stnlen (data, length) + 1;
  if (off >= length)
    return 0;                /* failed to find '\0' */
  keyword = EXTRACTOR_common_convert_to_utf8 (&data[off], length - off, "ISO-8859-1");
  if (keyword == NULL)
    return 0;
  i = 0;
  ret = 0;
  while (tagmap[i].name != NULL)
    {
      if (0 == strcmp (tagmap[i].name, data))
	{
	  ADDF (tagmap[i].type, keyword);
	  return 0;
	}

      i++;
    }
  ADDF (EXTRACTOR_METATYPE_KEYWORDS, keyword);
 FINISH:
  return ret;
}

static int
processiTXt (const char *data,
             unsigned int length, 
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  unsigned int pos;
  char *keyword;
  const char *language;
  const char *translated;
  int i;
  int compressed;
  char *buf;
  char *lan;
  uLongf bufLen;
  int ret;
  int zret;

  pos = stnlen (data, length) + 1;
  if (pos + 3 >= length)
    return 0;
  compressed = data[pos++];
  if (compressed && (data[pos++] != 0))
    return 0;                /* bad compression method */
  language = &data[pos];
  ret = 0;
  if (stnlen (language, length - pos) > 0)
    {
      lan = stndup (language, length - pos);
      ADDF (EXTRACTOR_METATYPE_LANGUAGE,
	    lan);
    }
  pos += stnlen (language, length - pos) + 1;
  if (pos + 1 >= length)
    return 0;
  translated = &data[pos];      /* already in utf-8! */
  if (stnlen (translated, length - pos) > 0)
    {
      lan = stndup (translated, length - pos);
      ADDF (EXTRACTOR_METATYPE_KEYWORDS,
	    lan);
    }
  pos += stnlen (translated, length - pos) + 1;
  if (pos >= length)
    return 0;

  if (compressed)
    {
      bufLen = 1024 + 2 * (length - pos);
      while (1)
        {
          if (bufLen * 2 < bufLen)
            return 0;
          bufLen *= 2;
          if (bufLen > 50 * (length - pos))
            {
              /* printf("zlib problem"); */
              return 0;
            }
          buf = malloc (bufLen);
          if (buf == NULL)
            {
              /* printf("out of memory"); */
              return 0;      /* out of memory */
            }
          zret = uncompress ((Bytef *) buf,
                            &bufLen,
                            (const Bytef *) &data[pos], length - pos);
          if (zret == Z_OK)
            {
              /* printf("zlib ok"); */
              break;
            }
          free (buf);
          if (zret != Z_BUF_ERROR)
            return 0;        /* unknown error, abort */
        }
      keyword = stndup (buf, bufLen);
      free (buf);
    }
  else
    {
      keyword = stndup (&data[pos], length - pos);
    }
  i = 0;
  while (tagmap[i].name != NULL)
    {
      if (0 == strcmp (tagmap[i].name, data))
	{
	  ADDF (tagmap[i].type, keyword /* already in utf8 */);
	  return 0;
	}
      i++;
    }
  ADDF (EXTRACTOR_METATYPE_COMMENT, keyword);
 FINISH:
  return ret;
}


static int
processIHDR (const char *data,
             unsigned int length, 
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  char tmp[128];
  int ret;

  if (length < 12)
    return 0;
  ret = 0;
  snprintf (tmp,
            sizeof(tmp),
            "%ux%u",
            htonl (getIntAt (&data[4])), htonl (getIntAt (&data[8])));
  ADD (EXTRACTOR_METATYPE_IMAGE_DIMENSIONS, tmp);
 FINISH:
  return ret;
}

static int
processzTXt (const char *data,
             unsigned int length,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  char *keyword;
  unsigned int off;
  int i;
  char *buf;
  uLongf bufLen;
  int zret;
  int ret;

  data += 4;
  off = stnlen (data, length) + 1;
  if (off >= length)
    return 0;                /* failed to find '\0' */
  if (data[off] != 0)
    return 0;                /* compression method must be 0 */
  off++;
  ret = 0;
  bufLen = 1024 + 2 * (length - off);
  while (1)
    {
      if (bufLen * 2 < bufLen)
        return 0;
      bufLen *= 2;
      if (bufLen > 50 * (length - off))
        {
          /* printf("zlib problem"); */
          return 0;
        }
      buf = malloc (bufLen);
      if (buf == NULL)
        {
          /* printf("out of memory"); */
          return 0;          /* out of memory */
        }
      zret = uncompress ((Bytef *) buf,
			 &bufLen, (const Bytef *) &data[off], length - off);
      if (zret == Z_OK)
        {
          /* printf("zlib ok"); */
          break;
        }
      free (buf);
      if (zret != Z_BUF_ERROR)
        return 0;            /* unknown error, abort */
    }
  keyword = EXTRACTOR_common_convert_to_utf8 (buf, bufLen, "ISO-8859-1");
  free (buf);
  i = 0;
  while (tagmap[i].name != NULL)
    {
      if (0 == strcmp (tagmap[i].name, data))
	{
	  ADDF (tagmap[i].type, keyword);
	  return 0;
	}
      i++;
    }
  ADDF (EXTRACTOR_METATYPE_COMMENT, keyword);
 FINISH:
  return ret;
}

static int
processtIME (const char *data,
             unsigned int length,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  unsigned short y;
  unsigned int year;
  unsigned int mo;
  unsigned int day;
  unsigned int h;
  unsigned int m;
  unsigned int s;
  char val[256];
  int ret;

  if (length != 7)
    return 0;
  ret = 0;
  memcpy (&y, &data[4], sizeof (unsigned short));
  year = ntohs (y);
  mo = (unsigned char) data[6];
  day = (unsigned char) data[7];
  h = (unsigned char) data[8];
  m = (unsigned char) data[9];
  s = (unsigned char) data[10];
  snprintf (val, 
	    sizeof(val),
	    "%04u-%02u-%02u %02d:%02d:%02d", year, mo, day, h, m, s);
  ADD (EXTRACTOR_METATYPE_MODIFICATION_DATE, val);
 FINISH:
  return ret;
}

#define PNG_HEADER "\211PNG\r\n\032\n"



int 
EXTRACTOR_png_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  const char *pos;
  const char *end;
  unsigned int length;
  int ret;

  if (size < strlen (PNG_HEADER))
    return 0;
  if (0 != strncmp (data, PNG_HEADER, strlen (PNG_HEADER)))
    return 0;
  end = &data[size];
  pos = &data[strlen (PNG_HEADER)];
  ADD (EXTRACTOR_METATYPE_MIMETYPE, "image/png");
  ret = 0;
  while (ret == 0)
    {
      if (pos + 12 >= end)
        break;
      length = htonl (getIntAt (pos));
      pos += 4;
      /* printf("Length: %u, pos %u\n", length, pos - data); */
      if ((pos + 4 + length + 4 > end) || (pos + 4 + length + 4 < pos + 8))
        break;
      if (0 == strncmp (pos, "IHDR", 4))
        ret = processIHDR (pos, length, proc, proc_cls);
      if (0 == strncmp (pos, "iTXt", 4))
        ret = processiTXt (pos, length, proc, proc_cls);
      if (0 == strncmp (pos, "tEXt", 4))
        ret = processtEXt (pos, length, proc, proc_cls);
      if (0 == strncmp (pos, "zTXt", 4))
        ret = processzTXt (pos, length, proc, proc_cls);
      if (0 == strncmp (pos, "tIME", 4))
        ret = processtIME (pos, length, proc, proc_cls);
      pos += 4 + length + 4;    /* Chunk type, data, crc */
    }
 FINISH:
  return ret;
}
