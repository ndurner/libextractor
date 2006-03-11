/* 
   WordLeaker - Shows information about Word DOC files
   Copyright (C) 2005 Sacha Fuentes <madelman@iname.com>

   Based on poledump.c
   Original idea from WordDumper (http://www.computerbytesman.com)
   Info on Word format: http://www.aozw65.dsl.pipex.com/generator_wword8.htm
   Info on Word format: http://jakarta.apache.org/poi/hpsf/internals.html
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, US
*/

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <list>
#include <ctime>

#include "pole.h"
#include "WordLeaker.h"

unsigned long fcSttbSavedBy;
unsigned long lcbSttbSavedBy;


  
// read the type of the property and displays its value
void showProperty( POLE::Stream* stream ) {
  unsigned long read, type;
  unsigned char buffer[256];
  unsigned char c;
  unsigned long i;
  unsigned long t, t1, t2;
  char *s;
    
  read = stream->read(buffer, 4);
  type = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
    
  switch (type) {
      case 2: // VT_I2
        read = stream->read(buffer, 2);
        i = buffer[0] + (buffer[1] << 8);
        cout << i << endl;
        break;
      case 3: // VT_I4
        read = stream->read(buffer, 4);
        i = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
        cout << i << endl;
        break;
      case 11: // VT_BOOL
        read = stream->read(buffer, 1);
        if ((char) buffer[0] == -1)
            cout << "true" << endl;
        else        
            cout << "false" << endl;
        break;
      case 30: // VT_LPSTR
        read = stream->read(buffer, 4);
        i = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
        while ((c = stream->getch()) != 0)
            cout << c;
        cout << endl;
        break;
      case 64: // VT_FILETIME
        read = stream->read(buffer, 8);
        t1 = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
        t2 = buffer[4]  + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
        t = filetime_to_unixtime(t1, t2);
        s = ctime((time_t *) &t);
        cout << s;
        break;
      default:
          cout << "Unknown format " << type << endl;
  }
}

// show the revision data (users and files)
void dumpRevision( POLE::Storage* storage ) {
  unsigned int nRev;
  unsigned int where = 0;
  POLE::Stream* stream;
    
  cout << "Revision:" << endl;
  cout << "---------" << endl << endl;

  // FIXME: should look if using 0Table or 1Table
  stream = storage->stream( "1Table" );
  if( !stream ) {
      cout << "There's no revision information" << endl;
      return;
  }
  
  unsigned char * buffer = new unsigned char[lcbSttbSavedBy];
  unsigned char buffer2[1024];
  unsigned int length;
  
  // goto offset of revision
  stream->seek(fcSttbSavedBy);
  // read all the revision history
  stream->read(buffer, lcbSttbSavedBy);

  // there are n strings, so n/2 revisions (author & file)
  nRev = (buffer[2] + (buffer[3] << 8)) / 2;
  where = 6;
  
  for (unsigned int i=0; i < nRev; i++) {
    cout << "Rev #" << i << ": Author \"";
    length = buffer[where++];
    // it's unicode, for now we only get the low byte
    for (unsigned int j=0; j < length; j++) {
        where++;
        cout << buffer[where];
        where++;
    }
    where++;
    cout << "\" worked on file \"";
    length = buffer[where++];
    // it's unicode, for now we only get the low byte
    for (unsigned int j=0; j < length; j++) {
        where++;
        cout << buffer[where];
        where++;
    }
    where++;
    cout << "\"" << endl;    
  }
  
  cout << endl;      
  delete buffer;
  
}

// show data from DocumentSummary stream
void dumpDocumentSummary( POLE::Storage* storage ) {
  POLE::Stream* stream;
  unsigned long read, nproperties, propertyID, offsetProp, offsetCur;
  unsigned long begin;
    
  cout << "Document Summary:" << endl;
  cout << "-----------------" << endl << endl;

  stream = storage->stream( "DocumentSummaryInformation" );
  if( !stream ) {
      cout << "There's no document summary information" << endl;
      return;
  }
  
  unsigned char buffer[256];

  // ClassID & Offset
  stream->seek(28);
  stream->read(buffer, 20);
  // beginning of section
  begin = stream->tell();
  // length of section
  read = stream->read(buffer, 4);
  // number of properties
  read = stream->read(buffer, 4);
  nproperties = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
  // properties

  for (unsigned long i = 0; i < nproperties; i++) {
    read = stream->read(buffer, 8);
    propertyID = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
    offsetProp = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
      if (propertyID > 1 && propertyID < 16) {
        cout << DocumentSummaryProperties[propertyID] << ": ";
        offsetCur = stream->tell();
        stream->seek(offsetProp + begin);
        // read and show the property
        showProperty(stream);  
        stream->seek(offsetCur);
    }
  }

  cout << endl;      
}

