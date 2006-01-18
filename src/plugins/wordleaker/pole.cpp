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

#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "pole.h"

namespace POLE
{

class Header
{
  public:
    unsigned char id[8];       // signature, or magic identifier
    unsigned b_shift;          // bbat->blockSize = 1 << b_shift
    unsigned s_shift;          // sbat->blockSize = 1 << s_shift
    unsigned num_bat;          // blocks allocated for big bat
    unsigned dirent_start;     // starting block for directory info
    unsigned threshold;        // switch from small to big file (usually 4K)
    unsigned sbat_start;       // starting block index to store small bat
    unsigned num_sbat;         // blocks allocated for small bat
    unsigned mbat_start;       // starting block to store meta bat
    unsigned num_mbat;         // blocks allocated for meta bat
    unsigned long bb_blocks[109];
    
    Header();
    void load( const unsigned char* buffer );
    void save( unsigned char* buffer );
    void debug();
};

class AllocTable
{
  public:
    static const unsigned Eof;
    static const unsigned Avail;
    static const unsigned Bat;    
    unsigned blockSize;
    AllocTable();
    void clear();
    unsigned long count();
    void resize( unsigned long newsize );
    void preserve( unsigned long n );
    void set( unsigned long index, unsigned long val );
    unsigned unused();
    void setChain( std::vector<unsigned long> );
    std::vector<unsigned long> follow( unsigned long start );
    unsigned long operator[](unsigned long index );
    void load( const unsigned char* buffer, unsigned len );
    void save( unsigned char* buffer );
    unsigned size();
    void debug();
  private:
    std::vector<unsigned long> data;
    AllocTable( const AllocTable& );
    AllocTable& operator=( const AllocTable& );
};

class DirEntry
{
  public:
    std::string name;
    bool dir;              // true if directory   
    unsigned long size;    // size (not valid if directory)
    unsigned long start;   // starting block
    unsigned prev;         // previous sibling
    unsigned next;         // next sibling
    unsigned child;        // first child
};

class DirTree
{
  public:
    static const unsigned End;
    DirTree();
    void clear();
    unsigned entryCount();
    DirEntry* entry( unsigned index );
    DirEntry* entry( const std::string& name, bool create=false );
    int indexOf( DirEntry* e );
    int parent( unsigned index );
    std::string fullName( unsigned index );
    std::vector<unsigned> children( unsigned index );
    std::vector<DirEntry*> listDirectory();
    bool enterDirectory( const std::string& dir );
    void leaveDirectory();
    std::string path();
    void load( unsigned char* buffer, unsigned len );
    void save( unsigned char* buffer );
    unsigned size();
    void debug();
  private:
    unsigned current;
    std::vector<DirEntry> entries;
    DirTree( const DirTree& );
    DirTree& operator=( const DirTree& );
};

class StorageIO
{
  public:
    Storage* storage;
    std::string filename;
    std::fstream file;
    int result;               // result of operation
    bool opened;              // true if file is opened
    unsigned long filesize;   // size of the file
    
    Header* header;           // storage header 
    DirTree* dirtree;         // directory tree
    AllocTable* bbat;         // allocation table for big blocks
    AllocTable* sbat;         // allocation table for small blocks
    
    std::vector<unsigned long> sb_blocks; // blocks for "small" files
       
    std::list<Stream*> streams;

    StorageIO( Storage* storage, const char* filename );
    ~StorageIO();
    
    bool open();
    void close();
    void flush();
    void load();
    void create();

    unsigned long loadBigBlocks( std::vector<unsigned long> blocks, unsigned char* buffer, unsigned long maxlen );

    unsigned long loadBigBlock( unsigned long block, unsigned char* buffer, unsigned long maxlen );

    unsigned long loadSmallBlocks( std::vector<unsigned long> blocks, unsigned char* buffer, unsigned long maxlen );

    unsigned long loadSmallBlock( unsigned long block, unsigned char* buffer, unsigned long maxlen );

  private:  
    // no copy or assign
    StorageIO( const StorageIO& );
    StorageIO& operator=( const StorageIO& );

};

class StreamImpl
{
  public:
    StreamImpl( StorageIO* io, DirEntry* entry );
    ~StreamImpl();
    unsigned long size();
    void seek( unsigned long pos );
    unsigned long tell();
    int getch();
    unsigned long read( unsigned char* data, unsigned long maxlen );
    unsigned long read( unsigned long pos, unsigned char* data, unsigned long maxlen );

