/*
     This file is part of libextractor.
     (C) 2004 Vidyut Samanta and Christian Grothoff

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

#include "md5.h"

static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordList *oldhead, 
					      const char *phrase,
					      EXTRACTOR_KeywordType type) {

   EXTRACTOR_KeywordList * keyword;   
   keyword = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
   keyword->next = oldhead;    
   keyword->keyword = strdup(phrase);
   keyword->keywordType = type;
   return keyword;
}



#define DIGEST_BITS 128
#define DIGEST_HEX_BYTES (DIGEST_BITS / 4)
#define DIGEST_BIN_BYTES (DIGEST_BITS / 8)
#define MAX_DIGEST_BIN_BYTES DIGEST_BIN_BYTES 

struct EXTRACTOR_Keywords * libextractor_hash_md5_extract(const char * filename,
							  char * data,
							  size_t size,
							  struct EXTRACTOR_Keywords * prev) {
  unsigned char bin_buffer[MAX_DIGEST_BIN_BYTES];
  char hash[8 * MAX_DIGEST_BIN_BYTES];
  char buf[16];
  int i;

  md5_buffer(data, size, bin_buffer);  
  hash[0] = '\0';
  for (i=0;i<DIGEST_HEX_BYTES / 2; i++) {
    snprintf(buf,
	     16,
	     "%02x",
	     bin_buffer[i]);
    strcat(hash, buf);
  }
  prev = addKeyword(prev,
		    hash,
		    EXTRACTOR_HASH_MD5);
  
  return prev;
}
