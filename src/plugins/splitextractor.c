/*
     This file is part of libextractor.
     (C) 2002, 2003, 2005, 2006 Vidyut Samanta and Christian Grothoff

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

/**
 * Default split characters.
 */
static const char * TOKENIZERS = "._ ,%@-\n_[](){}";

/**
 * Do not use keywords shorter than this minimum
 * length.
 */
static int MINIMUM_KEYWORD_LENGTH = 4;

static void addKeyword(struct EXTRACTOR_Keywords ** list,
		       const char * keyword) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = strdup(keyword);
  next->keywordType = EXTRACTOR_SPLIT;
  *list = next;
}

static int token(char letter,
		 const char * options) {
  size_t i;

  i = 0;
  while (options[i] != '\0') {
    if (letter == options[i])
      return 1;
    i++;
  }
  return 0;
}

static void splitKeywords(const char * keyword,
			  struct EXTRACTOR_Keywords ** list,
			  const char * options) {
  char * dp;
  size_t pos;
  size_t last;
  size_t len;

  dp = strdup(keyword);
  len = strlen(dp);
  pos = 0;
  last = 0;
  while (pos < len) {
    while ( (0 == token(dp[pos], options)) &&
	    (pos < len) )
      pos++;
    dp[pos++] = '\0';
    if (pos - last > MINIMUM_KEYWORD_LENGTH) 
      addKeyword(list, 
		 &dp[last]);    
    while ( (1 == token(dp[pos], options)) &&
	    (pos < len) )
      pos++;
    last = pos;
  }
  free(dp);
}

/* split other keywords into multiple keywords */
struct EXTRACTOR_Keywords *
libextractor_split_extract(const char * filename,
			   const char * data,
			   size_t size,
			   struct EXTRACTOR_Keywords * prev,
			   const char * options) {
  struct EXTRACTOR_Keywords * pos;

  if (options == NULL)
    options = TOKENIZERS;
  pos = prev;
  while (pos != NULL) {
    splitKeywords(pos->keyword,		  
		  &prev,
		  options);
    pos = pos->next;
  }
  return prev;
}

/* end of splitextractor.c */
