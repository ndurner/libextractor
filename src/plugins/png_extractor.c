/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */
/**
 * @file plugins/png_extractor.c
 * @brief plugin to support PNG files
 * @author Christian Grothoff
 */
#include "platform.h"
#include <zlib.h>
#include "extractor.h"
#include "convert.h"

/**
 * Header that every PNG file must start with.
 */
#define PNG_HEADER "\211PNG\r\n\032\n"


/**
 * Function to create 0-terminated string from the
 * first n characters of the given input.
 *
 * @param str input string
 * @param n length of the input
 * @return n-bytes from str followed by 0-termination, NULL on error
 */
static char *
stndup (const char *str, 
	size_t n)
{
  char *tmp;

  if (NULL == (tmp = malloc (n + 1)))
    return NULL;
  tmp[n] = '\0';
  memcpy (tmp, str, n);
  return tmp;
}


/**
 * strnlen is GNU specific, let's redo it here to be
 * POSIX compliant.
 *
 * @param str input string
 * @param maxlen maximum length of str
 * @return first position of 0-terminator in str, or maxlen
 */
static size_t
stnlen (const char *str, 
	size_t maxlen)
{
  size_t ret;

  ret = 0;
  while ( (ret < maxlen) &&
	  ('\0' != str[ret]) )
    ret++;
  return ret;
}


/**
 * Interpret the 4 bytes in 'buf' as a big-endian
 * encoded 32-bit integer, convert and return.
 *
 * @param pos (unaligned) pointer to 4 byte integer
 * @return converted integer in host byte order
 */
static uint32_t
get_int_at (const void *pos)
{
  uint32_t i;

  memcpy (&i, pos, sizeof (i));
  return htonl (i);
}


/**
 * Map from PNG meta data descriptor strings
 * to LE types.
 */
static struct
{
  /**
   * PNG name.
   */
  const char *name;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
} tagmap[] =
{
  { "Author", EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "Description", EXTRACTOR_METATYPE_DESCRIPTION },
  { "Comment", EXTRACTOR_METATYPE_COMMENT },
  { "Copyright", EXTRACTOR_METATYPE_COPYRIGHT },
  { "Source", EXTRACTOR_METATYPE_SOURCE_DEVICE },
  { "Creation Time", EXTRACTOR_METATYPE_CREATION_DATE },
  { "Title", EXTRACTOR_METATYPE_TITLE },
  { "Software", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE },
  { "Disclaimer", EXTRACTOR_METATYPE_DISCLAIMER },
  { "Warning", EXTRACTOR_METATYPE_WARNING },
  { "Signature", EXTRACTOR_METATYPE_UNKNOWN },
  { NULL, EXTRACTOR_METATYPE_RESERVED }
};


/**
 * Give the given metadata to LE.  Set "ret" to 1 and
 * goto 'FINISH' if LE says we are done.
 *
 * @param t type of the metadata
 * @param s utf8 string with the metadata
 */
#define ADD(t,s) do { if (0 != (ret = ec->proc (ec->cls, "png", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1))) goto FINISH; } while (0)


/**
 * Give the given metadata to LE and free the memory.  Set "ret" to 1 and
 * goto 'FINISH' if LE says we are done.
 *
 * @param t type of the metadata
 * @param s utf8 string with the metadata, to be freed afterwards
 */
#define ADDF(t,s) do { if ( (NULL != s) && (0 != (ret = ec->proc (ec->cls, "png", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1))) ) { free (s); goto FINISH; } if (NULL != s) free (s); } while (0)


