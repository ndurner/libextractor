/*
     This file is part of libextractor.
     (C) 2002, 2003, 2009 Vidyut Samanta and Christian Grothoff

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

     This code was inspired by pdfinfo and depends heavily
     on the xpdf code that pdfinfo is a part of. See also
     the INFO file in this directory.
 */

#include "platform.h"
#include "extractor.h"
#include "convert.h"
#include <math.h>

#include <poppler/goo/gmem.h>
#include <poppler/Object.h>
#include <poppler/Stream.h>
#include <poppler/Array.h>
#include <poppler/Dict.h>
#include <poppler/XRef.h>
#include <poppler/Catalog.h>
#include <poppler/Page.h>
#include <poppler/PDFDoc.h>
#include <poppler/Error.h>
#include <poppler/GlobalParams.h>
#include <poppler/goo/GooString.h>

#define ADD(s, type) do { if (0!=proc(proc_cls, "pdf", type, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) { err = 1; goto EXIT; }} while (0)

static int 
printInfoString(Dict *infoDict,
		const char *key,
		enum EXTRACTOR_MetaType type,
		EXTRACTOR_MetaDataProcessor proc,
		void *proc_cls)
{
  Object obj;
  GooString *s1;
  const char * s;
  char *ckey = strdup (key);
  int err = 0;
  char * result;
      
  if (ckey == NULL)
    return 0;
  result = NULL;
  if (infoDict->lookup(ckey, &obj)->isString()) {
    s1 = obj.getString();
    s = s1->getCString();
    if ((((unsigned char)s[0]) & 0xff) == 0xfe &&
	(((unsigned char)s[1]) & 0xff) == 0xff) {
      result = EXTRACTOR_common_convert_to_utf8(&s[2], s1->getLength() - 2, "UTF-16BE");
      if (result != NULL)
	ADD (result, type);
    } else {
      size_t len = strlen(s);
      
      while(0 < len) 
	{
	  /*
	   * Avoid outputting trailing spaces.
	   *
	   * The following expression might be rewritten as
	   * (! isspace(s[len - 1]) && 0xA0 != s[len - 1]).
	   * There seem to exist isspace() implementations
	   * which do return non-zero from NBSP (maybe locale-dependent).
	   * Remove ISO-8859 non-breaking space (NBSP, hex value 0xA0) from
	   * the expression if it looks suspicious (locale issues for instance).
	   *
	   * Squeezing out all non-printable characters might also be useful.
	   */
  	  if ( (' '  != s[len - 1]) && (((char)0xA0) != s[len - 1]) &&
               ('\r' != s[len - 1]) && ('\n' != s[len - 1]) &&
               ('\t' != s[len - 1]) && ('\v' != s[len - 1]) &&
               ('\f' != s[len - 1]) )
	    break;	  
          else
            len --;
        }

        /* there should be a check to truncate preposterously long values. */
      
      if (0 < len) {
	result = EXTRACTOR_common_convert_to_utf8(s, len,
						  "ISO-8859-1");
	if (result != NULL)
	  ADD (result, type);
      }
    }
  }
 EXIT:
  obj.free();
  if (result != NULL)
    free (result);
  free (ckey);
  return err;
}

static int 
printInfoDate(Dict *infoDict,
	      const char *key,
	      enum EXTRACTOR_MetaType type,
	      EXTRACTOR_MetaDataProcessor proc,
	      void *proc_cls)
{
  Object obj;
  const char *s;
  GooString *s1;  
  char *gkey;
  char * result;
  int err;
  
  err = 0;
  result = NULL;
  gkey = strdup (key);
  if (gkey == NULL)
    return 0;
  if (infoDict->lookup(gkey, &obj)->isString()) {
    s1 = obj.getString();
    s = s1->getCString();
    
    if ((s1->getChar(0) & 0xff) == 0xfe &&
	(s1->getChar(1) & 0xff) == 0xff) {
      /* isUnicode */
      
      result = EXTRACTOR_common_convert_to_utf8((const char*)&s[2], s1->getLength() - 2, "UTF-16BE");
      if (result != NULL)
	ADD (result, type);
    } else {
      if (s[0] == 'D' && s[1] == ':') 
	s += 2;
      
      ADD (s, type);
    }
    /* printf(fmt, s);*/
  }
 EXIT:
  obj.free();
  if (result != NULL)
    free (result);
  free (gkey);
  return err;
}

#define PIS(s,t) do { if (0 != (err = printInfoString (info.getDict(), s, t, proc, proc_cls))) goto EXIT; } while (0)

#define PID(s,t) do { if (0 != (err = printInfoDate (info.getDict(), s, t, proc, proc_cls))) goto EXIT; } while (0)

extern "C" {
 

  int 
  EXTRACTOR_pdf_extract (const char *data,
			 size_t size,
			 EXTRACTOR_MetaDataProcessor proc,
			 void *proc_cls,
			 const char *options)
  {
    PDFDoc * doc;
    Object info;
    Object obj;
    BaseStream * stream;
    int err;

    if (globalParams == NULL)
      {
	globalParams = new GlobalParams();
	globalParams->setErrQuiet (gTrue);
      }
    obj.initNull();
    err = 0;
    stream = new MemStream( (char*) data, 0, size, &obj);
    doc = new PDFDoc(stream, NULL, NULL);
    if (! doc->isOk()) {
      delete doc;
      return 0;
    }

    ADD ("application/pdf",
	 EXTRACTOR_METATYPE_MIMETYPE);
    if ( (NULL != doc->getDocInfo(&info)) &&
	 (info.isDict()) ) {
      PIS ("Title", EXTRACTOR_METATYPE_TITLE);
      PIS ("Subject", EXTRACTOR_METATYPE_SUBJECT);
      PIS ("Keywords", EXTRACTOR_METATYPE_KEYWORDS);
      PIS ("Author", EXTRACTOR_METATYPE_AUTHOR_NAME);
      /*
       * we now believe that Adobe's Creator is not a person nor an
       * organisation, but just a piece of software.
       */
      PIS ("Creator", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);
      PIS ("Producer", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE);
      {
	char pcnt[20];
	sprintf(pcnt, "%d", doc->getNumPages());
	ADD (pcnt, EXTRACTOR_METATYPE_PAGE_COUNT);
      }
      {
	char pcnt[64];
#if HAVE_POPPLER_GETPDFMAJORVERSION
	sprintf(pcnt, "PDF %d.%d", 
		doc->getPDFMajorVersion(),
		doc->getPDFMinorVersion());
#else
	sprintf(pcnt, "PDF %.1f", 
		doc->getPDFVersion());
#endif
	ADD (pcnt, EXTRACTOR_METATYPE_FORMAT);
      }
      PID ("CreationDate", EXTRACTOR_METATYPE_CREATION_DATE);
      PID ("ModDate", EXTRACTOR_METATYPE_MODIFICATION_DATE);
    }
  EXIT:
    info.free();
    delete doc;

    return err;
  }
}

