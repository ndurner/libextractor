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
#include <string.h>
#include "convert.h"

static struct {
  char * name;
  EXTRACTOR_KeywordType type;
} tagmap[] = {
  { "author" ,         EXTRACTOR_AUTHOR},
  { "title" ,          EXTRACTOR_TITLE},
  { "description" ,    EXTRACTOR_DESCRIPTION},
  { "language",        EXTRACTOR_LANGUAGE},
  { "rights",          EXTRACTOR_COPYRIGHT},
  { "publisher",       EXTRACTOR_PUBLISHER},
  { "formatter",       EXTRACTOR_SOFTWARE},
  { "copyright",       EXTRACTOR_COPYRIGHT},
  { "abstract",        EXTRACTOR_SUMMARY},
  { "subject",         EXTRACTOR_SUBJECT},
  { "abstract",        EXTRACTOR_SUMMARY},
  { "date",            EXTRACTOR_DATE},
  { "keywords",        EXTRACTOR_KEYWORDS},
  { "dc.author" ,      EXTRACTOR_AUTHOR},
  { "dc.title" ,       EXTRACTOR_TITLE},
  { "dc.description" , EXTRACTOR_DESCRIPTION},
  { "dc.subject",      EXTRACTOR_SUBJECT},
  { "dc.creator",      EXTRACTOR_CREATOR},
  { "dc.publisher",    EXTRACTOR_PUBLISHER},
  { "dc.date",         EXTRACTOR_DATE},
  { "dc.format",       EXTRACTOR_FORMAT},
  { "dc.identifier",   EXTRACTOR_RESOURCE_IDENTIFIER},
  { "dc.rights",       EXTRACTOR_COPYRIGHT},
  {NULL, EXTRACTOR_UNKNOWN},
};

static char * relevantTags[] = {
  "title",
  "meta",
  NULL,
};

/* which mime-types should not be subjected to
   the HTML extractor (no use trying & parsing
   is expensive!) */
static char * blacklist[] = {
  "image/jpeg",
  "image/gif",
  "image/png",
  "image/x-png",
  "image/xcf",
  "image/tiff",
  "application/java",
  "application/pdf",
  "application/postscript",
  "application/elf",
  "application/gnunet-directory",
  "application/x-gzip",
  "application/bz2",
  "application/x-rpm",
  "application/x-rar",
  "application/x-zip",
  "application/x-arj",
  "application/x-compress",
  "application/x-tar",
  "application/x-lha",
  "application/x-gtar",
  "application/x-dpkg",
  "application/ogg",
  "audio/real",
  "audio/x-wav",
  "audio/avi",
  "audio/midi",
  "audio/mpeg",
  "video/real",
  "video/asf",
  "video/quicktime",
  NULL,
};

typedef struct TI {
  struct TI * next;
  const char * tagStart;
  const char * tagEnd;
  const char * dataStart;
  const char * dataEnd;
} TagInfo;

/**
 * Add a keyword.
 */
static struct EXTRACTOR_Keywords * 
addKeyword(EXTRACTOR_KeywordType type,
	   char * keyword,
	   struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}

/* ******************** parser helper functions ************** */

static int lookFor(char c, 
		   size_t * pos, 
		   const char * data,
		   size_t size) {
  size_t p = *pos;

  if (data[p] == '\0') return 0;
  while ( (p < size) &&
	  (data[p] != c) ) {
    p++;
    if (data[p] == '\0') return 0;
  }
  *pos = p;
  return p < size;
}

static int skipWhitespace(size_t * pos, 
			  const char * data,
			  size_t size) {
  size_t p = *pos;

  if (data[p] == '\0') return 0;
  while ( (p < size) &&
	  (isspace(data[p])) ) {
    p++;
    if (data[p] == '\0') return 0;
  }
  *pos = p;
  return p < size;
}

static int skipLetters(size_t * pos, 
		       const char * data,
		       size_t size) {
  size_t p = *pos;
  
  if (data[p] == '\0') return 0;
  while ( (p < size) &&
	  (isalpha(data[p])) ) {
    p++;
    if (data[p] == '\0') return 0;
  }
  *pos = p;
  return p < size;
}

static int lookForMultiple(const char * c, 
			   size_t * pos, 
			   const char * data,
			   size_t size) {
  size_t p = *pos;

  if (data[p] == '\0') return 0;
  while ( (p < size) &&
	  (strchr(c, data[p]) == NULL) ) {
    p++;
    if (data[p] == '\0') return 0;
  }
  *pos = p;
  return p < size;
}