    StorageIO* io;
    DirEntry* entry;

  private:
    std::vector<unsigned long> blocks;

    // no copy or assign
    StreamImpl( const StreamImpl& );
    StreamImpl& operator=( const StreamImpl& );

    // pointer for read
    unsigned long m_pos;

    // simple cache system to speed-up getch()
    unsigned char* cache_data;
    unsigned long cache_size;
    unsigned long cache_pos;
    void updateCache();
};

}; // namespace POLE

using namespace POLE;

static inline unsigned long readU16( const unsigned char* ptr )
{
  return ptr[0]+(ptr[1]<<8);
}

static inline unsigned long readU32( const unsigned char* ptr )
{
  return ptr[0]+(ptr[1]<<8)+(ptr[2]<<16)+(ptr[3]<<24);
}

static inline void writeU16( unsigned char* ptr, unsigned long data )
{
  ptr[0] = data & 0xff;
  ptr[1] = (data >> 8) & 0xff;
}

static inline void writeU32( unsigned char* ptr, unsigned long data )
{
  ptr[0] = data & 0xff;
  ptr[1] = (data >> 8) & 0xff;
  ptr[2] = (data >> 16) & 0xff;
  ptr[3] = (data >> 24) & 0xff;
}

static const unsigned char pole_magic[] = 
 { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };

// =========== Header ==========

Header::Header()
{
  b_shift = 9;
  s_shift = 6;
  num_bat = 0;
  dirent_start = 0;
  threshold = 4096;
  sbat_start = 0;
  num_sbat = 0;
  mbat_start = 0;
  num_mbat = 0;

  for( unsigned i = 0; i < 8; i++ )
    id[i] = pole_magic[i];  
  for( unsigned i=0; i<109; i++ )
    bb_blocks[i] = AllocTable::Avail;
}

void Header::load( const unsigned char* buffer )
{
  b_shift     = readU16( buffer + 0x1e );
  s_shift     = readU16( buffer + 0x20 );
  num_bat      = readU32( buffer + 0x2c );
  dirent_start = readU32( buffer + 0x30 );
  threshold    = readU32( buffer + 0x38 );
  sbat_start   = readU32( buffer + 0x3c );
  num_sbat     = readU32( buffer + 0x40 );
  mbat_start   = readU32( buffer + 0x44 );
  num_mbat     = readU32( buffer + 0x48 );
  
  for( unsigned i = 0; i < 8; i++ )
    id[i] = buffer[i];  
  for( unsigned i=0; i<109; i++ )
    bb_blocks[i] = readU32( buffer + 0x4C+i*4 );
}

void Header::save( unsigned char* buffer )
{
  memset( buffer, 0, 0x4c );
  memcpy( buffer, pole_magic, 8 );        // ole signature
  writeU32( buffer + 8, 0 );              // unknown 
  writeU32( buffer + 12, 0 );             // unknown
  writeU32( buffer + 16, 0 );             // unknown
  writeU16( buffer + 24, 0x003e );        // revision ?
  writeU16( buffer + 26, 3 );             // version ?
  writeU16( buffer + 28, 0xfffe );        // unknown
  writeU16( buffer + 0x1e, b_shift );
  writeU16( buffer + 0x20, s_shift );
  writeU32( buffer + 0x2c, num_bat );
  writeU32( buffer + 0x30, dirent_start );
  writeU32( buffer + 0x38, threshold );
  writeU32( buffer + 0x3c, sbat_start );
  writeU32( buffer + 0x40, num_sbat );
  writeU32( buffer + 0x44, mbat_start );
  writeU32( buffer + 0x48, num_mbat );
  
  for( unsigned i=0; i<109; i++ )
    writeU32( buffer + 0x4C+i*4, bb_blocks[i] );
}

void Header::debug()
{
  std::cout << std::endl;
  std::cout << "b_shift " << b_shift << std::endl;
  std::cout << "s_shift " << s_shift << std::endl;
  std::cout << "num_bat " << num_bat << std::endl;
  std::cout << "dirent_start " << dirent_start << std::endl;
  std::cout << "threshold " << threshold << std::endl;
  std::cout << "sbat_start " << sbat_start << std::endl;
  std::cout << "num_sbat " << num_sbat << std::endl;
  std::cout << "mbat_start " << mbat_start << std::endl;
  std::cout << "num_mbat " << num_mbat << std::endl;
  
  unsigned s = (num_bat<=109) ? num_bat : 109;
  std::cout << "bat blocks: ";
  for( unsigned i = 0; i < s; i++ )
    std::cout << bb_blocks[i] << " ";
  std::cout << std::endl;
}
 