/**
 * Process EXt tag.
 *
 * @param ec extraction context
 * @param length length of the tag
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processtEXt (struct EXTRACTOR_ExtractContext *ec,
             uint32_t length)
{
  void *ptr;
  unsigned char *data;
  char *keyword;
  size_t off;
  unsigned int i;
  int ret;

  if (length != ec->read (ec->cls, &ptr, length))
    return 1;
  data = ptr;
  off = stnlen ((char*) data, length) + 1;
  if (off >= length)
    return 0;                /* failed to find '\0' */
  if (NULL == (keyword = EXTRACTOR_common_convert_to_utf8 ((char*) &data[off],
							   length - off, 
							   "ISO-8859-1")))
    return 0;
  ret = 0;
  for (i = 0; NULL != tagmap[i].name; i++)
    if (0 == strcmp (tagmap[i].name, (char*) data))
      {
	ADDF (tagmap[i].type, keyword);
	return 0;
      }
  ADDF (EXTRACTOR_METATYPE_KEYWORDS, keyword);
FINISH:
  return ret;
}


/**
 * Process iTXt tag.
 *
 * @param ec extraction context
 * @param length length of the tag
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processiTXt (struct EXTRACTOR_ExtractContext *ec,
             uint32_t length)
{
  void *ptr;
  unsigned char *data;
  size_t pos;
  char *keyword;
  const char *language;
  const char *translated;
  unsigned int i;
  int compressed;
  char *buf;
  char *lan;
  uLongf bufLen;
  int ret;
  int zret;

  if (length != ec->read (ec->cls, &ptr, length))
    return 1;
  data = ptr;
  pos = stnlen ((char *) data, length) + 1;
  if (pos >= length)
    return 0;
  compressed = data[pos++];
  if (compressed && (0 != data[pos++]))
    return 0;                /* bad compression method */
  language = (char *) &data[pos];
  ret = 0;
  if ( (stnlen (language, length - pos) > 0) &&
       (NULL != (lan = stndup (language, length - pos))) )
    ADDF (EXTRACTOR_METATYPE_LANGUAGE, lan);
  pos += stnlen (language, length - pos) + 1;
  if (pos + 1 >= length)
    return 0;
  translated = (char*) &data[pos];      /* already in utf-8! */
  if ( (stnlen (translated, length - pos) > 0) &&
       (NULL != (lan = stndup (translated, length - pos))) )
    ADDF (EXTRACTOR_METATYPE_KEYWORDS, lan);
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
          if (NULL == (buf = malloc (bufLen)))
            {
              /* printf("out of memory"); */
              return 0;      /* out of memory */
            }
          if (Z_OK == 
	      (zret = uncompress ((Bytef *) buf,
				  &bufLen,
				  (const Bytef *) &data[pos], length - pos)))
            {
              /* printf("zlib ok"); */
              break;
            }
          free (buf);
          if (Z_BUF_ERROR != zret)
            return 0;        /* unknown error, abort */
        }
      keyword = stndup (buf, bufLen);
      free (buf);
    }
  else
    {
      keyword = stndup ((char *) &data[pos], length - pos);
    }
  if (NULL == keyword)
    return ret;
  for (i = 0; NULL != tagmap[i].name; i++)
    if (0 == strcmp (tagmap[i].name, (char*) data))
      {
	ADDF (tagmap[i].type, keyword /* already in utf8 */);
	return 0;
      }
  ADDF (EXTRACTOR_METATYPE_COMMENT, keyword);
FINISH:
  return ret;
}


