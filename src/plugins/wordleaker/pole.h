/* POLE - Portable C++ library to access OLE Storage 
   Copyright (C) 2002-2004 Ariya Hidayat <ariya@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, US
*/

#ifndef POLE_H
#define POLE_H

#include <string>
#include <list>

namespace POLE
{

class StorageIO;
class Stream;
class StreamImpl;

class Storage
{
  friend class Stream;
  friend class StreamOut;

public:

  enum { Ok, OpenFailed, NotOLE, BadOLE, UnknownError, 
    StupidWorkaroundForBrokenCompiler=255 };

  /**
   * Constructs a storage with name filename.
   **/
  Storage( const char* filename );

  /**
   * Destroys the storage.
   **/
  ~Storage();
  
  /**
   * Opens the storage. Returns true if no error occurs.
   **/
  bool open();

  /**
   * Closes the storage.
   **/
  void close();

  /**
   * Returns the error code of last operation.
   **/
  int result();

  /**
   * Returns the current path.
   **/
  std::string path();

  /**
   * Finds all stream and directories in current path.
   **/
  std::list<std::string> listDirectory();

  /**
   * Changes path to directory. Returns true if no error occurs.
   **/
  bool enterDirectory( const std::string& directory );

  /**
   * Goes to one directory up.
   **/
  void leaveDirectory();

  /**
   * Finds and returns a stream with the specified name.
   **/
  Stream* stream( const std::string& name );
  
private:
  StorageIO* io;
  
  // no copy or assign
  Storage( const Storage& );
  Storage& operator=( const Storage& );

};

class Stream
{
  friend class Storage;
  friend class StorageIO;
  
public:
  
  /**
   * Returns the stream size.
   **/
  unsigned long size();

  /**
   * Returns the read pointer.
   **/
  unsigned long tell();

  /**
   * Sets the read position.
   **/
  void seek( unsigned long pos ); 

  /**
   * Reads a byte.
   **/
  int getch();

  /**
   * Reads a block of data.
   **/
  unsigned long read( unsigned char* data, unsigned long maxlen );

private:

  Stream();
  ~Stream();

  // no copy or assign
  Stream( const Stream& );
  Stream& operator=( const Stream& );
    
  StreamImpl* impl;
};


}

#endif // POLE_H
