/*
     This file is part of libextractor.
     (C) 2006 Vidyut Samanta and Christian Grothoff

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

static char * stndup(const char * str,
		     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
}

/**
 * strnlen is GNU specific, let's redo it here to be
 * POSIX compliant.
 */
static size_t stnlen(const char * str,
		     size_t maxlen) {
  size_t ret;
  ret = 0;
  while ( (ret < maxlen) &&
	  (str[ret] != '\0') )
    ret++;
  return ret;
}

static struct EXTRACTOR_Keywords * 
addKeyword(EXTRACTOR_KeywordType type,
	   const char * keyword,
	   struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = strdup(keyword);
  result->keywordType = type;
  return result;
}

static struct {
  char * name;
  EXTRACTOR_KeywordType type;
} tagmap[] = {
   { "Author" , EXTRACTOR_AUTHOR},
   { "Description" , EXTRACTOR_DESCRIPTION},
   { "Comment", EXTRACTOR_COMMENT},
   { "Copyright", EXTRACTOR_COPYRIGHT},
   { "Source", EXTRACTOR_SOURCE},
   { "Creation Time", EXTRACTOR_DATE},
   { "Title", EXTRACTOR_TITLE},
   { "Software", EXTRACTOR_SOFTWARE},
   { "Disclaimer", EXTRACTOR_DISCLAIMER},
   { "Warning", EXTRACTOR_WARNING},
   { "Signature", EXTRACTOR_RESOURCE_IDENTIFIER},
   { NULL, EXTRACTOR_UNKNOWN},
};

#define PDF_HEADER "%PDF"
#define PDF_EOF "%%EOF"
#define PDF_SXR "startxref"
#define PDF_XREF "xref"
#define PDF_INFO "/Info "
#define PDF_TRAILER "trailer"
#define MAX_STEPS 256

#define IS_NL(c) ((c == '\n') || (c == '\r'))
#define MIN(a,b) ((a) < (b) ? (a) : (b)) 
#define SKIP(k,p,b,s) while ( (p<s) && (NULL != strchr(k, b[p])) ) p++;