// =========== AllocTable ==========

const unsigned AllocTable::Avail = 0xffffffff;
const unsigned AllocTable::Eof = 0xfffffffe;
const unsigned AllocTable::Bat = 0xfffffffd;

AllocTable::AllocTable()
{
  blockSize = 4096;
  // initial size
  resize( 128 );
}

unsigned long AllocTable::count()
{
  return data.size();
}

void AllocTable::resize( unsigned long newsize )
{
  unsigned oldsize = data.size();
  data.resize( newsize );
  if( newsize > oldsize )
    for( unsigned i = oldsize; i<newsize; i++ )
      data[i] = Avail;
}

// make sure there're still free blocks
void AllocTable::preserve( unsigned long n )
{
  std::vector<unsigned long> pre;
  for( unsigned i=0; i < n; i++ )
    pre.push_back( unused() );
}

unsigned long AllocTable::operator[]( unsigned long index )
{
  unsigned long result;
  result = data[index];
  return result;
}

void AllocTable::set( unsigned long index, unsigned long value )
{
  if( index >= count() ) resize( index + 1);
  data[ index ] = value;
}

void AllocTable::setChain( std::vector<unsigned long> chain )
{
  if( chain.size() )
  {
    for( unsigned i=0; i<chain.size()-1; i++ )
      set( chain[i], chain[i+1] );
    set( chain[ chain.size()-1 ], AllocTable::Eof );
  }
}

// follow 
std::vector<unsigned long> AllocTable::follow( unsigned long start )
{
  std::vector<unsigned long> chain;

  if( start >= count() ) return chain; 

  unsigned long p = start;
  while( p < count() )
  {
    if( p >= (unsigned long)Eof ) break;
    if( p >= count() ) break;
    chain.push_back( p );
    if( data[p] >= count() ) break;
    p = data[ p ];
  }

  return chain;
}

unsigned AllocTable::unused()
{
  // find first available block
  for( unsigned i = 0; i < data.size(); i++ )
    if( data[i] == Avail )
      return i;
  
  // completely full, so enlarge the table
  unsigned block = data.size();
  resize( data.size()+10 );
  return block;      
}

void AllocTable::load( const unsigned char* buffer, unsigned len )
{
  resize( len / 4 );
  for( unsigned i = 0; i < count(); i++ )
    set( i, readU32( buffer + i*4 ) );
}

// return space required to save this dirtree
unsigned AllocTable::size()
{
  return count() * 4;
}

void AllocTable::save( unsigned char* buffer )
{
  for( unsigned i = 0; i < count(); i++ )
    writeU32( buffer + i*4, data[i] );
}

void AllocTable::debug()
{
  std::cout << "block size " << data.size() << std::endl;
  for( unsigned i=0; i< data.size(); i++ )
  {
     if( data[i] == Avail ) continue;
     std::cout << i << ": ";
     if( data[i] == Eof ) std::cout << "eof";
     else std::cout << data[i];
     std::cout << std::endl;
  }
}

// =========== DirTree ==========

const unsigned DirTree::End = 0xffffffff;

DirTree::DirTree()
{
  current = 0;
  clear();
}

void DirTree::clear()
{
  // leave only root entry
  entries.resize( 1 );
  entries[0].name = "Root Entry";
  entries[0].dir = true;
  entries[0].size = 0;
  entries[0].start = End;
  entries[0].prev = End;
  entries[0].next = End;
  entries[0].child = End;
  current = 0;
}

unsigned DirTree::entryCount()
{
  return entries.size();
}

DirEntry* DirTree::entry( unsigned index )
{
  if( index >= entryCount() ) return (DirEntry*) 0;
  return &entries[ index ];
}

int DirTree::indexOf( DirEntry* e )
{
  for( unsigned i = 0; i < entryCount(); i++ )
    if( entry( i ) == e ) return i;
    
  return -1;
}

