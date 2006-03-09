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

     This code depends heavily on the wordleaker code and
     a lot of code was borrowed from wordleaker.cpp. See also
     the README file in this directory.
 */

#include "platform.h"
#include "extractor.h"
#include "../convert.h"
#include <math.h>

#include "wordleaker.h"
#include "pole.h"



#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <list>
#include <ctime>


extern "C" {

  static EXTRACTOR_KeywordType 
  SummaryProperties[] = {
    EXTRACTOR_UNKNOWN,
    EXTRACTOR_UNKNOWN,
    EXTRACTOR_TITLE,
    EXTRACTOR_SUBJECT,
    EXTRACTOR_AUTHOR,
    EXTRACTOR_KEYWORDS,
    EXTRACTOR_COMMENT,
    EXTRACTOR_TEMPLATE,
    EXTRACTOR_LAST_SAVED_BY,
    EXTRACTOR_VERSIONNUMBER,
    EXTRACTOR_TOTAL_EDITING_TIME,
    EXTRACTOR_LAST_PRINTED,
    EXTRACTOR_CREATION_DATE,
    EXTRACTOR_MODIFICATION_DATE,
    EXTRACTOR_PAGE_COUNT,
    EXTRACTOR_WORD_COUNT,
    EXTRACTOR_CHARACTER_COUNT,
    EXTRACTOR_THUMBNAILS,
    EXTRACTOR_SOFTWARE,
    EXTRACTOR_SECURITY,
  };

  static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
						const char * keyword,
						struct EXTRACTOR_Keywords * next) {
    EXTRACTOR_KeywordList * result;

    if (keyword == NULL)
      return next;
    result = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
    result->next = next;
    result->keyword = strdup(keyword);
    result->keywordType = type;
    return result;
  }

  static char * dateToString( unsigned long date ) {
    char f[16];
    sprintf(f, "%d/%d/%d", (date / 10000 % 100), (date / 100 % 100), (date % 100));
    return strdup(f);
  }
  
  static const char * idToProduct( unsigned int id ) {
    // TODO: find the rest of ids
    switch ( id ) {
    case  0x6A62:
      return "Word 97";
    case 0x626A:
      return "Word 98 (Mac)";
    default:
      return "Unknown";
    }      
  }

  static const char * lidToLanguage( unsigned int lid ) {
    switch ( lid ) {
    case 0x0400: 
      return _("No Proofing");
    case 0x0401: 
      return _("Arabic");
    case 0x0402:
      return _("Bulgarian");
    case 0x0403:
      return _("Catalan");
    case 0x0404:
      return _("Traditional Chinese");
    case 0x0804:
      return _("Simplified Chinese");
    case 0x0405:
      return _("Czech");
    case 0x0406:
      return _("Danish");
    case 0x0407:
      return _("German");
    case 0x0807:
      return _("Swiss German");
    case 0x0408:
      return _("Greek");
    case 0x0409:
      return _("U.S. English");
    case 0x0809:
      return _("U.K. English");
    case 0x0c09:
      return _("Australian English");
    case 0x040a:
      return _("Castilian Spanish");
    case 0x080a:
      return _("Mexican Spanish");
    case 0x040b:
      return _("Finnish");
    case 0x040c:
      return _("French");
    case 0x080c:
      return _("Belgian French");
    case 0x0c0c:
      return _("Canadian French");
    case 0x100c:
      return _("Swiss French");
    case 0x040d:
      return _("Hebrew");
    case 0x040e:
      return _("Hungarian");
    case 0x040f:
      return _("Icelandic");
    case 0x0410:
      return _("Italian");
    case 0x0810:
      return _("Swiss Italian");
    case 0x0411:
      return _("Japanese");
    case 0x0412:
      return _("Korean");
    case 0x0413:
      return _("Dutch");
    case 0x0813:
      return _("Belgian Dutch");
    case 0x0414:
      return _("Norwegian - Bokmal");
    case 0x0814:
      return _("Norwegian - Nynorsk");
    case 0x0415:
      return _("Polish");
    case 0x0416:
      return _("Brazilian Portuguese");
    case 0x0816:
      return _("Portuguese");
    case 0x0417:
      return _("Rhaeto-Romanic");
    case 0x0418:
      return _("Romanian");
    case 0x0419:
      return _("Russian");
    case 0x041a:
      return _("Croato-Serbian (Latin)");
    case 0x081a:
      return _("Serbo-Croatian (Cyrillic)");
    case 0x041b:
      return _("Slovak");
    case 0x041c:
      return _("Albanian");
    case 0x041d:
      return _("Swedish");
    case 0x041e:
      return _("Thai");
    case 0x041f:
      return _("Turkish");
    case 0x0420:
      return _("Urdu");
    case 0x0421:
      return _("Bahasa"); 
    case 0x0422:
      return _("Ukrainian");
    case 0x0423:
      return _("Byelorussian");
    case 0x0424:
      return _("Slovenian");
    case 0x0425:
      return _("Estonian");
    case 0x0426:
      return _("Latvian");
    case 0x0427:
      return _("Lithuanian");
    case 0x0429:
      return _("Farsi");
    case 0x042D:
      return _("Basque");
    case 0x042F:
      return _("Macedonian");
    case 0x0436:
      return _("Afrikaans");
    case 0x043E:
      return _("Malaysian");  
    default:
      return _("Unknown");
    }
  }


 
  // read the type of the property and displays its value
  static char * getProperty( POLE::Stream* stream ) {
    unsigned long read, type;
    unsigned char buffer[256];
    unsigned char c;
    unsigned long i;
    unsigned int j;
    unsigned long t, t1, t2;
    char *s;
    
    read = stream->read(buffer, 4);
    type = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
    
    switch (type) {
    case 2: // VT_I2
      read = stream->read(buffer, 2);
      i = buffer[0] + (buffer[1] << 8);
      s = (char*) malloc(16);
      snprintf(s, 16, "%u", i);
      return s;
    case 3: // VT_I4
      read = stream->read(buffer, 4);
      i = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
      s = (char*) malloc(16);
      snprintf(s, 16, "%u", i);
      return s;
    case 11: // VT_BOOL
      read = stream->read(buffer, 1);
      if ((char) buffer[0] == -1)
	return strdup("true");
      return strdup("false");
    case 30: // VT_LPSTR
      read = stream->read(buffer, 4);
      i = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
      if ( (i < 0) || (i > 16*1024*1024))
	return NULL;
      s = (char*) malloc(i+1);
      s[i] = '\0';
      j = 0;
      while ( ((c = stream->getch()) != 0) && (i > j) )
	s[j++] = c;
      if ( (j > 0) && (s[j-1] == '\n') )
	s[--j] = '\0';
      if (j != i) {
	free(s);
	return NULL;
      }
      return s;
    case 64: // VT_FILETIME
      read = stream->read(buffer, 8);
      t1 = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
      t2 = buffer[4]  + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
      t = filetime_to_unixtime(t1, t2);
      char * ret = ctime_r((time_t *) &t, (char*)malloc(32));
      ret[strlen(ret)-1] = '\0'; /* kill newline */
      return ret;
    }
    return NULL;
  }


  struct EXTRACTOR_Keywords * libextractor_word_extract(const char * filename,
							const char * data,
							size_t size,
							struct EXTRACTOR_Keywords * prev) {
    char ver[16];
    char product[128];
    if (size < 512 + 898)
      return prev;
    const unsigned char * buffer = (const unsigned char*) &data[512];
    unsigned int wIdent = buffer[0] + (buffer[1] << 8);
    unsigned int nProduct = buffer[4] + (buffer[5] << 8);
    unsigned int lid = buffer[6] + (buffer[7] << 8);
    unsigned int envr = buffer[18];
    unsigned int wMagicCreated = buffer[34] + (buffer[35] << 8);
    unsigned int wMagicRevised = buffer[36] + (buffer[37] << 8);
    unsigned long lProductCreated = buffer[68] + (buffer[69] << 8) + (buffer[70] << 16) + (buffer[71] << 24);
    unsigned long lProductRevised = buffer[72] + (buffer[73] << 8) + (buffer[74] << 16) + (buffer[75] << 24);
    unsigned long fcSttbSavedBy = buffer[722] + (buffer[723] << 8) + (buffer[724] << 16) + (buffer[725] << 24);
    unsigned long lcbSttbSavedBy = buffer[726] + (buffer[727] << 8) + (buffer[728] << 16) + (buffer[729] << 24);
    
    snprintf(ver, 16, "%u", nProduct);
    prev = addKeyword(EXTRACTOR_PRODUCTVERSION,
		      ver,
		      prev);
    prev = addKeyword(EXTRACTOR_LANGUAGE,
		      lidToLanguage(lid),
		      prev);
    char * date = dateToString(lProductCreated);
    snprintf(product, 128, _("%s (Build %s)"),
	     idToProduct(wMagicCreated),
	     date);
    free(date);
    prev = addKeyword(EXTRACTOR_CREATED_BY_SOFTWARE,
		      product,
		      prev);
    date = dateToString(lProductRevised);
    snprintf(product, 128, _("%s (Build %s)"),
	     idToProduct(wMagicRevised),
	     date);
    free(date);
    prev = addKeyword(EXTRACTOR_MODIFIED_BY_SOFTWARE,
		      product,
		      prev);
    
    POLE::Storage* storage = new POLE::Storage( filename );
    storage->open();
    if( storage->result() != POLE::Storage::Ok )
      return prev;
    
    POLE::Stream * stream = storage->stream( "SummaryInformation" );
    if (stream) {
      unsigned char buffer[256];
      
      // ClassID & Offset
      stream->seek(28);
      stream->read(buffer, 20);
      // beginning of section
      unsigned long begin = stream->tell();
      // length of section
      unsigned long read = stream->read(buffer, 4);
      // number of properties
      read = stream->read(buffer, 4);
      unsigned int nproperties = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
      // properties
      for (unsigned int i = 0; i < nproperties; i++) {
	read = stream->read(buffer, 8);
	unsigned int propertyID = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
	unsigned int offsetProp = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
	if (propertyID > 1 && propertyID < 20) {
	  unsigned long offsetCur = stream->tell();
	  stream->seek(offsetProp + begin);
	  char * prop = getProperty(stream);  
	  prev = addKeyword(SummaryProperties[propertyID],
			    prop,
			    prev);
	  free(prop);
	  stream->seek(offsetCur);
	}
      }
    }
    
    unsigned int where = 0;
    
    // FIXME: should look if using 0Table or 1Table
    stream = storage->stream("1Table");
    if (! stream) 
      stream = storage->stream("0Table");
    if (stream) {
      unsigned char * buffer = new unsigned char[lcbSttbSavedBy];
      unsigned char buffer2[1024];
      
      // goto offset of revision
      stream->seek(fcSttbSavedBy);
      // read all the revision history
      if (lcbSttbSavedBy == stream->read(buffer, lcbSttbSavedBy)) {
      
	// there are n strings, so n/2 revisions (author & file)
	unsigned int nRev = (buffer[2] + (buffer[3] << 8)) / 2;
	where = 6;
	for (unsigned int i=0; i < nRev; i++) {	
	  if (where >= lcbSttbSavedBy)
	    break;
	  unsigned int length = buffer[where++];
	  if (where + 2 * length + 2 >= lcbSttbSavedBy)
	    break;
	  char * author = convertToUtf8((const char*) &buffer[where],
					length * 2,
					"UTF-16BE");
	  where += length * 2 + 1;
	  length = buffer[where++];
	  if (where + 2 * length >= lcbSttbSavedBy)
	    break;
	  char * filename = convertToUtf8((const char*) &buffer[where],
					  length * 2,
					  "UTF-16BE");	
	  where += length * 2 + 1;
	  char * rbuf = (char*) malloc(strlen(author) + strlen(filename) + 512);
	  snprintf(rbuf, 512 + strlen(author) + strlen(filename),
		   _("Revision #%u: Author '%s' worked on '%s'"),
		   i, author, filename);
	  free(author);
	  free(filename);
	  prev = addKeyword(EXTRACTOR_REVISION_HISTORY,
			    rbuf, 
			    prev);
	  free(rbuf);
	}
      }
      delete buffer;
    
    }
    delete storage;
    
    return prev;
  }

}

