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

static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
					  char * keyword,
					  EXTRACTOR_KeywordList * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}

struct EXTRACTOR_Keywords * libextractor_TEMPLATE_extract(const char * filename,
							  char * data,
							  size_t size,
							  struct EXTRACTOR_Keywords * prev) {

  return prev;
}