struct EXTRACTOR_Keywords * 
libextractor_pdf_extract(const char * filename,
			 const char * data,
			 size_t size,
			 struct EXTRACTOR_Keywords * prev) {
  size_t pos;
  size_t steps;
  unsigned int xstart;
  unsigned int xcount;
  unsigned int xinfo;
  unsigned long long startxref;
  unsigned long long xrefpos;
  int haveValidXref;
  unsigned long long info_offset;
  char buf[MAX_STEPS+1];
  int i;

  while ( (size > 0) && (IS_NL(data[size-1])) )
    size--;
  if (size < strlen(PDF_HEADER) + strlen(PDF_EOF) + strlen(PDF_SXR) + 3)
    return prev;
  if (0 != memcmp(data, PDF_HEADER, strlen(PDF_HEADER)))
    return prev;
  if (0 != memcmp(&data[size - strlen(PDF_EOF)], PDF_EOF, strlen(PDF_EOF))) 
    return prev;
  
  pos = size - strlen(PDF_EOF) - strlen(PDF_SXR);
  steps = 0;
  while ( (steps++ < MAX_STEPS) &&
	  (pos > 0) &&
	  (0 != memcmp(&data[pos], PDF_SXR, strlen(PDF_SXR))) ) 
    pos--;
  printf("pos: %u\n", pos);
  if (0 != memcmp(&data[pos], PDF_SXR, strlen(PDF_SXR)))
    return prev; 
  memcpy(buf, &data[pos + strlen(PDF_SXR)], steps);
  buf[steps] = '\0';
  if (1 != sscanf(buf, "%llu", &startxref)) 
    return prev;
  printf("startxref: %llu\n", startxref);
  if (startxref >= size - strlen(PDF_XREF))
    return prev;
  if (0 != memcmp(&data[startxref], PDF_XREF, strlen(PDF_XREF)))
    return prev;
  haveValidXref = 0;
  xrefpos = startxref + strlen(PDF_XREF);

  while (1) {
    pos = xrefpos;
    while ( (pos < size) && (IS_NL(data[pos])) )
      pos++;
    memcpy(buf, &data[pos], MIN(MAX_STEPS, size - pos));
    buf[MIN(MAX_STEPS,size-pos)] = '\0';
    if (2 != sscanf(buf, "%u %u", &xstart, &xcount)) 
      break;
    printf("xstart: %u - xcount: %u - pos %u\n",
	   xstart,
	   xcount,
	   pos);
    while ( (pos < size) && (! IS_NL(data[pos])) )
      pos++;
    if ( (pos < size) && IS_NL(data[pos]))
      pos++;
    xrefpos = 20 * xcount + pos;    
    if ( (xrefpos >= size) || (xrefpos < pos) )
      return prev; /* invalid xref size */
    haveValidXref = 1;
    printf("xref portion ends at %llu\n",
	   xrefpos);
  }
  if (! haveValidXref)
    return prev;
  if (size - pos < strlen(PDF_TRAILER))
    return prev;
  if (0 != memcmp(&data[pos],
		  PDF_TRAILER,
		  strlen(PDF_TRAILER))) 
    return prev;
  pos += strlen(PDF_TRAILER);
  SKIP("<< \n\r", pos, data, size);
  while ( (pos < size) &&
	  (pos + strlen(PDF_INFO) < size) &&
	  (0 != memcmp(&data[pos],
		       PDF_INFO,
		       strlen(PDF_INFO))) ) {
    while ( (pos < size) &&
	    (! IS_NL(data[pos]) ) ) {
      if ( (data[pos] == '>') &&
	   (pos + 1 < size) &&
	   (data[pos+1] == '>') ) 
	return prev; /* no info */      
      pos++;
    }
    while ( (pos < size) &&
	    (IS_NL(data[pos]) || isspace(data[pos]) ) )
      pos++;
  }
  if ( ! ( (pos < size) &&
	   (pos + strlen(PDF_INFO) < size) &&
	   (0 == memcmp(&data[pos],
			PDF_INFO,
			strlen(PDF_INFO))) ) ) 
    return prev;

  pos += strlen(PDF_INFO);
  memcpy(buf,
	 &data[pos],
	 MIN(MAX_STEPS, size - pos));
  buf[MIN(MAX_STEPS,size-pos)] = '\0';
  for (i=0;i<MIN(MAX_STEPS,size-pos);i++)
    if (isspace(buf[i])) {
      buf[i] = '\0';
      break;
    }
  if (1 != sscanf(buf, "%u", &xinfo)) 
    return prev;
  printf("xinfo: %u\n", xinfo);

  haveValidXref = 0;  
  /* now go find xinfo in xref table */
  xrefpos = startxref + strlen(PDF_XREF);
  while (1) {
    pos = xrefpos;
    while ( (pos < size) && (IS_NL(data[pos])) )
      pos++;
    memcpy(buf, &data[pos], MIN(MAX_STEPS, size - pos));
    buf[MIN(MAX_STEPS,size-pos)] = '\0';
    if (2 != sscanf(buf, "%u %u", &xstart, &xcount)) 
      break;
    printf("xstart: %u - xcount: %u - pos %u\n",
	   xstart,
	   xcount,
	   pos);
    while ( (pos < size) && (! IS_NL(data[pos])) )
      pos++;
    if ( (pos < size) && IS_NL(data[pos]))
      pos++;
    if ( (xinfo > xstart) &&
	 (xinfo < xstart + xcount) ) {
      haveValidXref = 1;
      pos += 20 * xinfo - xstart;
      memcpy(buf, &data[pos], 20);
      buf[20] = '\0';
      sscanf(buf, "%10llu %*5u %*c", &info_offset);
      

      break;
    }
    xrefpos = 20 * xcount + pos;    
    if ( (xrefpos >= size) || (xrefpos < pos) )
      return prev; /* invalid xref size */
  }
  if (! haveValidXref)
    return prev;

  /* read size of xref */
  /* parse xref */
  /* find info index */
  /* parse info */

  return prev;
}

