/*
     This file is part of libextractor.
     (C) 2004, 2009 Vidyut Samanta and Christian Grothoff

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
#include <ctype.h>
#include "extractor.h"
#include "zlib.h"
#include "unzip.h"

#define CASESENSITIVITY (0)
#define MAXFILENAME (256)


/**
 * Name of the file with the meta-data in OO documents.
 */
#define METAFILE "meta.xml"

typedef struct {
  const char * text;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tmap[] = {
  { "meta:generator",     EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  { "meta:page-count",    EXTRACTOR_METATYPE_PAGE_COUNT },
  { "meta:creation-date", EXTRACTOR_METATYPE_CREATION_DATE },
  { "dc:date",            EXTRACTOR_METATYPE_UNKNOWN_DATE },
  { "dc:creator",         EXTRACTOR_METATYPE_CREATOR },
  { "dc:language",        EXTRACTOR_METATYPE_LANGUAGE },
  { "dc:title",           EXTRACTOR_METATYPE_TITLE },
  { "dc:description",     EXTRACTOR_METATYPE_DESCRIPTION },
  { "dc:subject",         EXTRACTOR_METATYPE_SUBJECT },
  { "meta:keyword",       EXTRACTOR_METATYPE_KEYWORDS },
  { "meta:user-defined meta:name=\"Info 1\"", EXTRACTOR_METATYPE_COMMENT },
  { "meta:user-defined meta:name=\"Info 2\"", EXTRACTOR_METATYPE_COMMENT },
  { "meta:user-defined meta:name=\"Info 3\"", EXTRACTOR_METATYPE_COMMENT },
  { "meta:user-defined meta:name=\"Info 4\"", EXTRACTOR_METATYPE_COMMENT },
  { NULL, 0 },
};


/**
 * returns either zero when mimetype info is missing
 * or an already malloc'ed string containing the mimetype info.
 */
static char *
libextractor_oo_getmimetype(EXTRACTOR_unzip_file uf) {
  char filename_inzip[MAXFILENAME];
  EXTRACTOR_unzip_file_info file_info;
  char * buf = NULL;
  size_t buf_size = 0;

  if (EXTRACTOR_UNZIP_OK != EXTRACTOR_common_unzip_local_file(uf,
							      "mimetype",
							      CASESENSITIVITY))
    return NULL;
  if ( (EXTRACTOR_UNZIP_OK == EXTRACTOR_common_unzip_get_current_file_info(uf,
									   &file_info,
									   filename_inzip,
									   sizeof(filename_inzip),
									   NULL,
									   0,
									   NULL,
									   0) &&
	(EXTRACTOR_UNZIP_OK == EXTRACTOR_common_unzip_open_current_file3(uf, NULL, NULL, 0)) ) ) {
    buf_size = file_info.uncompressed_size;
    
    if (buf_size > 1024) 
      {
	/* way too large! */
      }
    else if (NULL == (buf = malloc(1 + buf_size))) 
      {
	/* memory exhausted! */
      } 
    else if (buf_size != (size_t) EXTRACTOR_common_unzip_read_current_file(uf,buf,buf_size)) 
      {
	free(buf);
	buf = NULL;
      }
    else 
      {
	/* found something */
	buf[buf_size] = '\0';
	while ( (0 < buf_size) &&
		isspace( (unsigned char) buf[buf_size - 1]))
	  buf[--buf_size] = '\0';
	if ('\0' == buf[0]) 
	  {
	    free(buf);
	    buf = NULL;
	  }
      }
  }
  EXTRACTOR_common_unzip_close_current_file(uf);  
  return buf;
}


typedef struct Ecls {
  char * data;
  size_t size;
  size_t pos;
} Ecls;


int 
EXTRACTOR_odf_extract (const char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  char filename_inzip[MAXFILENAME];
  EXTRACTOR_unzip_file uf;
  EXTRACTOR_unzip_file_info file_info;
  char * buf;
  char * pbuf;
  size_t buf_size;
  int i;
  EXTRACTOR_unzip_filefunc_def io;
  Ecls cls;
  char * mimetype;

  if (size < 100)
    return 0;
  if ( !( ('P'==data[0]) && ('K'==data[1]) && (0x03==data[2]) && (0x04==data[3])) )
    return 0;

  cls.data = (void*) data;
  cls.size = size;
  cls.pos = 0;
  io.zopen_file = &EXTRACTOR_common_unzip_zlib_open_file_func;
  io.zread_file = &EXTRACTOR_common_unzip_zlib_read_file_func;
  io.zwrite_file = NULL;
  io.ztell_file = &EXTRACTOR_common_unzip_zlib_tell_file_func;
  io.zseek_file = &EXTRACTOR_common_unzip_zlib_seek_file_func;
  io.zclose_file = &EXTRACTOR_common_unzip_zlib_close_file_func;
  io.zerror_file = &EXTRACTOR_common_unzip_zlib_testerror_file_func;
  io.opaque = &cls;

  uf = EXTRACTOR_common_unzip_open2("ERROR", &io);
  if (uf == NULL)
    return 0;
  mimetype = libextractor_oo_getmimetype(uf);
  if ( (NULL != mimetype) &&
       (0 != proc (proc_cls, 
		   "deb",
		   EXTRACTOR_METATYPE_MIMETYPE,
		   EXTRACTOR_METAFORMAT_UTF8,
		   "text/plain",
		   mimetype,
		   strlen (mimetype)+1)) )
    {
      EXTRACTOR_common_unzip_close(uf);
      free (mimetype);
      return 1;
    }
  free (mimetype);
  if (EXTRACTOR_common_unzip_local_file(uf,
		    METAFILE,
		    CASESENSITIVITY) != EXTRACTOR_UNZIP_OK) {
    EXTRACTOR_common_unzip_close(uf);
    return 0; /* not found */
  }

  if (EXTRACTOR_UNZIP_OK != 
      EXTRACTOR_common_unzip_get_current_file_info(uf,
						   &file_info,
						   filename_inzip,
						   sizeof(filename_inzip),
						   NULL,0,NULL,0)) {
    EXTRACTOR_common_unzip_close(uf);
    return 0; /* problems... */
  }

  if (EXTRACTOR_UNZIP_OK != EXTRACTOR_common_unzip_open_current_file3(uf, NULL, NULL, 0)) {
    EXTRACTOR_common_unzip_close(uf);
    return 0; /* problems... */
  }

  buf_size = file_info.uncompressed_size;
  if (buf_size > 128 * 1024) {
    EXTRACTOR_common_unzip_close_current_file(uf);
    EXTRACTOR_common_unzip_close(uf);
    return 0; /* hardly meta-data! */
  }
  buf = malloc(buf_size+1);
  if (buf == NULL) 
    {
      EXTRACTOR_common_unzip_close_current_file(uf);
      EXTRACTOR_common_unzip_close(uf);
      return 0; /* out of memory */
    }

  if (buf_size != EXTRACTOR_common_unzip_read_current_file(uf,buf,buf_size)) 
    {
      free(buf);
      EXTRACTOR_common_unzip_close_current_file(uf);
      EXTRACTOR_common_unzip_close(uf);
      return 0;
    }
  EXTRACTOR_common_unzip_close_current_file(uf);
  /* we don't do "proper" parsing of the meta-data but rather use some heuristics
     to get values out that we understand */
  buf[buf_size] = '\0';
  /* printf("%s\n", buf); */
  /* try to find some of the typical OO xml headers */
  if ( (strstr(buf, "xmlns:meta=\"http://openoffice.org/2000/meta\"") != NULL) ||
       (strstr(buf, "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"") != NULL) ||
       (strstr(buf, "xmlns:xlink=\"http://www.w3.org/1999/xlink\"") != NULL) ) {
    /* accept as meta-data */
    i = -1;
    while (tmap[++i].text != NULL) {
      char * spos;
      char * epos;
      char needle[256];
      int oc;

      pbuf = buf;

      while (1) {
	strcpy(needle, "<");
	strcat(needle, tmap[i].text);
	strcat(needle, ">");
	spos = strstr(pbuf, needle);
	if (NULL == spos) {
	strcpy(needle, tmap[i].text);
	strcat(needle, "=\"");
	spos = strstr(pbuf, needle);
	if (spos == NULL)
	  break;
	spos += strlen(needle);
	epos = spos;
	while ( (epos[0] != '\0') &&
		(epos[0] != '"') )
	  epos++;
	} else {
	  oc = 0;
	  spos += strlen(needle);
	  while ( (spos[0] != '\0') &&
		  ( (spos[0] == '<') ||
		    (oc > 0) ) ) {
	    if (spos[0] == '<')
	      oc++;
	    if (spos[0] == '>')
	      oc--;
	    spos++;
	  }
	  epos = spos;
	  while ( (epos[0] != '\0') &&
		  (epos[0] != '<') &&
		  (epos[0] != '>') ) {
	    epos++;
	  }
	}
	if (spos != epos) 
	  {
	    char key[epos - spos + 1];

	    memcpy(key, spos, epos-spos);
	    key[epos-spos] = '\0';
	    if (0 != proc (proc_cls, 
			   "odf",
			   tmap[i].type,
			   EXTRACTOR_METAFORMAT_UTF8,
			   "text/plain",
			   key,
			   epos - spos + 1))
	      {
		free(buf);
		EXTRACTOR_common_unzip_close(uf);
		return 1;	      
	      }	  
	    pbuf = epos;
	  } 
	else
	  break;
      }
    }
  }
  free(buf);
  EXTRACTOR_common_unzip_close(uf);
  return 0;
}


