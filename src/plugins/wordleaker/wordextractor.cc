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

extern "C" {

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

 
  // read the type of the property and displays its value
  char * getProperty( POLE::Stream* stream ) {
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
      return ctime_r((time_t *) &t, (char*)malloc(32));
    }
    return NULL;
  }


  struct EXTRACTOR_Keywords * libextractor_word_extract(const char * filename,
							const char * data,
							size_t size,
							struct EXTRACTOR_Keywords * prev) {
    char ver[16];
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
    
    // cout << "Created by: " << idToProduct(wMagicCreated) << " (Build " << dateToString(lProductCreated) << ")" << endl;
    // cout << "Revised by: " << idToProduct(wMagicRevised) << " (Build " << dateToString(lProductRevised) << ")" << endl;
    
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
	  // cout << SummaryProperties[propertyID] << ": ";
	  unsigned long offsetCur = stream->tell();
	  stream->seek(offsetProp + begin);
	  // read and show the property
	  char * prop = getProperty(stream);  
	  free(prop);
	  stream->seek(offsetCur);
	}
      }
    }
    
    unsigned int where = 0;
    
    // FIXME: should look if using 0Table or 1Table
    stream = storage->stream( "1Table" );
    if (stream) {
      unsigned char * buffer = new unsigned char[lcbSttbSavedBy];
      unsigned char buffer2[1024];
      
      // goto offset of revision
      stream->seek(fcSttbSavedBy);
      // read all the revision history
      stream->read(buffer, lcbSttbSavedBy);
      
      // there are n strings, so n/2 revisions (author & file)
      unsigned int nRev = (buffer[2] + (buffer[3] << 8)) / 2;
      where = 6;
      
      for (unsigned int i=0; i < nRev; i++) {
	// cout << "Rev #" << i << ": Author \"";
	unsigned int length = buffer[where++];
	// it's unicode, for now we only get the low byte
	for (unsigned int j=0; j < length; j++) {
	  where++;
	  // cout << buffer[where];
	  where++;
	}
	where++;
	// cout << "\" worked on file \"";
	length = buffer[where++];
	// it's unicode, for now we only get the low byte
	for (unsigned int j=0; j < length; j++) {
	  where++;
	  // cout << buffer[where];
	  where++;
	}
	where++;
	// cout << "\"" << endl;    
      }
      
      delete buffer;
    
    }
    delete storage;
    
    return prev;
  }

}

