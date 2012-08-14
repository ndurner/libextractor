/*
     This file is part of libextractor.
     (C) 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/odf_extractor.c
 * @brief plugin to support ODF files
 * @author Christian Grothoff
 */
#include "platform.h"
#include <ctype.h>
#include "extractor.h"
#include "unzip.h"

/**
 * Maximum length of a filename allowed inside the ZIP archive.
 */
#define MAXFILENAME 256

/**
 * Name of the file with the meta-data in OO documents.
 */
#define METAFILE "meta.xml"


/**
 * Mapping from ODF meta data strings to LE types.
 */
struct Matches 
{
  /**
   * ODF description.
   */
  const char * text;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * NULL-terminated map from ODF meta data strings to LE types.
 */
static struct Matches tmap[] = {
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
  { NULL, 0 }
};


/**
 * Obtain the mimetype of the archive by reading the 'mimetype'
 * file of the ZIP.
 *
 * @param uf unzip context to extract the mimetype from
 * @return NULL if no mimetype could be found, otherwise the mime type
 */
static char *
libextractor_oo_getmimetype (struct EXTRACTOR_UnzipFile * uf) 
{
  char filename_inzip[MAXFILENAME];
  struct EXTRACTOR_UnzipFileInfo file_info;
  char *buf;
  size_t buf_size;

  if (EXTRACTOR_UNZIP_OK !=
      EXTRACTOR_common_unzip_go_find_local_file (uf,
						 "mimetype",
						 2))
    return NULL;
  if (EXTRACTOR_UNZIP_OK != 
      EXTRACTOR_common_unzip_get_current_file_info (uf,
						    &file_info,
						    filename_inzip,
						    sizeof (filename_inzip),
						    NULL,
						    0,
						    NULL,
						    0))
    return NULL;
  if (EXTRACTOR_UNZIP_OK != 
      EXTRACTOR_common_unzip_open_current_file (uf))
    return NULL;
  buf_size = file_info.uncompressed_size;    
  if (buf_size > 1024) 
    {
      /* way too large! */
      EXTRACTOR_common_unzip_close_current_file (uf);  
      return NULL;
    }
  if (NULL == (buf = malloc (1 + buf_size))) 
    {
      /* memory exhausted! */
      EXTRACTOR_common_unzip_close_current_file (uf);  
      return NULL;
    } 
  if (buf_size != 
      (size_t) EXTRACTOR_common_unzip_read_current_file (uf, 
							 buf, 
							 buf_size)) 
    {
      free(buf);
      EXTRACTOR_common_unzip_close_current_file(uf);  
      return NULL;
    }
  /* found something */
  buf[buf_size] = '\0';
  while ( (0 < buf_size) &&
	  isspace( (unsigned char) buf[buf_size - 1]))
    buf[--buf_size] = '\0';
  if ('\0' == buf[0]) 
    {
      free (buf);
      buf = NULL;
    }
  EXTRACTOR_common_unzip_close_current_file (uf);
  return buf;
}


/**
 * Main entry method for the ODF extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_odf_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  char filename_inzip[MAXFILENAME];
  struct EXTRACTOR_UnzipFile *uf;
  struct EXTRACTOR_UnzipFileInfo file_info;
  char *buf;
  char *pbuf;
  size_t buf_size;
  unsigned int i;
  char *mimetype;

  if (NULL == (uf = EXTRACTOR_common_unzip_open (ec)))
    return;
  if (NULL != (mimetype = libextractor_oo_getmimetype (uf)))
    {
      if (0 != ec->proc (ec->cls, 
			 "odf",
			 EXTRACTOR_METATYPE_MIMETYPE,
			 EXTRACTOR_METAFORMAT_UTF8,
			 "text/plain",
			 mimetype,
			 strlen (mimetype) + 1))
	{
	  EXTRACTOR_common_unzip_close (uf);
	  free (mimetype);
	  return;
	}
      free (mimetype);
    }
  if (EXTRACTOR_UNZIP_OK !=
      EXTRACTOR_common_unzip_go_find_local_file (uf,
						 METAFILE,
						 2)) 
    {
      /* metafile not found */
      EXTRACTOR_common_unzip_close (uf);
      return; 
    }
  if (EXTRACTOR_UNZIP_OK != 
      EXTRACTOR_common_unzip_get_current_file_info (uf,
						    &file_info,
						    filename_inzip,
						    sizeof (filename_inzip),
						    NULL, 0, NULL, 0)) 
    {
      /* problems accessing metafile */
      EXTRACTOR_common_unzip_close (uf);
      return;
    }
  if (EXTRACTOR_UNZIP_OK != 
      EXTRACTOR_common_unzip_open_current_file (uf)) 
    {
      /* problems with unzip */
      EXTRACTOR_common_unzip_close (uf);
      return; 
    }

  buf_size = file_info.uncompressed_size;
  if (buf_size > 128 * 1024) 
    {
      /* too big to be meta-data! */
      EXTRACTOR_common_unzip_close_current_file (uf);
      EXTRACTOR_common_unzip_close (uf);
      return; 
    }
  if (NULL == (buf = malloc (buf_size+1)))
    {
      /* out of memory */
      EXTRACTOR_common_unzip_close_current_file (uf);
      EXTRACTOR_common_unzip_close (uf);
      return;
    }
  if (buf_size != EXTRACTOR_common_unzip_read_current_file (uf, buf, buf_size)) 
    {
      EXTRACTOR_common_unzip_close_current_file (uf);
      goto CLEANUP;
    }
  EXTRACTOR_common_unzip_close_current_file (uf);
  /* we don't do "proper" parsing of the meta-data but rather use some heuristics
     to get values out that we understand */
  buf[buf_size] = '\0';
  /* printf("%s\n", buf); */
  /* try to find some of the typical OO xml headers */
  if ( (strstr (buf, "xmlns:meta=\"http://openoffice.org/2000/meta\"") != NULL) ||
       (strstr (buf, "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"") != NULL) ||
       (strstr (buf, "xmlns:xlink=\"http://www.w3.org/1999/xlink\"") != NULL) ) 
    {
      /* accept as meta-data */
      for (i = 0; NULL  != tmap[i].text; i++)
	{
	  char * spos;
	  char * epos;
	  char needle[256];
	  int oc;
	  
	  pbuf = buf;
	  
	  while (1) 
	    {
	      strcpy(needle, "<");
	      strcat(needle, tmap[i].text);
	      strcat(needle, ">");
	      spos = strstr(pbuf, needle);
	      if (NULL == spos) 
		{
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
		} 
	      else
		{
		  oc = 0;
		  spos += strlen(needle);
		  while ( (spos[0] != '\0') &&
			  ( (spos[0] == '<') ||
			    (oc > 0) ) ) 
		    {
		      if (spos[0] == '<')
			oc++;
		      if (spos[0] == '>')
			oc--;
		      spos++;
		    }
		  epos = spos;
		  while ( (epos[0] != '\0') &&
			  (epos[0] != '<') &&
			  (epos[0] != '>') ) 
		    {
		      epos++;
		    }
		}
	      if (spos != epos) 
		{
		  char key[epos - spos + 1];
		  
		  memcpy(key, spos, epos-spos);
		  key[epos-spos] = '\0';
		  if (0 != ec->proc (ec->cls, 
				     "odf",
				     tmap[i].type,
				     EXTRACTOR_METAFORMAT_UTF8,
				     "text/plain",
				     key,
				     epos - spos + 1))
		    goto CLEANUP;		    	  
		  pbuf = epos;
		} 
	      else
		break;
	    }
	}
    }
 CLEANUP:
  free (buf);
  EXTRACTOR_common_unzip_close (uf);
}

/* end of odf_extractor.c */