int DirTree::parent( unsigned index )
{
  // brute-force, basically we iterate for each entries, find its children
  // and check if one of the children is 'index'
  for( unsigned j=0; j<entryCount(); j++ )
  {
    std::vector<unsigned> chi = children( j );
    for( unsigned i=0; i<chi.size();i++ )
      if( chi[i] == index )
        return j;
  }
        
  return -1;
}

std::string DirTree::fullName( unsigned index )
{
  // don't use root name ("Root Entry"), just give "/"
  if( index == 0 ) return "/";

  std::string result = entry( index )->name;
  result.insert( 0,  "/" );
  int p = parent( index );
  while( p > 0 )
  {
     result.insert( 0, entry( p )->name );
     result.insert( 0,  "/" );
     index = p;
     if( index <= 0 ) break;
  }
  return result;
}

// given a fullname (e.g "/ObjectPool/_1020961869"), find the entry
// if not found and create is false, return 0
// if create is true, a new entry is returned
DirEntry* DirTree::entry( const std::string& name, bool create )
{
   if( !name.length() ) return (DirEntry*)0;
   
   // quick check for "/" (that's root)
   if( name == "/" ) return entry( 0 );
   
   // split the names, e.g  "/ObjectPool/_1020961869" will become:
   // "ObjectPool" and "_1020961869" 
   std::list<std::string> names;
   std::string::size_type start = 0, end = 0;
   while( start < name.length() )
   {
     end = name.find_first_of( '/', start );
     if( end == std::string::npos ) end = name.length();
     names.push_back( name.substr( start, end-start ) );
     start = end+1;
   }
  
   // start from root when name is absolute
   // or current directory when name is relative
   int index = (name[0] == '/' ) ? 0 : current;

   // trace one by one   
   std::list<std::string>::iterator it; 
   for( it = names.begin(); it != names.end(); ++it )
   {
     // find among the children of index
     std::vector<unsigned> chi = children( index );
     unsigned child = 0;
     for( unsigned i = 0; i < chi.size(); i++ )
     {
       DirEntry* ce = entry( chi[i] );
       if( ce ) if( ce->name == *it )
         child = chi[i];
     }
     
     // traverse to the child
     if( child > 0 ) index = child;
     else
     {
       // not found among children
       if( !create ) return (DirEntry*)0;
       
       // create a new entry
       unsigned parent = index;
       entries.push_back( DirEntry() );
       index = entryCount()-1;
       DirEntry* e = entry( index );
       e->name = *it;
       e->dir = false;
       e->size = 0;
       e->start = 0;
       e->child = End;
       e->prev = End;
       e->next = entry(parent)->child;
       entry(parent)->child = index;
     }
   }

   return entry( index );
}

// helper function: recursively find siblings of index
void dirtree_find_siblings( DirTree* dirtree, std::vector<unsigned>& result, 
  unsigned index )
{
  DirEntry* e = dirtree->entry( index );
  if( !e ) return;

  // prevent infinite loop  
  for( unsigned i = 0; i < result.size(); i++ )
    if( result[i] == index ) return;

  // add myself    
  result.push_back( index );
  
  // visit previous sibling, don't go infinitely
  unsigned prev = e->prev;
  if( ( prev > 0 ) && ( prev < dirtree->entryCount() ) )
  {
    for( unsigned i = 0; i < result.size(); i++ )
      if( result[i] == prev ) prev = 0;
    if( prev ) dirtree_find_siblings( dirtree, result, prev );
  }
    
  // visit next sibling, don't go infinitely
  unsigned next = e->next;
  if( ( next > 0 ) && ( next < dirtree->entryCount() ) )
  {
    for( unsigned i = 0; i < result.size(); i++ )
      if( result[i] == next ) next = 0;
    if( next ) dirtree_find_siblings( dirtree, result, next );
  }
}

std::vector<unsigned> DirTree::children( unsigned index )
{
  std::vector<unsigned> result;
  
  DirEntry* e = entry( index );
  if( e ) if( e->child < entryCount() )
    dirtree_find_siblings( this, result, e->child );
    
  return result;
}

std::vector<DirEntry*> DirTree::listDirectory()
{
  std::vector<DirEntry*> result;
  
  std::vector<unsigned> chi = children( current );
  for( unsigned i = 0; i < chi.size(); i++ )
    result.push_back( entry( chi[i] ) );
  
  return result;
}

