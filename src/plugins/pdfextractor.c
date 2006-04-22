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

/**
 * TODO:
 * - code clean up (factor out some parsing aspects?)
 * - proper dictionary support
 * - filters (compression!)
 * - page count (and other document catalog information,
 *   such as language, viewer preferences, page layout,
 *   Metadatastreams (10.2.2), legal and permissions info)
 * - pdf 1.5 support ((compressed) cross reference streams)
 */

#include "platform.h"
#include "extractor.h"
#include <zlib.h>
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 1
#endif
#include <time.h>
#include "convert.h"

static char * stndup(const char * str,
		     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
}

static struct EXTRACTOR_Keywords * 
addKeyword(EXTRACTOR_KeywordType type,
	   char * keyword,
	   struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}



static char * 
dateDecode(const char * pdfString) {
  unsigned char * ret;

  if (pdfString == NULL)
    return NULL;
  if (strlen(pdfString) < 4)
    return NULL;
  return stndup(&pdfString[3], strlen(pdfString) - 4);
}

static unsigned char * 
stringDecode(const char * pdfString,
	     size_t * size) {
  size_t slen;
  size_t r;
  size_t w;
  unsigned char * ret;
  char hex[3];
  int i;
  int val;

  slen = strlen(pdfString);
  if (slen < 2)
    return NULL;
  switch (pdfString[0]) {
  case '(':
    if (pdfString[slen-1] != ')')    
      return NULL;
    ret = malloc(slen);
    w = 0;
    for (r=1;r<slen-1;r++) {
      if (pdfString[r] == '/') {
	r++;
	switch (pdfString[r]) {
	case '/':
	  ret[w++] = '/';
	  break;
	case 'n':
	  ret[w++] = '\n';
	  break;
	case 'r':
	  ret[w++] = '\r';
	  break;
	case 't':
	  ret[w++] = '\t';
	  break;
	case 'b':
	  ret[w++] = '\b';
	  break;
	case 'f':
	  ret[w++] = '\f';
	  break;
	case '(':
	  ret[w++] = '(';
	  break;
	case ')':
	  ret[w++] = ')';
	  break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9': {
	  char buf[4];
	  unsigned int u;
	  memset(buf, 0, 4);
	  buf[0] = pdfString[r++];
	  if ( (pdfString[r] >= '0') &&
	       (pdfString[r] <= '9') )
	    buf[1] = pdfString[r++];
	  if ( (pdfString[r] >= '0') &&
	       (pdfString[r] <= '9') )
	    buf[2] = pdfString[r++];
	  if (1 == sscanf(buf, "%o", &u)) {
	    ret[w++] = (char) u;
	  } else {
	    free(ret);
	    return NULL; /* invalid! */
	  }	       
	  break;
	}
	default: /* invalid */
	  free(ret);
	  return NULL;
	}
      } else {
	ret[w++] = pdfString[r];
      }
    }
    ret[w] = '/';
    *size = w;
    return ret;
  case '<':
    if (pdfString[slen-1] != '>')
      return NULL;
    hex[2] = '\0';
    ret = malloc(1 + ((slen - 1) / 2));
    for (i=0;i<slen-2;i+=2) {
      hex[0] = pdfString[i+1];
      hex[1] = '0';
      if (i + 1 < slen)
	hex[1] = pdfString[i+2];
      if ( (1 != sscanf(hex, "%x", &val)) &&
	   (1 != sscanf(hex, "%X", &val)) ) {
	free(ret);
	return NULL;
      }
      ret[i/2] = val;
    }
    ret[(slen-1)/2] = '\0';
    *size = (slen-1) / 2;
    return ret;
  }
  return NULL;
}

static char * 
charsetDecode(const unsigned char * in,
	      size_t size) {
  if (in == NULL)
    return NULL;
  if ( (size < 2) ||
       (in[0] != 0xfe) ||
       (in[1] != 0xff) ) {
    /* TODO: extend glibc with
       character set that corresponds to
       Adobe's extended ISOLATIN1 encoding! */
    return convertToUtf8(in,
			 size,
			 "CSISOLATIN1");
  } else { 
    return convertToUtf8(&in[2],
			 size - 2,
			 "UTF-16BE");
  }
}

