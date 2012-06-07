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

#include "extractor_plugins.h"


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
processtEXt (struct EXTRACTOR_PluginList *plugin,
             unsigned int length,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  unsigned char *data;
  char *keyword;
  unsigned int off;
  int i;
  int ret;

  if (length != pl_read (plugin, &data, length))
    return 1;

  //data += 4;
  off = stnlen ((char*) data, length) + 1;
  if (off >= length)
    return 0;                /* failed to find '\0' */
  keyword = EXTRACTOR_common_convert_to_utf8 ( (char*) &data[off], length - off, "ISO-8859-1");
  if (keyword == NULL)
    return 0;
  i = 0;
  ret = 0;
  while (tagmap[i].name != NULL)
    {
      if (0 == strcmp (tagmap[i].name, (char*) data))
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
processiTXt (struct EXTRACTOR_PluginList *plugin,
             unsigned int length, 
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  unsigned char *data;
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

  if (length != pl_read (plugin, &data, length))
    return 1;

  pos = stnlen ( (char*) data, length) + 1;
  if (pos >= length)
    return 0;
  compressed = data[pos++];
  if (compressed && (data[pos++] != 0))
    return 0;                /* bad compression method */
  language = (char*) &data[pos];
  ret = 0;
  if (stnlen (language, length - pos) > 0)
    {
      lan = stndup (language, length - pos);
      ADDF (EXTRACTOR_METATYPE_LANGUAGE, lan);
    }
  pos += stnlen (language, length - pos) + 1;
  if (pos + 1 >= length)
    return 0;
  translated = (char*) &data[pos];      /* already in utf-8! */
  if (stnlen (translated, length - pos) > 0)
    {
      lan = stndup (translated, length - pos);
      ADDF (EXTRACTOR_METATYPE_KEYWORDS, lan);
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
      keyword = stndup ((char*) &data[pos], length - pos);
    }
  i = 0;
  while (tagmap[i].name != NULL)
    {
      if (0 == strcmp (tagmap[i].name, (char*) data))
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
processIHDR (struct EXTRACTOR_PluginList *plugin,
             unsigned int length, 
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  unsigned char *data;
  char tmp[128];
  int ret;

  if (length < 12)
    return 0;

  if (length != pl_read (plugin, &data, length))
    return 1;

  ret = 0;
  snprintf (tmp,
            sizeof(tmp),
            "%ux%u",
            htonl (getIntAt (data)), htonl (getIntAt (&data[4])));
  ADD (EXTRACTOR_METATYPE_IMAGE_DIMENSIONS, tmp);
FINISH:
  return ret;
}

static int
processzTXt (struct EXTRACTOR_PluginList *plugin,
             unsigned int length,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  unsigned char *data;
  char *keyword;
  unsigned int off;
  int i;
  char *buf;
  uLongf bufLen;
  int zret;
  int ret;

  if (length != pl_read (plugin, &data, length))
    return 1;

  //data += 4;
  off = stnlen ( (char*) data, length) + 1;
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
      if (0 == strcmp (tagmap[i].name, (char*)  data))
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
processtIME (struct EXTRACTOR_PluginList *plugin,
             unsigned int length,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
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

  if (length != pl_read (plugin, &data, length))
    return 1;

  ret = 0;
  memcpy (&y, data, sizeof (unsigned short));
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
EXTRACTOR_png_extract_method (struct EXTRACTOR_PluginList *plugin,
    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  unsigned char *data;
  unsigned int length;
  int64_t pos;
  int ret;

  if (plugin == NULL)
    return 1;

  ret = strlen (PNG_HEADER);

  if (ret != pl_read (plugin, &data, ret))
    return 1;
  
  if (0 != strncmp ((char*) data, PNG_HEADER, ret))
    return 1;

  ADD (EXTRACTOR_METATYPE_MIMETYPE, "image/png");
  ret = 0;
  while (ret == 0)
    {
      if (4 != pl_read (plugin, &data, 4))
        break;
      length = htonl (getIntAt (data));
      /* printf("Length: %u, pos %u\n", length, pos - data); */
      if (4 != pl_read (plugin, &data, 4))
        break;
      pos = pl_get_pos (plugin);
      if (pos <= 0)
        break;
      pos += length + 4; /* Chunk type, data, crc */
      if (0 == strncmp ((char*) data, "IHDR", 4))
        ret = processIHDR (plugin, length, proc, proc_cls);
      if (0 == strncmp ((char*) data, "iTXt", 4))
        ret = processiTXt (plugin, length, proc, proc_cls);
      if (0 == strncmp ((char*)data, "tEXt", 4))
        ret = processtEXt (plugin, length, proc, proc_cls);
      if (0 == strncmp ((char*) data, "zTXt", 4))
        ret = processzTXt (plugin, length, proc, proc_cls);
      if (0 == strncmp ((char*) data, "tIME", 4))
        ret = processtIME (plugin, length, proc, proc_cls);
      if (ret != 0)
        break;
      if (pos != pl_seek (plugin, pos, SEEK_SET))
        break;
    }
FINISH:
  return 1;
}