bool DirTree::enterDirectory( const std::string& dir )
{
  DirEntry* e = entry( dir );
  if( !e ) return false;
  if( !e->dir ) return false;
  
  int index = indexOf( e );
  if( index < 0 ) return false;
    
  current = index;
  return true;
}

void DirTree::leaveDirectory()
{
  // already at root ?
  if( current == 0 ) return;

  int p = parent( current );
  if( p >= 0 ) current = p;
}

std::string DirTree::path()
{
  return fullName( current );
}

void DirTree::load( unsigned char* buffer, unsigned size )
{
  entries.clear();
  current = 0;
  
  for( unsigned i = 0; i < size/128; i++ )
  {
    unsigned p = i * 128;
    
    // would be < 32 if first char in the name isn't printable
    unsigned prefix = 32;
    
    // parse name of this entry, which stored as Unicode 16-bit
    std::string name;
    int name_len = readU16( buffer + 0x40+p );
    for( int j=0; ( buffer[j+p]) && (j<name_len); j+= 2 )
      name.append( 1, buffer[j+p] );
      
    // first char isn't printable ? remove it...
    if( buffer[p] < 32 )
    { 
      prefix = buffer[0]; 
      name.erase( 0,1 ); 
    }

    DirEntry e;
    e.name = name;
    e.start = readU32( buffer + 0x74+p );
    e.size = readU32( buffer + 0x78+p );
    e.prev = readU32( buffer + 0x44+p );
    e.next = readU32( buffer + 0x48+p );
    e.child = readU32( buffer + 0x4C+p );
    e.dir = ( buffer[ 0x42 + p]!=2 );
    
    entries.push_back( e );
  }  
}

// return space required to save this dirtree
unsigned DirTree::size()
{
  return entryCount() * 128;
}

void DirTree::save( unsigned char* buffer )
{
  memset( buffer, 0, size() );
  
  // root is fixed as "Root Entry"
  DirEntry* root = entry( 0 );
  std::string name = "Root Entry";
  for( unsigned j = 0; j < name.length(); j++ )
    buffer[ j*2 ] = name[j];
  writeU16( buffer + 0x40, name.length()*2 + 2 );    
  writeU32( buffer + 0x74, 0xffffffff );
  writeU32( buffer + 0x78, 0 );
  writeU32( buffer + 0x44, 0xffffffff );
  writeU32( buffer + 0x48, 0xffffffff );
  writeU32( buffer + 0x4c, root->child );
  buffer[ 0x42 ] = 5;
  buffer[ 0x43 ] = 1; 

  for( unsigned i = 1; i < entryCount(); i++ )
  {
    DirEntry* e = entry( i );
    if( !e ) continue;
    if( e->dir )
    {
      e->start = 0xffffffff;
      e->size = 0;
    }
    
    // max length for name is 32 chars
    std::string name = e->name;
    if( name.length() > 32 )
      name.erase( 32, name.length() );
      
    // write name as Unicode 16-bit
    for( unsigned j = 0; j < name.length(); j++ )
      buffer[ i*128 + j*2 ] = name[j];

    writeU16( buffer + i*128 + 0x40, name.length()*2 + 2 );    
    writeU32( buffer + i*128 + 0x74, e->start );
    writeU32( buffer + i*128 + 0x78, e->size );
    writeU32( buffer + i*128 + 0x44, e->prev );
    writeU32( buffer + i*128 + 0x48, e->next );
    writeU32( buffer + i*128 + 0x4c, e->child );
    buffer[ i*128 + 0x42 ] = e->dir ? 1 : 2;
    buffer[ i*128 + 0x43 ] = 1; // always black
  }  
}

void DirTree::debug()
{
  for( unsigned i = 0; i < entryCount(); i++ )
  {
    DirEntry* e = entry( i );
    if( !e ) continue;
    std::cout << i << ": ";
    std::cout << e->name << " ";
    if( e->dir ) std::cout << "(Dir) ";
    else std::cout << "(File) ";
    std::cout << e->size << " ";
    std::cout << "s:" << e->start << " ";
    std::cout << "(";
    if( e->child == End ) std::cout << "-"; else std::cout << e->child;
    std::cout << " ";
    if( e->prev == End ) std::cout << "-"; else std::cout << e->prev;
    std::cout << ":";
    if( e->next == End ) std::cout << "-"; else std::cout << e->next;
    std::cout << ")";    
    std::cout << std::endl;
  }
}

// =========== StorageIO ==========