static struct {
  char * name;
  EXTRACTOR_KeywordType type;
} tagmap[] = {
   { "/CreationDate", EXTRACTOR_CREATION_DATE},
   { "/Author" , EXTRACTOR_AUTHOR},
   { "/Description" , EXTRACTOR_DESCRIPTION},
   { "/Title" , EXTRACTOR_TITLE},
   { "/Comment", EXTRACTOR_COMMENT},
   { "/Copyright", EXTRACTOR_COPYRIGHT},
   { "/Subject", EXTRACTOR_SUBJECT},
   { "/PTEX.Fullbanner", EXTRACTOR_SOFTWARE},
   { "/Creator", EXTRACTOR_CREATOR},
   { "/ModDate", EXTRACTOR_MODIFICATION_DATE},
   { "/Producer", EXTRACTOR_PRODUCER},
   { "/Software", EXTRACTOR_SOFTWARE},
   { "/Keywords", EXTRACTOR_KEYWORDS},
   { "/Warning", EXTRACTOR_WARNING},
   { "/Signature", EXTRACTOR_RESOURCE_IDENTIFIER},
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
  size_t spos;
  size_t steps;
  size_t mlen;
  unsigned int xstart;
  unsigned int xcount;
  unsigned int xinfo;
  unsigned long long startxref;
  unsigned long long xrefpos;
  int haveValidXref;
  unsigned long long info_offset;
  char buf[MAX_STEPS+1];
  int i;
  char * meta;
  unsigned char * dmeta;
  char pcnt[20];
  float version;

  while ( (size > 0) && (IS_NL(data[size-1])) )
    size--;
  if (size < strlen(PDF_HEADER) + strlen(PDF_EOF) + strlen(PDF_SXR) + 3)
    return prev;
  if (0 != memcmp(data, PDF_HEADER, strlen(PDF_HEADER)))
    return prev;
  if (0 != memcmp(&data[size - strlen(PDF_EOF)], PDF_EOF, strlen(PDF_EOF))) 
    return prev;
  /* PDF format is pretty much sure by now */
  memcpy(buf,
	 data,
	 8);
  buf[8] = '\0';
  if (1 != sscanf(buf, "%%PDF-%f", &version)) {
    return prev;
  }
  sprintf(pcnt, "PDF %.1f", version);
  prev = addKeyword(EXTRACTOR_FORMAT,
		    strdup(pcnt),
		    prev);


  
  pos = size - strlen(PDF_EOF) - strlen(PDF_SXR);
  steps = 0;
  while ( (steps++ < MAX_STEPS) &&
	  (pos > 0) &&
	  (0 != memcmp(&data[pos], PDF_SXR, strlen(PDF_SXR))) ) 
    pos--;
  if (0 != memcmp(&data[pos], PDF_SXR, strlen(PDF_SXR))) {
    /* cross reference streams not yet supported! */
    return prev; 
  }
  memcpy(buf, &data[pos + strlen(PDF_SXR)], steps);
  buf[steps] = '\0';
  if (1 != sscanf(buf, "%llu", &startxref)) 
    return prev;
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
    while ( (pos < size) && (! IS_NL(data[pos])) )
      pos++;
    if ( (pos < size) && IS_NL(data[pos]))
      pos++;
    xrefpos = 20 * xcount + pos;    
    if ( (xrefpos >= size) || (xrefpos < pos) )
      return prev; /* invalid xref size */
    haveValidXref = 1;
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
      pos++;  }
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
  pos = info_offset;
  
  while ( (pos < size - 4) &&
	  (! ( (data[pos] == '<') &&
	       (data[pos+1] == '<') ) ) )
    pos++;
  pos++;
  if (pos >= size - 4)
    return prev;
  if ( (data[pos] == ' ') ||
       (data[pos] == 10) ||
       (data[pos] == 13) ) 
    pos++;
  
  while ( (pos < size - 2) &&
	  ( ! ( (data[pos] == '>') &&
		(data[pos+1] == '>') ) ) ) {
    i = 0;
    while (tagmap[i].name != NULL) {
      if ( (pos + strlen(tagmap[i].name) > pos) &&
	   (pos + strlen(tagmap[i].name) + 1 < size) &&
	   (0 == memcmp(&data[pos],
			tagmap[i].name,
			strlen(tagmap[i].name))) ) {
	pos += strlen(tagmap[i].name);
	if (isspace(data[pos]))
	  pos++;
	spos = pos;
	while ( (pos < size + 2) &&
		(! IS_NL(data[pos])) &&
		(data[pos] != '/') &&
		(! ( (data[pos] == '>') &&
		     (data[pos+1] == '>') ) ) )
	  pos++;	
	meta = stndup(&data[spos],
		      pos - spos);
	if (i == 0) {
	  dmeta = dateDecode(meta);
	  mlen = strlen(dmeta);
	} else {
	  dmeta = stringDecode(meta,
			       &mlen);
	}
	if (meta != NULL)
	  free(meta);
	meta = charsetDecode(dmeta, mlen);
	if (dmeta != NULL)
	  free(dmeta);
	if (meta != NULL) {
	  prev = addKeyword(tagmap[i].type,
			    meta,
			    prev);
	}
	break;
      }
      i++;
    }
    if (tagmap[i].name == NULL) {
      while ( (pos < size) &&
	      (! IS_NL(data[pos])) )
	pos++;
    }
    while ( (pos < size) &&
	    (IS_NL(data[pos])) )
      pos++;
  }
  return prev;
}

