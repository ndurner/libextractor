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

#include "platform.h"
#include "extractor.h"

typedef struct
{
  char *text;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tmap[] = {
  {"/Title (",    EXTRACTOR_METATYPE_TITLE},
  {"/Subject (",  EXTRACTOR_METATYPE_SUBJECT},
  {"/Author (",   EXTRACTOR_METATYPE_AUTHOR_NAME},
  {"/Keywords (", EXTRACTOR_METATYPE_KEYWORDS},
  {"/Creator (",  EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  {"/Producer (", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE},
  {NULL, 0},
};

static int
parseZZZ (const char *data,
          size_t pos, size_t len,
	  EXTRACTOR_MetaDataProcessor proc,
	  void *proc_cls)
{
  size_t slen;
  size_t end;
  int i;

  end = pos + len;
  slen = strlen ("ps:SDict begin [");
  if (len <= slen)
    return 0;
  if (0 != strncmp ("ps:SDict begin [ ", &data[pos], slen))
    return 0;
  pos += slen;
  while (pos < end)
    {
      i = 0;
      while (tmap[i].text != NULL)
        {
          slen = strlen (tmap[i].text);
          if (pos + slen < end)
            {
              if (0 == strncmp (&data[pos], tmap[i].text, slen))
                {
                  pos += slen;
                  slen = pos;
                  while ((slen < end) && (data[slen] != ')'))
                    slen++;
                  slen = slen - pos;
		  {
		    char value[slen + 1];
		    value[slen] = '\0';
		    memcpy (value, &data[pos], slen);
		    if (0 != proc (proc_cls, 
				   "dvi",
				   tmap[i].type,
				   EXTRACTOR_METAFORMAT_C_STRING,
				   "text/plain",
				   value,
				   slen +1))
		      {
			return 1;
		      }
		  }
                  pos += slen + 1;
                }
            }
          i++;
        }
      pos++;
    }
  return 0;
}

static unsigned int
getIntAt (const void *data)
{
  char p[4];

  memcpy (p, data, 4);          /* ensure alignment! */
  return *(unsigned int *) &p[0];
}

static unsigned int
getShortAt (const void *data)
{
  char p[2];

  memcpy (p, data, 2);          /* ensure alignment! */
  return *(unsigned short *) &p[0];
}


int 
EXTRACTOR_dvi_extract (const unsigned char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  unsigned int klen;
  uint32_t pos;
  uint32_t opos;
  unsigned int len;
  unsigned int pageCount;
  char pages[16];

  if (size < 40)
    return 0;
  if ((data[0] != 247) || (data[1] != 2))
    return 0;                /* cannot be dvi or unsupported version */
  klen = data[14];

  pos = size - 1;
  while ((data[pos] == 223) && (pos > 0))
    pos--;
  if ((data[pos] != 2) || (pos < 40))
    return 0;
  pos--;
  pos -= 4;
  /* assert pos at 'post_post tag' */
  if (data[pos] != 249)
    return 0;
  opos = pos;
  pos = ntohl (getIntAt (&data[opos + 1]));
  if (pos + 25 > size)
    return 0;
  /* assert pos at 'post' command */
  if (data[pos] != 248)
    return 0;
  pageCount = 0;
  opos = pos;
  pos = ntohl (getIntAt (&data[opos + 1]));
  while (1)
    {
      if (pos == UINT32_MAX)
        break;
      if (pos + 45 > size)
        return 0;
      if (data[pos] != 139)     /* expect 'bop' */
        return 0;
      pageCount++;
      opos = pos;
      pos = ntohl (getIntAt (&data[opos + 41]));
      if (pos == UINT32_MAX)
        break;
      if (pos >= opos)
        return 0;            /* invalid! */
    }
  /* ok, now we believe it's a dvi... */
  snprintf (pages, sizeof(pages), "%u", pageCount);
  if (0 != proc (proc_cls, 
		 "dvi",
		 EXTRACTOR_METATYPE_PAGE_COUNT,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 pages,
		 strlen (pages) +1))
    return 1;
  if (0 != proc (proc_cls, 
		 "dvi",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "application/x-dvi",
		 strlen ("application/x-dvi") +1))
    return 1;
  {
    char comment[klen + 1];
    
    comment[klen] = '\0';
    memcpy (comment, &data[15], klen);
    if (0 != proc (proc_cls, 
		   "dvi",
		   EXTRACTOR_METATYPE_COMMENT,
		   EXTRACTOR_METAFORMAT_UTF8,
		   "text/plain",
		   comment,
		   klen +1))
      return 1;
  }
  /* try to find PDF/ps special */
  pos = opos;
  while (pos < size - 100)
    {
      switch (data[pos])
        {
        case 139:              /* begin page 'bop', we typically have to skip that one to
                                   find the zzz's */
          pos += 45;            /* skip bop */
          break;
        case 239:              /* zzz1 */
          len = data[pos + 1];
          if (pos + 2 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 2, len, proc, proc_cls))
	      return 1;
          pos += len + 2;
          break;
        case 240:              /* zzz2 */
          len = ntohs (getShortAt (&data[pos + 1]));
          if (pos + 3 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 3, len, proc, proc_cls))
	      return 1;
          pos += len + 3;
          break;
        case 241:              /* zzz3, who uses that? */
          len = (ntohs (getShortAt (&data[pos + 1]))) + 65536 * data[pos + 3];
          if (pos + 4 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 4, len, proc, proc_cls))
	      return 1;
          pos += len + 4;
          break;
        case 242:              /* zzz4, hurray! */
          len = ntohl (getIntAt (&data[pos + 1]));
          if (pos + 1 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 5, len, proc, proc_cls))
	      return 1;
          pos += len + 5;
          break;
        default:               /* unsupported opcode, abort scan */
          return 0;
        }
    }
  return 0;
}