StorageIO::StorageIO( Storage* st, const char* fname )
{
  storage = st;
  filename = fname;
  result = Storage::Ok;
  opened = false;
  
  header = new Header();
  dirtree = new DirTree();
  bbat = new AllocTable();
  sbat = new AllocTable();
  
  filesize = 0;
  bbat->blockSize = 1 << header->b_shift;
  sbat->blockSize = 1 << header->s_shift;
}

StorageIO::~StorageIO()
{
  if( opened ) close();
  delete sbat;
  delete bbat;
  delete dirtree;
  delete header;
}

bool StorageIO::open()
{
  // already opened ? close first
  if( opened ) close();
  
  load();
  
  return result == Storage::Ok;
}

void StorageIO::load()
{
  unsigned char* buffer = 0;
  unsigned long buflen = 0;
  std::vector<unsigned long> blocks;
  
  // open the file, check for error
  result = Storage::OpenFailed;
  file.open( filename.c_str(), std::ios::binary | std::ios::in );
  if( !file.good() ) return;
  
  // find size of input file
  file.seekg( 0, std::ios::end );
  filesize = file.tellg();

  // load header
  buffer = new unsigned char[512];
  file.seekg( 0 ); 
  file.read( (char*)buffer, 512 );
  header->load( buffer );
  delete[] buffer;

  // check OLE magic id
  result = Storage::NotOLE;
  for( unsigned i=0; i<8; i++ )
    if( header->id[i] != pole_magic[i] )
      return;
  
  // sanity checks
  result = Storage::BadOLE;
  if( header->threshold != 4096 ) return;
  if( header->num_bat == 0 ) return;
  if( header->s_shift > header->b_shift ) return;
  if( header->b_shift <= 6 ) return;
  if( header->b_shift >=31 ) return;
  
  // important block size
  bbat->blockSize = 1 << header->b_shift;
  sbat->blockSize = 1 << header->s_shift;
  
  // find blocks allocated to store big bat
  // the first 109 blocks are in header, the rest in meta bat
  blocks.resize( header->num_bat );
  for( unsigned i = 0; i < header->num_bat; i++ )
    if( i < 109 ) blocks[i] = header->bb_blocks[i];
  if( header->num_bat > 109 )
  if( header->num_mbat > 0 )
  {
    buffer = new unsigned char[ bbat->blockSize ];
    unsigned k = 109;
    for( unsigned r = 0; r < header->num_mbat; r++ )
    {
      loadBigBlock( header->mbat_start+r, buffer, bbat->blockSize );
      for( unsigned s=0; s < bbat->blockSize/4; s+=4 )
        blocks[k++] = readU32( buffer + s );
      // FIXME check if k > num_bat
    }    
    delete[] buffer;
  }
  
  // load big bat
  buflen = blocks.size()*bbat->blockSize;
  buffer = new unsigned char[ buflen ];  
  loadBigBlocks( blocks, buffer, buflen );
  bbat->load( buffer, buflen );
  delete[] buffer;

  // load small bat
  blocks.clear();
  blocks = bbat->follow( header->sbat_start );
  buflen = blocks.size()*bbat->blockSize;
  buffer = new unsigned char[ buflen ];  
  loadBigBlocks( blocks, buffer, buflen );
  sbat->load( buffer, buflen );
  delete[] buffer;
  
  // load directory tree
  blocks = bbat->follow( header->dirent_start );
  buflen = blocks.size()*bbat->blockSize;
  buffer = new unsigned char[ buflen ];  
  loadBigBlocks( blocks, buffer, buflen );
  sb_blocks = bbat->follow( readU32( buffer + 0x74 ) ); // small files
  dirtree->load( buffer, buflen );

  // fetch block chain as data for small-files
  delete[] buffer;
  
  // so far so good
  result = Storage::Ok;
  opened = true;
}

void StorageIO::create()
{
  // std::cout << "Creating " << filename << std::endl; 
  
  file.open( filename.c_str(), std::ios::out|std::ios::binary );
  if( !file.good() )
  {
    std::cerr << "Can't create " << filename << std::endl;
    result = Storage::OpenFailed;
    return;
  }
  
  // so far so good
  opened = true;
  result = Storage::Ok;
}

void StorageIO::close()
{
  if( !opened ) return;
  
  file.close(); 
  opened = false;
  
  std::list<Stream*>::iterator it;
  for( it = streams.begin(); it != streams.end(); ++it )
    delete *it;
}

