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
#include <zlib.h>

/*
 * Note that this code is not complete!
 *
 * References:
 * http://www.mkssoftware.com/docs/man4/tar.4.asp
 */


static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
					  char * keyword,
					  EXTRACTOR_KeywordList * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  if (strlen(keyword) == 0) {
    free(keyword);
    return next;
  }
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}

static char * stndup(const char * str,
                     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
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


static struct EXTRACTOR_Keywords *
tar_extract(const char * data,
	    size_t size,
	    struct EXTRACTOR_Keywords * prev) {
  TarHeader * tar;
  USTarHeader * ustar;
  size_t pos;

  if (0 != (size % 512) )
    return prev; /* cannot be tar! */
  if (size < 1024)
    return prev;
  size -= 1024; /* last 2 blocks are all zeros */
  /* fixme: we may want to check that the last
     1024 bytes are all zeros here... */

  pos = 0;
  while (pos + sizeof(TarHeader) < size) {
    unsigned long long fsize;
    char buf[13];

    tar = (TarHeader*) &data[pos];
    /* fixme: we may want to check the header checksum here... */
    if (pos + sizeof(USTarHeader) < size) {
      ustar = (USTarHeader*) &data[pos];
      if (0 == strncmp("ustar",
		       &ustar->magic[0],
		       strlen("ustar")))
	pos += 512; /* sizeof(USTarHeader); */
      else
	pos += 257; /* sizeof(TarHeader); minus gcc alignment... */
    } else {
      pos += 257; /* sizeof(TarHeader); minus gcc alignment... */
    }
    memcpy(buf, &tar->filesize[0], 12);
    buf[12] = '\0';
    if (1 != sscanf(buf, "%12llo", &fsize)) /* octal! Yuck yuck! */
      return prev;
    if ( (pos + fsize > size) ||
	 (fsize > size) ||
	 (pos + fsize < pos) )
      return prev;
    prev = addKeyword(EXTRACTOR_FILENAME,
		      stndup(&tar->name[0],
			     100),
		      prev);
    if ( (fsize & 511) != 0)
      fsize = (fsize |= 511)+1; /* round up! */
    if (pos + fsize < pos)
      return prev;
    pos += fsize;
  }
  return prev;
}

/* do not decompress tar.gz files > 16 MB */
#define MAX_TGZ_SIZE 16 * 1024 * 1024

struct EXTRACTOR_Keywords * libextractor_tar_extract(const char * filename,
						     const unsigned char * data,
						     size_t size,
						     struct EXTRACTOR_Keywords * prev) {
  if ( (data[0] == 0x1f) &&
       (data[1] == 0x8b) &&
       (data[2] == 0x08) ) {
    time_t ctime;
    char * buf;
    size_t bufSize;
    gzFile gzf;

    /* Creation time */
    ctime = ((((((  (unsigned int)data[7] << 8)
                  | (unsigned int)data[6]) << 8)
                  | (unsigned int)data[5]) << 8)
                  | (unsigned int)data[4]);
    if (ctime) {
      struct tm ctm;
      char tmbuf[60];

      ctm = *gmtime(&ctime);
      if (strftime(tmbuf, sizeof(tmbuf),
       nl_langinfo(D_FMT),
       &ctm))
        prev = addKeyword(EXTRACTOR_CREATION_DATE, strdup(tmbuf), prev);
    }

    /* try for tar.gz */
    bufSize = data[size-4] + 256 * data[size-3] + 65536 * data[size-2] + 256*65536 * data[size-1];
    if (bufSize > MAX_TGZ_SIZE) {
      return prev;
    }
    gzf = gzopen(filename, "rb");
    if (gzf == NULL) {
      return prev;
    }
    buf = malloc(bufSize);
    if (buf == NULL) {
      gzclose(gzf);
      return prev;
    }
    if (bufSize != gzread(gzf, buf, bufSize)) {
      free(buf);
      gzclose(gzf);
      return prev;
    }
    gzclose(gzf);
    prev = tar_extract(buf, bufSize, prev);
    free(buf);
    return prev;
  } else {
    /* try for uncompressed tar */
    return tar_extract(data, size, prev);
  }
}

