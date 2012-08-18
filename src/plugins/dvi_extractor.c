/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/dvi_extractor.c
 * @brief plugin to support DVI files (from LaTeX)
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"


/**
 * Pair of a PostScipt prefix and the corresponding LE type.
 */
struct Matches
{
  /**
   * Prefix in the PS map.
   */
  const char *text;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * Map from PS names to LE types.
 */
static struct Matches tmap[] = {
  { "/Title (",    EXTRACTOR_METATYPE_TITLE },
  { "/Subject (",  EXTRACTOR_METATYPE_SUBJECT },
  { "/Author (",   EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "/Keywords (", EXTRACTOR_METATYPE_KEYWORDS },
  { "/Creator (",  EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  { "/Producer (", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE },
  { NULL, 0 } 
};


/**
 * Parse a "ZZZ" tag.  Specifically, the data may contain a 
 * postscript dictionary with metadata.
 *
 * @param data overall input stream
 * @param pos where in data is the zzz data
 * @param len how many bytes from 'pos' does the zzz data extend?
 * @param proc function to call with meta data found
 * @param proc_cls closure for proc
 * @return 0 to continue to extract, 1 to stop
 */
static int
parseZZZ (const char *data,
          size_t pos, size_t len,
	  EXTRACTOR_MetaDataProcessor proc,
	  void *proc_cls)
{
  size_t slen;
  size_t end;
  unsigned int i;

  end = pos + len;
  slen = strlen ("ps:SDict begin [");
  if ( (len <= slen) ||
       (0 != strncmp ("ps:SDict begin [ ", &data[pos], slen)) )
    return 0;
  pos += slen;
  while (pos < end)
    {
      for (i = 0; NULL != tmap[i].text; i++)
        {
          slen = strlen (tmap[i].text);
          if ( (pos + slen > end) ||
	       (0 != strncmp (&data[pos], tmap[i].text, slen)) )
	    continue;
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
			   slen + 1))
	      return 1;
	  }
	  pos += slen + 1;
	  break;
	}
      pos++;
    }
  return 0;
}


/**
 * Read 32-bit unsigned integer in big-endian format from 'data'.
 *
 * @param data pointer to integer (possibly unaligned)
 * @return 32-bit integer in host byte order
 */
static uint32_t
getIntAt (const void *data)
{
  uint32_t p;

  memcpy (&p, data, 4);          /* ensure alignment! */
  return ntohl (p);
}


/**
 * Read 16-bit unsigned integer in big-endian format from 'data'.
 *
 * @param data pointer to integer (possibly unaligned)
 * @return 16-bit integer in host byte order
 */
static uint16_t
getShortAt (const void *data)
{
  uint16_t p;

  memcpy (&p, data, sizeof (uint16_t));          /* ensure alignment! */
  return ntohs (p);
}


/**
 * Main entry method for the 'application/x-dvi' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_dvi_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  unsigned int klen;
  uint32_t pos;
  uint32_t opos;
  unsigned int len;
  unsigned int pageCount;
  char pages[16];
  void *buf;
  unsigned char *data;
  uint64_t size;
  uint64_t off;
  ssize_t iret;
  
  if (40 >= (iret = ec->read (ec->cls, &buf, 1024)))
    return;
  data = buf;
  if ((data[0] != 247) || (data[1] != 2))
    return;                /* cannot be DVI or unsupported version */
  klen = data[14];
  size = ec->get_size (ec->cls);
  if (size > 16 * 1024 * 1024)
    return; /* too large */
  if (NULL == (data = malloc ((size_t) size)))
    return; /* out of memory */
  memcpy (data, buf, iret);
  off = iret;
  while (off < size)
    {
      if (0 >= (iret = ec->read (ec->cls, &buf, 16 * 1024)))
	{
	  free (data);
	  return;
	}
      memcpy (&data[off], buf, iret);
      off += iret;
    }
  pos = size - 1;
  while ((223 == data[pos]) && (pos > 0))
    pos--;
  if ((2 != data[pos]) || (pos < 40))
    goto CLEANUP;
  pos--;
  pos -= 4;
  /* assert pos at 'post_post tag' */
  if (data[pos] != 249)
    goto CLEANUP;
  opos = pos;
  pos = getIntAt (&data[opos + 1]);
  if (pos + 25 > size)
    goto CLEANUP;
  /* assert pos at 'post' command */
  if (data[pos] != 248)
    goto CLEANUP;
  pageCount = 0;
  opos = pos;
  pos = getIntAt (&data[opos + 1]);
  while (1)
    {
      if (UINT32_MAX == pos)
        break;
      if (pos + 45 > size)
	goto CLEANUP;
      if (data[pos] != 139)     /* expect 'bop' */
	goto CLEANUP;
      pageCount++;
      opos = pos;
      pos = getIntAt (&data[opos + 41]);
      if (UINT32_MAX == pos)
        break;
      if (pos >= opos)
	goto CLEANUP;           /* invalid! */
    }
  /* ok, now we believe it's a dvi... */
  snprintf (pages,
	    sizeof (pages),
	    "%u", 
	    pageCount);
  if (0 != ec->proc (ec->cls, 
		     "dvi",
		     EXTRACTOR_METATYPE_PAGE_COUNT,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     pages,
		     strlen (pages) + 1))
    goto CLEANUP;
  if (0 != ec->proc (ec->cls, 
		     "dvi",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "application/x-dvi",
		     strlen ("application/x-dvi") + 1))
    goto CLEANUP;
  {
    char comment[klen + 1];
    
    comment[klen] = '\0';
    memcpy (comment, &data[15], klen);
    if (0 != ec->proc (ec->cls, 
		       "dvi",
		       EXTRACTOR_METATYPE_COMMENT,
		       EXTRACTOR_METAFORMAT_C_STRING,
		       "text/plain",
		       comment,
		       klen + 1))
      goto CLEANUP;
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
            if (0 != parseZZZ ((const char *) data, pos + 2, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 2;
          break;
        case 240:              /* zzz2 */
          len = getShortAt (&data[pos + 1]);
          if (pos + 3 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 3, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 3;
          break;
        case 241:              /* zzz3, who uses that? */
          len = (getShortAt (&data[pos + 1])) + 65536 * data[pos + 3];
          if (pos + 4 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 4, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 4;
          break;
        case 242:              /* zzz4, hurray! */
          len = getIntAt (&data[pos + 1]);
          if (pos + 1 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 5, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 5;
          break;
        default:               /* unsupported opcode, abort scan */
	  goto CLEANUP;
        }
    }
 CLEANUP:
  free (data);
}

/* end of dvi_extractor.c */
