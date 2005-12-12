/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005 Vidyut Samanta and Christian Grothoff

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

/*
 * Note that this code is not complete!
 * It will not report correct results for very long member filenames
 * (> 99 octets) when the archive was made with GNU tar or Solaris tar.
 *
 * References:
 * http://www.mkssoftware.com/docs/man4/tar.4.asp
 * (does document USTAR format common nowadays,
 *  but not other extended formats such as the one produced
 *  by GNU tar 1.13 when very long filenames are met.)
 */

static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
                                          char * keyword,
                                          EXTRACTOR_KeywordList * next) {
  EXTRACTOR_KeywordList * result = next;

  if (NULL != keyword) {
    if (0 == strlen(keyword)) {
      free(keyword);
    } else {
      result = malloc(sizeof(EXTRACTOR_KeywordList));
      if(NULL == result) {
        free(keyword);
      } else {
        result->next = next;
        result->keyword = keyword;
        result->keywordType = type;
      }
    }
  }

  return result;
}

static EXTRACTOR_KeywordList * appendKeyword(EXTRACTOR_KeywordType type,
					     char * keyword,
					     EXTRACTOR_KeywordList * last) {
  EXTRACTOR_KeywordList * result;

  if ( (last != NULL) &&
       (last->next != NULL) )
    abort();
  if (keyword == NULL)
    return last;
  if (strlen(keyword) == 0) {
    free(keyword);
    return last;
  }
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = NULL;
  result->keywordType = type;
  result->keyword = keyword;
  if (last != NULL)
    last->next = result;
  return result;
}

typedef struct {
  char name[100];
  char mode[8];
  char userId[8];
  char groupId[8];
  char filesize[12];
  char lastModTime [12];
  char chksum[8];
  char link;
  char linkName[100];
} TarHeader;

typedef struct {
  TarHeader tar;
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor [8];
  char prefix[155];
} USTarHeader;

static unsigned
taroctalvalue(const char *data,
              size_t size,
              unsigned long long *valueptr)
{
   unsigned result = 0;

   if(NULL != data && 0 < size)
   {
     const char *p = data;
     int found = 0;
     unsigned long long value = 0;

     while( (p < data + size) && (' ' == *p))
       p += 1;

     while( (p < data + size) && ('0' <= *p) && (*p < '8') )
     {
       found = 1;
       value *= 8;
       value += (*p - '0');
       p += 1;
     }

     if(0 != found)
     {
       while( (p < data + size) && (' ' == *p) )
         p += 1;

       while( (p < data + size) && (0 == *p) )
         p += 1;

       result = (p - data);
     }

     if( (0 < result) && (NULL != valueptr) )
       *valueptr = value;
   }

   return result;
}


struct EXTRACTOR_Keywords *
libextractor_tar_extract(const char * filename,
			 const char * data,
			 size_t size,
			 struct EXTRACTOR_Keywords * prev) {
  const TarHeader * tar;
  const USTarHeader * ustar;
  size_t pos;
  int contents_are_empty = 1;
  const char * mimetype = NULL;
  struct EXTRACTOR_Keywords * last;

  last = prev;
  if (last != NULL)
    while (last->next != NULL)
      last = last->next;

  if (0 != (size % 512) )
    return prev; /* cannot be tar! */
  if (size < 1024)
    return prev; /* too short, or somehow truncated */

  pos = 0;
  while (pos + sizeof(TarHeader) < size) {
    unsigned long long fsize;
    const char * nul_pos;
    const char * ustar_prefix = NULL;
    unsigned int ustar_prefix_length = 0;
    unsigned int tar_name_length;
    unsigned int zeropos;
    int header_is_empty = 1;

    if (pos + 1024 < size) {
      const int * idata = (const int*) (data + pos);
      for (zeropos = 0; zeropos < 1024 / sizeof(int); zeropos++) {
	if(0 != idata[zeropos]) {
	  header_is_empty = 0;
	  break;
	}
      }
    }

    if (header_is_empty) /* assume the EOF mark was reached */
      break;

    tar = (const TarHeader*) &data[pos];

    /*
     * checking all octal fields helps reduce
     * the possibility of false positives ;
     * only the file size is used for now.
     */
    if( (12 > taroctalvalue(tar->filesize, 12, &fsize))
     || (12 > taroctalvalue(tar->lastModTime, 12, NULL))
     || (8  > taroctalvalue(tar->mode, 8, NULL))
     || (8  > taroctalvalue(tar->userId, 8, NULL))
     || (8  > taroctalvalue(tar->groupId, 8, NULL)) )
      break;

    /* fixme: we may want to check the header checksum here... */
    /* fixme: we attempt to follow MKS document for long file names,
       but no TAR file was found yet which matched what we understood ! */
    if (pos + sizeof(USTarHeader) < size) {

      nul_pos = memchr(data + pos, 0, sizeof tar->name);
      tar_name_length = (0 == nul_pos)
         	      ? sizeof(tar->name)
                      : (nul_pos - (data + pos));

      ustar = (const USTarHeader*) &data[pos];

      if (NULL == mimetype) {
        if(0 == memcmp(ustar->magic, "ustar  ", 7))
          mimetype = "application/x-gtar";
        else
          mimetype = "application/x-tar";
      }

      if (0 == strncmp("ustar",
                       &ustar->magic[0],
                       strlen("ustar"))) {
        if(0 != *ustar->prefix) {
           nul_pos = memchr(ustar->prefix, 0, sizeof ustar->prefix);

           ustar_prefix_length = (0 == nul_pos)
                               ? sizeof ustar->prefix
                               : nul_pos - ustar->prefix;
           ustar_prefix = ustar->prefix;
        }
      }

      pos += 512; /* V7 Tar, USTar and GNU Tar usual headers take 512 octets */
    } else {
      pos += 257; /* sizeof(TarHeader); minus gcc alignment... */
    }

    if ( (pos + fsize > size) ||
	 (fsize > size) ||
	 (pos + fsize < pos) )
      break;

    if (0 < ustar_prefix_length + tar_name_length) {
      char * fname = malloc(1 + ustar_prefix_length + tar_name_length);

      if (NULL != fname) {
         if(0 < ustar_prefix_length)
           memcpy(fname, ustar_prefix, ustar_prefix_length);
         if(0 < tar_name_length)
           memcpy(fname + ustar_prefix_length, tar->name, tar_name_length);
         fname[ustar_prefix_length + tar_name_length]= '\0';
         last = appendKeyword(EXTRACTOR_FILENAME, fname, last);
         contents_are_empty = 0;
	 if (prev == NULL)
	   prev = last;
      }
    }

    if ( (fsize & 511) != 0)
      fsize = (fsize | 511)+1; /* round up! */
    if (pos + fsize < pos)
      break;
    pos += fsize;
  }

  /*
   * we only report mimetype when at least one archive member was found;
   * this should avoid most magic number ambiguities (more checks needed).
   */
  if ( (NULL != mimetype) && (0 == contents_are_empty) )
    prev = addKeyword(EXTRACTOR_MIMETYPE, strdup(mimetype), prev);

  return prev;
}