unsigned long StorageIO::loadBigBlocks( std::vector<unsigned long> blocks,
  unsigned char* data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;
  if( !file.good() ) return 0;
  if( blocks.size() < 1 ) return 0;
  if( maxlen == 0 ) return 0;

  // read block one by one, seems fast enough
  unsigned long bytes = 0;
  for( unsigned long i=0; (i < blocks.size() ) & ( bytes<maxlen ); i++ )
  {
    unsigned long block = blocks[i];
    if( block < 0 ) continue;
    unsigned long pos =  bbat->blockSize * ( block+1 );
    unsigned long p = (bbat->blockSize < maxlen-bytes) ? bbat->blockSize : maxlen-bytes;
    if( pos + p > filesize ) p = filesize - pos;
    file.seekg( pos );
    file.read( (char*)data + bytes, p );
    bytes += p;
  }

  return bytes;
}

unsigned long StorageIO::loadBigBlock( unsigned long block,
  unsigned char* data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;
  if( !file.good() ) return 0;
  if( block < 0 ) return 0;

  // wraps call for loadBigBlocks
  std::vector<unsigned long> blocks;
  blocks.resize( 1 );
  blocks[ 0 ] = block;

  return loadBigBlocks( blocks, data, maxlen );
}

// return number of bytes which has been read
unsigned long StorageIO::loadSmallBlocks( std::vector<unsigned long> blocks,
  unsigned char* data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;
  if( !file.good() ) return 0;
  if( blocks.size() < 1 ) return 0;
  if( maxlen == 0 ) return 0;

  // our own local buffer
  unsigned char buf[ bbat->blockSize ];

  // read small block one by one
  unsigned long bytes = 0;
  for( unsigned long i=0; ( i<blocks.size() ) & ( bytes<maxlen ); i++ )
  {
    unsigned long block = blocks[i];
    if( block < 0 ) continue;

    // find where the small-block exactly is
    unsigned long pos = block * sbat->blockSize;
    unsigned long bbindex = pos / bbat->blockSize;
    if( bbindex >= sb_blocks.size() ) break;

    loadBigBlock( sb_blocks[ bbindex ], buf, bbat->blockSize );

    // copy the data
    unsigned offset = pos % bbat->blockSize;
    unsigned long p = (maxlen-bytes < bbat->blockSize-offset ) ? maxlen-bytes :  bbat->blockSize-offset;
    p = (sbat->blockSize<p ) ? sbat->blockSize : p;
    memcpy( data + bytes, buf + offset, p );
    bytes += p;
  }

  return bytes;
}

unsigned long StorageIO::loadSmallBlock( unsigned long block,
  unsigned char* data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;
  if( !file.good() ) return 0;
  if( block < 0 ) return 0;

  // wraps call for loadSmallBlocks
  std::vector<unsigned long> blocks;
  blocks.resize( 1 );
  blocks.assign( 1, block );

  return loadSmallBlocks( blocks, data, maxlen );
}

// =========== StreamImpl ==========

StreamImpl::StreamImpl( StorageIO* s, DirEntry* e)
{
  io = s;
  entry = e;
  m_pos = 0;

  if( entry->size >= io->header->threshold ) 
    blocks = io->bbat->follow( entry->start );
  else
    blocks = io->sbat->follow( entry->start );

  // prepare cache
  cache_pos = 0;
  cache_size = 4096; // optimal ?
  cache_data = new unsigned char[cache_size];
  updateCache();
}

// FIXME tell parent we're gone
StreamImpl::~StreamImpl()
{
  delete[] cache_data;  
}

void StreamImpl::seek( unsigned long pos )
{
  m_pos = pos;
}

unsigned long StreamImpl::tell()
{
  return m_pos;
}

int StreamImpl::getch()
{
  // past end-of-file ?
  if( m_pos > entry->size ) return -1;

  // need to update cache ?
  if( !cache_size || ( m_pos < cache_pos ) ||
    ( m_pos >= cache_pos + cache_size ) )
      updateCache();

  // something bad if we don't get good cache
  if( !cache_size ) return -1;

  int data = cache_data[m_pos - cache_pos];
  m_pos++;

  return data;
}