/**
 * Search all tags that correspond to "tagname".  Example:
 * If the tag is <meta name="foo" desc="bar">, and
 * tagname == "meta", keyname="name", keyvalue="foo",
 * and searchname="desc", then this function returns a 
 * copy (!) of "bar".  Easy enough?
 *
 * @return NULL if nothing is found
 */
static char * findInTags(TagInfo * t,
			 const char * tagname,
			 const char * keyname,
			 const char * keyvalue,
			 const char * searchname) {
  return NULL;
}


/* mimetype = text/html */
struct EXTRACTOR_Keywords * 
libextractor_html_extract(const char * filename,
			  const char * data,
			  const size_t size,
			  struct EXTRACTOR_Keywords * prev) {
  size_t xsize;
  const char * mime;
  TagInfo * tags;
  TagInfo * t;
  TagInfo tag;
  size_t pos;
  size_t tpos;
  int i;
  char * charset;
  char * tmp;

  if (size == 0)
    return prev;

  mime = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE,
			       prev);
  if (mime != NULL) {
    int j;
    j = 0;
    while (blacklist[j] != NULL) {
      if (0 == strcmp(blacklist[j], mime))
	return prev;
      j++;
    }
  }

  /* only scan first 32k */
  if (size > 1024 * 32)
    xsize = 1024 * 32;
  else
    xsize = size;
  tags = NULL;
  tag.next = NULL;
  pos = 0;
  while (pos < xsize) {
    if (! lookFor('<', &pos, data, size)) break;
    tag.tagStart = &data[++pos];
    if (! skipLetters(&pos, data, size)) break;
    tag.tagEnd = &data[pos];
    if (! skipWhitespace(&pos, data, size)) break;
  STEP3:
    if (! lookForMultiple(">\"\'", &pos, data, size)) break;
    if (data[pos] != '>') {      
      /* find end-quote, ignore escaped quotes (\') */
      do {
	tpos = pos;
	pos++;
	if (! lookFor(data[tpos], &pos, data, size)) 
	  break;
      } while (data[pos-1] == '\\');
      pos++;
      goto STEP3;
    }
    pos++;
    if (! skipWhitespace(&pos, data, size)) break;   
    tag.dataStart = &data[pos];
    if (! lookFor('<', &pos, data, size)) break;
    tag.dataEnd = &data[pos];
    i = 0;
    while (relevantTags[i] != NULL) {
      if ( (strlen(relevantTags[i]) == tag.tagEnd - tag.tagStart) &&
	   (0 == strncasecmp(relevantTags[i],
			     tag.tagStart,
			     tag.tagEnd - tag.tagStart)) ) {
	t = malloc(sizeof(TagInfo));
	*t = tag;
	t->next = tags;
	tags = t;
	break;
      }
      i++;
    } 
    /* abort early if we hit the body tag */
    if ( (tag.tagEnd - tag.tagStart == strlen("body")) &&
	 0 == strncasecmp("body",
			  tag.tagStart,
			  tag.tagEnd - tag.tagStart))
      break; 
  }

  /* fast exit */
  if (tags == NULL)
    return prev;

  charset = NULL;

  /* first, try to determine mime type and/or character set */
  tmp = findInTags(tags,
		   "meta", 
		   "http-equiv", "content-type",
		   "content");
  if (tmp != NULL) {
    /* ideally, tmp == "test/html; charset=ISO-XXXX-Y" or something like that;
       if text/html is present, we take that as the mime-type; if charset=
       is present, we try to use that for character set conversion. */
    if (0 == strncmp(tmp,
		     "text/html",
		     strlen("text/html"))) 
      prev = addKeyword(EXTRACTOR_MIMETYPE,
			strdup("text/html"),
			prev);
    
    charset = strstr(tmp, "charset=");
    free(tmp);
  }
  if (charset != NULL)
    charset = strdup(&charset[strlen("charset=")]);
  else
    charset = strdup("ISO-8859-1"); /* try a sensible default */
  
  
  i = 0;
  while (tagmap[i].name != NULL) {
    tmp = findInTags(tags,
		     "meta",
		     "name", tagmap[i].name,
		     "content");
    if (tmp != NULL) {
      prev = addKeyword(tagmap[i].type,
			convertToUtf8(tmp,
				      strlen(tmp),
				      charset),
			prev);    
      free(tmp);
    }
    i++;
  }

  
  while (tags != NULL) {
    t = tags;
    if ( (t->tagEnd - t->tagStart == strlen("title")) &&
	 0 == strncasecmp("title",
			  t->tagStart,
			  t->tagEnd - t->tagStart))
      prev = addKeyword(EXTRACTOR_TITLE,
			convertToUtf8(t->dataStart,
				      t->dataEnd - t->dataStart,
				      charset),
			prev);    
    tags = t->next;
    free(t);
  }
  free(charset);

  return prev;
}