// show data from Summary stream
void dumpSummary( POLE::Storage* storage ) {
  POLE::Stream* stream;
  unsigned long read, nproperties, propertyID, offsetProp, offsetCur;
  unsigned long begin;
    
  cout << "Summary:" << endl;
  cout << "--------" << endl << endl;

  stream = storage->stream( "SummaryInformation" );
  if( !stream ) {
      cout << "There's no summary information" << endl;
      return;
  }
  
  unsigned char buffer[256];

  // ClassID & Offset
  stream->seek(28);
  stream->read(buffer, 20);
  // beginning of section
  begin = stream->tell();
  // length of section
  read = stream->read(buffer, 4);
  // number of properties
  read = stream->read(buffer, 4);
  nproperties = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
  // properties
  for (unsigned long i = 0; i < nproperties; i++) {
    read = stream->read(buffer, 8);
    propertyID = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
    offsetProp = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
    if (propertyID > 1 && propertyID < 20) {
        cout << SummaryProperties[propertyID] << ": ";
        offsetCur = stream->tell();
        stream->seek(offsetProp + begin);
        // read and show the property
        showProperty(stream);  
        stream->seek(offsetCur);
    }
  }

  cout << endl;      
}

// reads the header of the file
bool readFIB( char* filename ) {
  fstream file;
    
   file.open( filename, std::ios::binary | std::ios::in );
  if( !file.good() ) {
    cout << "Can't find the file" << endl;
    return false;
  }
  
  unsigned char * buffer = new unsigned char[898];
  file.seekg( 512 ); 
  file.read( (char*)buffer, 898 );
  file.close();
  
  unsigned int wIdent = buffer[0] + (buffer[1] << 8);
  unsigned int nProduct = buffer[4] + (buffer[5] << 8);
  unsigned int lid = buffer[6] + (buffer[7] << 8);
  unsigned int envr = buffer[18];
  unsigned int wMagicCreated = buffer[34] + (buffer[35] << 8);
  unsigned int wMagicRevised = buffer[36] + (buffer[37] << 8);
  unsigned long lProductCreated = buffer[68] + (buffer[69] << 8) + (buffer[70] << 16) + (buffer[71] << 24);
  unsigned long lProductRevised = buffer[72] + (buffer[73] << 8) + (buffer[74] << 16) + (buffer[75] << 24);
  fcSttbSavedBy = buffer[722] + (buffer[723] << 8) + (buffer[724] << 16) + (buffer[725] << 24);
  lcbSttbSavedBy = buffer[726] + (buffer[727] << 8) + (buffer[728] << 16) + (buffer[729] << 24);
  delete[] buffer; 
  
  cout << "File: " << filename << endl;
  cout << "Product version: " << nProduct << endl;  
  cout << "Language: " << lidToLanguage(lid) << endl;
  cout << "Created by: " << idToProduct(wMagicCreated) << " (Build " << dateToString(lProductCreated) << ")" << endl;
  cout << "Revised by: " << idToProduct(wMagicRevised) << " (Build " << dateToString(lProductRevised) << ")" << endl;
  cout << endl;
  
  return true; 
    
}

int main(int argc, char *argv[]) {
  cout << endl << "WordLeaker v.0.1" << endl;
  cout << " by Madelman (http://elligre.tk/madelman/)" << endl << endl;
  
    
  if( argc < 2 ) {
    cout << "  You must supply a filename" << endl << endl;
    return 0;
  }
  
  char* filename = argv[1];

  if ( !readFIB(filename) )
      return 1;
  
  POLE::Storage* storage = new POLE::Storage( filename );
  storage->open();
  if( storage->result() != POLE::Storage::Ok ) {
    cout << "The file " << filename << " is not a Word document" << endl;
    return 1;
  }
  
  dumpSummary( storage );
  // FIXME: doesn't always work
  // but there's nothing really interesting in here
  //dumpDocumentSummary( storage );
  dumpRevision( storage );
  // TODO: we don't show the GUID
  // TODO: we don't show the macros
  
  delete storage;
  
  return 0;
}