/**
 * Process IHDR tag.
 *
 * @param ec extraction context
 * @param length length of the tag
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processIHDR (struct EXTRACTOR_ExtractContext *ec,
             uint32_t length)
{
  void *ptr;
  unsigned char *data;
  char tmp[128];
  int ret;

  if (length < 12)
    return 0;
  if (length != ec->read (ec->cls, &ptr, length))
    return 1;
  data = ptr;
  ret = 0;
  snprintf (tmp,
            sizeof (tmp),
            "%ux%u",
            get_int_at (data), get_int_at (&data[4]));
  ADD (EXTRACTOR_METATYPE_IMAGE_DIMENSIONS, tmp);
FINISH:
  return ret;
}


/**
 * Process zTXt tag.
 *
 * @param ec extraction context
 * @param length length of the tag
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processzTXt (struct EXTRACTOR_ExtractContext *ec,
             uint32_t length)
{
  void *ptr;
  unsigned char *data;
  char *keyword;
  size_t off;
  unsigned int i;
  char *buf;
  uLongf bufLen;
  int zret;
  int ret;

  if (length != ec->read (ec->cls, &ptr, length))
    return 1;
  data = ptr;
  off = stnlen ((char *) data, length) + 1;
  if (off >= length)
    return 0;                /* failed to find '\0' */
  if (0 != data[off])
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
      if (NULL == (buf = malloc (bufLen)))
        {
          /* printf("out of memory"); */
          return 0;          /* out of memory */
        }
      if (Z_OK == 
	  (zret = uncompress ((Bytef *) buf,
			      &bufLen,
			      (const Bytef *) &data[off], 
			      length - off)))
        {
          /* printf("zlib ok"); */
          break;
        }
      free (buf);
      if (Z_BUF_ERROR != zret)
        return 0;            /* unknown error, abort */
    }
  keyword = EXTRACTOR_common_convert_to_utf8 (buf, 
					      bufLen, 
					      "ISO-8859-1");
  free (buf);
  for (i = 0; NULL != tagmap[i].name; i++)
    if (0 == strcmp (tagmap[i].name, (char*)  data))
      {
	ADDF (tagmap[i].type, keyword);
	return 0;
      }
  ADDF (EXTRACTOR_METATYPE_COMMENT, keyword);
FINISH:
  return ret;
}


/**
 * Process IME tag.
 *
 * @param ec extraction context
 * @param length length of the tag
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processtIME (struct EXTRACTOR_ExtractContext *ec,
             uint32_t length)
{
  void *ptr;
  unsigned char *data;
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
  if (length != ec->read (ec->cls, &ptr, length))
    return 1;
  data = ptr;
  ret = 0;
  memcpy (&y, data, sizeof (uint16_t));
  year = ntohs (y);
  mo = (unsigned char) data[6];
  day = (unsigned char) data[7];
  h = (unsigned char) data[8];
  m = (unsigned char) data[9];
  s = (unsigned char) data[10];
  snprintf (val, 
	    sizeof (val),
	    "%04u-%02u-%02u %02d:%02d:%02d", 
	    year, mo, day, h, m, s);
  ADD (EXTRACTOR_METATYPE_MODIFICATION_DATE, val);
FINISH:
  return ret;
}


/**
 * Main entry method for the 'image/png' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_png_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *data;
  uint32_t length;
  int64_t pos;
  int ret;
  ssize_t len;

  len = strlen (PNG_HEADER);
  if (len != ec->read (ec->cls, &data, len))
    return;
  if (0 != strncmp ((const char*) data, PNG_HEADER, len))
    return;
  ADD (EXTRACTOR_METATYPE_MIMETYPE, "image/png");
  ret = 0;
  while (0 == ret)
    {
      if (sizeof (uint32_t) + 4 != ec->read (ec->cls, 
					     &data, 
					     sizeof (uint32_t) + 4))
        break;
      length = get_int_at (data);
      if (0 > (pos = ec->seek (ec->cls, 0, SEEK_CUR)))
        break;
      pos += length + 4; /* Chunk type, data, crc */
      if (0 == strncmp ((char*) data + sizeof (uint32_t), "IHDR", 4))
        ret = processIHDR (ec, length);
      if (0 == strncmp ((char*) data + sizeof (uint32_t), "iTXt", 4))
        ret = processiTXt (ec, length);
      if (0 == strncmp ((char*) data + sizeof (uint32_t), "tEXt", 4))
        ret = processtEXt (ec, length);
      if (0 == strncmp ((char*) data + sizeof (uint32_t), "zTXt", 4))
        ret = processzTXt (ec, length);
      if (0 == strncmp ((char*) data + sizeof (uint32_t), "tIME", 4))
        ret = processtIME (ec, length);
      if (ret != 0)
        break;
      if (pos != ec->seek (ec->cls, pos, SEEK_SET))
        break;
    }
FINISH:
  return;
}

/* end of png_extractor.c */