unsigned long StreamImpl::read( unsigned long pos, unsigned char* data, unsigned long maxlen )
{
  // sanity checks
  if( !data ) return 0;
  if( maxlen == 0 ) return 0;

  unsigned long totalbytes = 0;
  
  if ( entry->size < io->header->threshold )
  {
    // small file
    unsigned long index = pos / io->sbat->blockSize;

    if( index >= blocks.size() ) return 0;

    unsigned char buf[ io->sbat->blockSize ];
    unsigned long offset = pos % io->sbat->blockSize;
    while( totalbytes < maxlen )
    {
      if( index >= blocks.size() ) break;
      io->loadSmallBlock( blocks[index], buf, io->bbat->blockSize );
      unsigned long count = io->sbat->blockSize - offset;
      if( count > maxlen-totalbytes ) count = maxlen-totalbytes;
      memcpy( data+totalbytes, buf + offset, count );
      totalbytes += count;
      offset = 0;
      index++;
    }

  }
  else
  {
    // big file
    unsigned long index = pos / io->bbat->blockSize;
    
    if( index >= blocks.size() ) return 0;
    
    unsigned char buf[ io->bbat->blockSize ];
    unsigned long offset = pos % io->bbat->blockSize;
    while( totalbytes < maxlen )
    {
      if( index >= blocks.size() ) break;
      io->loadBigBlock( blocks[index], buf, io->bbat->blockSize );
      unsigned long count = io->bbat->blockSize - offset;
      if( count > maxlen-totalbytes ) count = maxlen-totalbytes;
      memcpy( data+totalbytes, buf + offset, count );
      totalbytes += count;
      index++;
      offset = 0;
    }

  }

  return totalbytes;
}

unsigned long StreamImpl::read( unsigned char* data, unsigned long maxlen )
{
  unsigned long bytes = read( tell(), data, maxlen );
  m_pos += bytes;
  return bytes;
}

void StreamImpl::updateCache()
{
  // sanity check
  if( !cache_data ) return;

  cache_pos = m_pos - ( m_pos % cache_size );
  unsigned long bytes = cache_size;
  if( cache_pos + bytes > entry->size ) bytes = entry->size - cache_pos;
  cache_size = read( cache_pos, cache_data, bytes );
}


// =========== Storage ==========

Storage::Storage( const char* filename )
{
  io = new StorageIO( this, filename );
}

Storage::~Storage()
{
  delete io;
}

int Storage::result()
{
  return io->result;
}

bool Storage::open()
{
  return io->open();
}

void Storage::close()
{
  io->close();
}

// list all files and subdirs in current path
std::list<std::string> Storage::listDirectory()
{
  std::list<std::string> result;

  std::vector<DirEntry*> entries;
  entries = io->dirtree->listDirectory();
  for( unsigned i = 0; i < entries.size(); i++ )
    result.push_back( entries[i]->name );
  
  return result;
}

// enters a sub-directory, returns false if not a directory or not found
bool Storage::enterDirectory( const std::string& directory )
{
  return io->dirtree->enterDirectory( directory );
}

// goes up one level (like cd ..)
void Storage::leaveDirectory()
{
  return io->dirtree->leaveDirectory();
}

std::string Storage::path()
{
  return io->dirtree->path();
}

Stream* Storage::stream( const std::string& name )
{
  // sanity check
  if( !name.length() ) return (Stream*)0;
  if( !io ) return (Stream*)0;

  // make absolute if necesary
  std::string fullName = name;
  if( name[0] != '/' ) fullName.insert( 0, path() + "/" );
  
  DirEntry* entry = io->dirtree->entry( name );
  if( !entry ) return (Stream*)0;

  Stream* s = new Stream();
  s->impl = new StreamImpl( io, entry );
  io->streams.push_back( s );
  
  return s;
}



// =========== Stream ==========

Stream::Stream()
{
  // just nullify, will be managed later Storage::stream
  impl = 0;
}

// FIXME tell parent we're gone
Stream::~Stream()
{
  delete impl;
}

unsigned long Stream::tell()
{
  return impl ? impl->tell() : 0;
}

void Stream::seek( unsigned long newpos )
{
  if( impl ) impl->seek( newpos );
}

unsigned long Stream::size()
{
  return impl ? impl->entry->size : 0;
}

int Stream::getch()
{
  return impl ? impl->getch() : 0;
}

unsigned long Stream::read( unsigned char* data, unsigned long maxlen )
{
  return impl ? impl->read( data, maxlen ) : 0;
}

