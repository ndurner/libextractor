/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006, 2009, 2012 Vidyut Samanta and Christian Grothoff

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

#include "platform.h"

#if HAVE_LIBBZ2
#include <bzlib.h>
#endif

#if HAVE_ZLIB
#include <zlib.h>
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/**
 * Maximum size of an IO buffer.
 */
#define MAX_READ (4 * 1024 * 1024)


#if HAVE_ZLIB
#define MIN_ZLIB_HEADER 12
#endif
#if HAVE_LIBBZ2
#define MIN_BZ2_HEADER 4
#endif
#if !defined (MIN_COMPRESSED_HEADER) && HAVE_ZLIB
#define MIN_COMPRESSED_HEADER MIN_ZLIB_HEADER
#endif
#if !defined (MIN_COMPRESSED_HEADER) && HAVE_LIBBZ2
#define MIN_COMPRESSED_HEADER MIN_BZ2_HEADER
#endif
#if !defined (MIN_COMPRESSED_HEADER)
#define MIN_COMPRESSED_HEADER -1
#endif

#define COMPRESSED_DATA_PROBE_SIZE 3

/**
 * Enum with the various possible types of compression supported.
 */ 
enum ExtractorCompressionType
{
  /**
   * We cannot tell from the data (header incomplete).
   */
  COMP_TYPE_UNDEFINED = -1,

  /**
   * Invalid header (likely uncompressed)
   */
  COMP_TYPE_INVALID = 0,

  /**
   * libz / gzip compression.
   */
  COMP_TYPE_ZLIB = 1,

  /**
   * bz2 compression
   */
  COMP_TYPE_BZ2 = 2
};


/**
 * Abstraction of the data source (file or a memory buffer)
 * for the decompressor.
 */ 
struct BufferedFileDataSource
{
  /**
   * Pointer to the buffer to read from (may be NULL)
   */ 
  const void *data;

  /**
   * A buffer to read into. For fd != -1: when data != NULL,
   * data is used directly.
   */ 
  void *buffer;

  /**
   * Size of the file (or the data buffer)
   */ 
  uint64_t fsize;

  /**
   * Position within the file or the data buffer
   */ 
  uint64_t fpos;

  /**
    * Position within the buffer.
   */ 
  uint64_t buffer_pos;

  /**
   * Number of bytes in the buffer (<= buffer_size)
   */ 
  uint64_t buffer_bytes;

  /**
   * Allocated size of the buffer
   */ 
  uint64_t buffer_size;

  /**
   * Descriptor of the file to read data from (may be -1)
   */ 
  int fd;

};


/**
 * An object from which uncompressed data can be read
 */ 
struct CompressedFileSource
{
  /**
   * The source of data
   */ 
  struct BufferedFileDataSource *bfds;

  /**
   * Size of the source (same as bfds->fsize)
   */ 
  int64_t fsize;

  /**
   * Position within the source
   */ 
  int64_t fpos;

  /**
   * Total size of the uncompressed data. Remains -1 until
   * decompression is finished.
   */ 
  int64_t uncompressed_size;

#if HAVE_LIBBZ2
  /**
   * BZ2 stream object
   */ 
  bz_stream bstrm;
#endif

#if HAVE_ZLIB
  /**
   * ZLIB stream object
   */ 
  z_stream strm;

  /**
   * Length of gzip header (may be 0, in that case ZLIB parses the header)
   */ 
  int gzip_header_length;
#endif

  /**
   * The type of compression used in the source
   */ 
  enum ExtractorCompressionType compression_type;

};


/**
 * Makes bfds seek to 'pos' and read a chunk of bytes there.
 * Changes bfds->fpos, bfds->buffer_bytes and bfds->buffer_pos.
 * Does almost nothing for memory-backed bfds.
 *
 * @param bfds bfds
 * @param pos position
 * @return 0 on success, -1 on error
 */ 
static int
bfds_pick_next_buffer_at (struct BufferedFileDataSource *bfds, 
			  uint64_t pos)
{
  int64_t position;
  ssize_t rd;
  
  if (pos > bfds->fsize)
    return -1; /* invalid */
  if (NULL == bfds->buffer)
    {
      bfds->buffer_bytes = bfds->fsize;
      return 0;
    }
#if WINDOWS
  position = _lseeki64 (bfds->fd, pos, SEEK_SET);
#elif HAVE_LSEEK64
  position = lseek64 (bfds->fd, pos, SEEK_SET);
#else
  position = (int64_t) lseek (bfds->fd, pos, SEEK_SET);
#endif
  if (position < 0)
    return -1;
  bfds->fpos = position;
  rd = read (bfds->fd, bfds->buffer, bfds->buffer_size);
  if (rd < 0)
    return -1;
  bfds->buffer_bytes = rd;
  return 0;
}


/**
 * Creates a bfds
 *
 * @param data data buffer to use as a source (NULL if fd != -1)
 * @param fd file descriptor to use as a source (-1 if data != NULL)
 * @param fsize size of the file (or the buffer)
 * @return newly allocated bfds
 */
static struct BufferedFileDataSource *
bfds_new (const void *data, 
	  int fd,
	  int64_t fsize)
{
  struct BufferedFileDataSource *result;
  size_t xtra;

  if (fsize > MAX_READ)
    xtra = MAX_READ;
  else
    xtra = (size_t) fsize;
  if ( (-1 == fd) && (NULL == data) )
    return NULL;
  if ( (-1 != fd) && (NULL != data) )
    fd = -1; /* don't need fd */
  if (NULL != data)
    xtra = 0;
  if (NULL == (result = malloc (sizeof (struct BufferedFileDataSource) + xtra)))
    return NULL;
  memset (result, 0, sizeof (struct BufferedFileDataSource));
  result->data = (NULL != data) ? data : &result[1];
  result->buffer = (NULL != data) ? NULL : &result[1];
  result->buffer_size = (NULL != data) ? fsize : xtra;
  result->fsize = fsize;
  result->fd = fd;
  bfds_pick_next_buffer_at (result, 0);
  return result;
}


/**
 * Unallocates bfds
 *
 * @param bfds bfds to deallocate
 */ 
static void
bfds_delete (struct BufferedFileDataSource *bfds)
{
  if (NULL != bfds->buffer)
    free (bfds->buffer);
  free (bfds);
}


/**
 * Makes bfds seek to 'pos' in 'whence' mode.
 * Will try to seek within the buffer, will move the buffer location if
 * the seek request falls outside of the buffer range.
 *
 * @param bfds bfds
 * @param pos position to seek to
 * @param whence one of the seek constants (SEEK_CUR, SEEK_SET, SEEK_END)
 * @return new absolute position, -1 on error
 */ 
static int64_t
bfds_seek (struct BufferedFileDataSource *bfds, 
	   int64_t pos, int whence)
{
  switch (whence)
    {
    case SEEK_CUR:
      if (NULL != bfds->buffer)
	{
	  if (0 != bfds_pick_next_buffer_at (bfds, 
					     bfds->fpos + bfds->buffer_pos + pos))
	    return -1;
	  bfds->buffer_pos = 0;
	  return bfds->fpos;
	}
      bfds->buffer_pos += pos; 
      return bfds->buffer_pos;
    case SEEK_SET:
      if (pos < 0)
	return -1;
      if (NULL != bfds->buffer)
	{
	  if (0 != bfds_pick_next_buffer_at (bfds, pos))
	    return -1;
	  bfds->buffer_pos = 0;
	  return bfds->fpos;
	}
      bfds->buffer_pos = pos; 
      return bfds->buffer_pos;
    case SEEK_END:
      if (NULL != bfds->buffer)
	{
	  if (0 != bfds_pick_next_buffer_at (bfds, bfds->fsize + pos))
	    return -1;
	  bfds->buffer_pos = 0;
	  return bfds->fpos;
	}
      bfds->buffer_pos = bfds->fsize + pos; 
      return bfds->buffer_pos;
    }
  return -1;
}


/**
 * Fills 'buf_ptr' with a chunk of data.
 * Will seek if necessary. Will fail if 'count' exceeds buffer size.
 *
 * @param bfds bfds
 * @param buf_ptr location to store data 
 * @param count number of bytes to read
 * @return number of bytes (<= count) available at location pointed by buf_ptr
 */ 
static ssize_t
bfds_read (struct BufferedFileDataSource *bfds, 
	   void *buf_ptr, 
	   size_t count)
{
  if (count > MAX_READ)
    return -1;
  if (count > bfds->buffer_bytes - bfds->buffer_pos)
    {
      if (bfds->fpos + bfds->buffer_pos != bfds_seek (bfds, bfds->fpos + bfds->buffer_pos, SEEK_SET))
	return -1;
      if (NULL != bfds->buffer)
	{
	  *buf_ptr = &bfds->buffer[bfds->buffer_pos];
	  bfds->buffer_pos += count < bfds->buffer_bytes ? count : bfds->buffer_bytes;
	  return (count < bfds->buffer_bytes ? count : bfds->buffer_bytes);
	}
      else
	{
	  int64_t ret = count < (bfds->buffer_bytes - bfds->buffer_pos) ? count : (bfds->buffer_bytes - bfds->buffer_pos);
	  *buf_ptr = (unsigned char*) &bfds->data[bfds->buffer_pos];
	  bfds->buffer_pos += ret;
	  return ret;
	}
    }
  else
    {
      if (NULL != bfds->buffer)
	*buf_ptr = &bfds->buffer[bfds->buffer_pos];
      else
	*buf_ptr = (unsigned char*) &bfds->data[bfds->buffer_pos];
      bfds->buffer_pos += count;
      return count;
    }
}


/**
 * Release resources of a compressed data source.
 *
 * @param cfs compressed data source to free
 */
static void
cfs_delete (struct CompressedFileSource *cfs)
{
  free (cfs);
}


/**
 * Reset gz-compressed data stream to the beginning.
 *
 * @return 1 on success, 0 if we failed to seek,
 *        -1 on decompressor initialization failure
 */ 
static int
cfs_reset_stream_zlib (struct CompressedFileSource *cfs)
{
  if (cfs->gzip_header_length != 
      bfds_seek (cfs->bfds, cfs->gzip_header_length, SEEK_SET))
    return 0;
  cfs->strm.next_in = NULL;
  cfs->strm.avail_in = 0;
  cfs->strm.total_in = 0;
  cfs->strm.zalloc = NULL;
  cfs->strm.zfree = NULL;
  cfs->strm.opaque = NULL;

  /*
   * note: maybe plain inflateInit(&strm) is adequate,
   * it looks more backward-compatible also ;
   *
   * ZLIB_VERNUM isn't defined by zlib version 1.1.4 ;
   * there might be a better check.
   */
  if (Z_OK != inflateInit2 (&cfs->strm,
#ifdef ZLIB_VERNUM
      15 + 32
#else
      -MAX_WBITS
#endif
      ))
    {
      return -1;
    }
  cfs->fpos = cfs->gzip_header_length;
  cfs->shm_pos = 0;
  cfs->shm_buf_size = 0;
  return 1;
}


/**
 * Reset bz2-compressed data stream to the beginning.
 *
 * @return 1 on success, 0 if we failed to seek,
 *        -1 on decompressor initialization failure
 */ 
static int
cfs_reset_stream_bz2 (struct CompressedFileSource *cfs)
{
  /* not implemented */
  return -1;
}


/**
 * Resets the compression stream to begin uncompressing
 * from the beginning. Used at initialization time, and when
 * seeking backward.
 *
 * @param cfs cfs to reset
 * @return 1 on success, , 0 if we failed to seek,
 *        -1 on error
 */
static int
cfs_reset_stream (struct CompressedFileSource *cfs)
{
  switch (cfs->compression_type)
  {
  case COMP_TYPE_ZLIB:
    return cfs_reset_stream_zlib (cfs);
  case COMP_TYPE_BZ2:
    return cfs_reset_stream_bz2 (cfs);
  default:
    return -1;
  }
}


/**
 * Initializes gz-decompression object. Might report metadata about
 * compresse stream, if available. Resets the stream to the beginning.
 *
 * @param cfs cfs to initialize
 * @param proc callback for metadata
 * @param proc_cls callback cls
 * @return 1 on success, -1 on error
 */
static int
cfs_init_decompressor_zlib (struct CompressedFileSource *cfs,
			    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  /* Process gzip header */
  unsigned int gzip_header_length = 10;
  unsigned char data[12];
  int64_t buf_bytes;
  int len;
  unsigned char *buf;
  unsigned char *cptr;
  
  if (sizeof (data) > bfds_read (cfs->bfds, data, sizeof (data)))
    return -1;
  
  if (0 != (data[3] & 0x4)) /* FEXTRA  set */
    gzip_header_length += 2 + (unsigned) (data[10] & 0xff) +
      (((unsigned) (data[11] & 0xff)) * 256);

  if (0 != (data[3] & 0x8)) /* FNAME set */
  {
    if (gzip_header_length > bfds_seek (cfs->bfds, gzip_header_length, SEEK_SET))
      return -1;
    buf_bytes = bfds_read (cfs->bfds, &buf, 1024);
    if (buf_bytes <= 0)
      return -1;
    cptr = buf;

    len = 0;
    /* stored file name is here */
    while (len < buf_bytes)
    {
      if ('\0' == *cptr)
      break;
      cptr++;
      len++;
    }

    if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_FILENAME,
		   EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
		   (const char *) buf,
		   len))
      return 0; /* done */

    /* FIXME: check for correctness */
    //gzip_header_length = (cptr - data) + 1;
    gzip_header_length += len + 1;
  }

  if (0 != (data[3] & 0x16)) /* FCOMMENT set */
  {
    int64_t buf_bytes;
    int len;
    unsigned char *buf;
    unsigned char *cptr;

    if (gzip_header_length > bfds_seek (cfs->bfds, gzip_header_length, SEEK_SET))
      return -1;
    buf_bytes = bfds_read (cfs->bfds, &buf, 1024);
    if (buf_bytes <= 0)
      return -1;
    cptr = buf;

    len = 0;
    /* stored file name is here */
    while (len < buf_bytes)
      {
	if ('\0' == *cptr)
	  break;
	cptr++;
	len++;
      }
    
    if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_COMMENT,
		   EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
		   (const char *) buf,
		   len))
      return 0; /* done */

    /* FIXME: check for correctness */
    //gzip_header_length = (cptr - data) + 1;
    gzip_header_length += len + 1;
  }

  if (data[3] & 0x2) /* FCHRC set */
    gzip_header_length += 2;

  memset (&cfs->strm, 0, sizeof (z_stream));

#ifdef ZLIB_VERNUM
  gzip_header_length = 0;
#endif

  cfs->gzip_header_length = gzip_header_length;
  return cfs_reset_stream_zlib (cfs);
}


/**
 * Initializes bz2-decompression object. Might report metadata about
 * compresse stream, if available. Resets the stream to the beginning.
 *
 * @param cfs cfs to initialize
 * @param proc callback for metadata
 * @param proc_cls callback cls
 * @return 1 on success, -1 on error
 */
static int
cfs_init_decompressor_bz2 (struct CompressedFileSource *cfs, 
			   EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  return -1;
}


/**
 * Initializes decompression object. Might report metadata about
 * compresse stream, if available. Resets the stream to the beginning.
 *
 * @param cfs cfs to initialize
 * @param proc callback for metadata
 * @param proc_cls callback cls
 * @return 1 on success, -1 on error
 */
static int
cfs_init_decompressor (struct CompressedFileSource *cfs, 
		       EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  switch (cfs->compression_type)
    {
    case COMP_TYPE_ZLIB:
      return cfs_init_decompressor_zlib (cfs, proc, proc_cls);
    case COMP_TYPE_BZ2:
      return cfs_init_decompressor_bz2 (cfs, proc, proc_cls);
    default:
      return -1;
    }
}


/**
 * Deinitializes gz-decompression object.
 *
 * @param cfs cfs to deinitialize
 * @return 1 on success, -1 on error
 */
static int
cfs_deinit_decompressor_zlib (struct CompressedFileSource *cfs)
{
  inflateEnd (&cfs->strm);
  return 1;
}


/**
 * Deinitializes bz2-decompression object.
 *
 * @param cfs cfs to deinitialize
 * @return 1 on success, -1 on error
 */
static int
cfs_deinit_decompressor_bz2 (struct CompressedFileSource *cfs)
{
  return -1;
}


/**
 * Deinitializes decompression object.
 *
 * @param cfs cfs to deinitialize
 * @return 1 on success, -1 on error
 */
static int
cfs_deinit_decompressor (struct CompressedFileSource *cfs)
{
  switch (cfs->compression_type)
    {
    case COMP_TYPE_ZLIB:
      return cfs_deinit_decompressor_zlib (cfs);
    case COMP_TYPE_BZ2:
      return cfs_deinit_decompressor_bz2 (cfs);
    default:
      return -1;
    }
}


/**
 * Allocates and initializes new cfs object.
 *
 * @param bfds data source to use
 * @param fsize size of the source
 * @param compression_type type of compression used
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return newly allocated cfs on success, NULL on error
 */
struct CompressedFileSource *
cfs_new (struct BufferedFileDataSource *bfds, 
	 int64_t fsize,
	 enum ExtractorCompressionType compression_type, 
	 EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int shm_result;
  struct CompressedFileSource *cfs;

  if (NULL == (cfs = malloc (sizeof (struct CompressedFileSource))))
    return NULL;
  memset (cfs, 0, sizeof (struct CompressedFileSource));
  cfs->compression_type = compression_type;
  cfs->bfds = bfds;
  cfs->fsize = fsize;
  cfs->uncompressed_size = -1;
  return cfs;
}


/**
 * Data is read from the source and shoved into decompressor
 * in chunks this big.
 */
#define COM_CHUNK_SIZE (10*1024)


/**
 * Re-fills shm with new uncompressed data, preserving the last
 * 'preserve' bytes of existing data as the first 'preserve' bytes
 * of the new data.
 * Does the actual decompression. Will set uncompressed_size on
 * the end of compressed stream.
 *
 * @param cfds cfs to read from
 * @param preserve number of bytes to preserve (0 to discard all old data)
 * @return number of bytes in shm. 0 if no more data can be uncompressed, -1 on error
 */
static int
cfs_read_zlib (struct CompressedFileSource *cfs, int64_t preserve)
{
  int ret;
  int64_t rc = preserve;
  int64_t total = cfs->strm.total_out;

  if (preserve > 0)
    memmove (cfs->shm_ptr, &((unsigned char *)cfs->shm_ptr)[0], preserve);

  while (rc < cfs->shm_size && ret != Z_STREAM_END)
  {
    if (cfs->strm.avail_in == 0)
    {
      int64_t count = bfds_read (cfs->bfds, &cfs->strm.next_in, COM_CHUNK_SIZE);
      if (count <= 0)
        return 0;
      cfs->strm.avail_in = (uInt) count;
    }
    cfs->strm.next_out = &((unsigned char *)cfs->shm_ptr)[rc];
    cfs->strm.avail_out = cfs->shm_size - rc;
    ret = inflate (&cfs->strm, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
      return 0;
    rc = cfs->strm.total_out - total;
  }
  if (ret == Z_STREAM_END)
    cfs->uncompressed_size = cfs->strm.total_out;
  cfs->shm_pos = preserve;
  cfs->shm_buf_size = rc + preserve;
  return 1;
}


/**
 * Re-fills shm with new uncompressed data, preserving the last
 * 'preserve' bytes of existing data as the first 'preserve' bytes
 * of the new data.
 * Does the actual decompression. Will set uncompressed_size on
 * the end of compressed stream.
 *
 * @param cfds cfs to read from
 * @param preserve number of bytes to preserve (0 to discard all old data)
 * @return number of bytes in shm. 0 if no more data can be uncompressed, -1 on error
 */
static int
cfs_read_bz2 (struct CompressedFileSource *cfs, int64_t preserve)
{
  return -1;
}


/**
 * Re-fills shm with new uncompressed data, preserving the last
 * 'preserve' bytes of existing data as the first 'preserve' bytes
 * of the new data.
 * Does the actual decompression. Will set uncompressed_size on
 * the end of compressed stream.
 *
 * @param cfds cfs to read from
 * @param preserve number of bytes to preserve (0 to discard all old data)
 * @return number of bytes in shm. 0 if no more data can be uncompressed, -1 on error
 */
static int64_t
cfs_read (struct CompressedFileSource *cfs, int64_t preserve)
{
  switch (cfs->compression_type)
    {
    case COMP_TYPE_ZLIB:
      return cfs_read_zlib (cfs, preserve);
    case COMP_TYPE_BZ2:
      return cfs_read_bz2 (cfs, preserve);
    default:
      return -1;
    }
}


/**
 * Moves the buffer to 'position' in uncompressed steam. If position
 * requires seeking backwards beyond the boundaries of the buffer, resets the
 * stream and repeats decompression from the beginning to 'position'.
 *
 * @param cfds cfs to seek on
 * @param position new starting point for the buffer
 * @return new absolute buffer position, -1 on error or EOS
 */
static int64_t
cfs_seek_zlib (struct CompressedFileSource *cfs, int64_t position)
{
  int64_t ret;

  if (position > cfs->strm.total_out - cfs->shm_buf_size && position < cfs->strm.total_out)
  {
    ret = cfs_read (cfs, cfs->strm.total_out - position);
    if (ret < 0)
      return ret;
    return position;
  }
  while (position >= cfs->strm.total_out)
  {
    if (0 > (ret = cfs_read (cfs, 0)))
      return ret;
    if (ret == 0)
      return position;
  }
  if (position < cfs->strm.total_out && position > cfs->strm.total_out - cfs->shm_buf_size)
    return cfs->strm.total_out - cfs->shm_buf_size;
  return -1;
}


/**
 * Moves the buffer to 'position' in uncompressed steam. If position
 * requires seeking backwards beyond the boundaries of the buffer, resets the
 * stream and repeats decompression from the beginning to 'position'.
 *
 * @param cfds cfs to seek on
 * @param position new starting point for the buffer
 * @return new absolute buffer position, -1 on error or EOS
 */
static int64_t
cfs_seek_bz2 (struct CompressedFileSource *cfs, int64_t position)
{
  return -1;
}


/**
 * Moves the buffer to 'position' in uncompressed steam. If position
 * requires seeking backwards beyond the boundaries of the buffer, resets the
 * stream and repeats decompression from the beginning to 'position'.
 *
 * @param cfds cfs to seek on
 * @param position new starting point for the buffer
 * @return new absolute buffer position, -1 on error or EOS
 */
static int64_t
cfs_seek (struct CompressedFileSource *cfs, int64_t position)
{
  switch (cfs->compression_type)
    {
    case COMP_TYPE_ZLIB:
      return cfs_seek_zlib (cfs, position);
    case COMP_TYPE_BZ2:
      return cfs_seek_bz2 (cfs, position);
    default:
      return -1;
    }
}


/**
 * Detect if we have compressed data on our hands.
 *
 * @param data pointer to a data buffer or NULL (in case fd is not -1)
 * @param fd a file to read data from, or -1 (if data is not NULL)
 * @param fsize size of data (if data is not NULL) or of file (if fd is not -1)
 * @return -1 to indicate an error, 0 to indicate uncompressed data, or a type (> 0) of compression
 */
static enum ExtractorCompressionType
get_compression_type (const unsigned char *data, 
		      int fd, 
		      int64_t fsize)
{
  void *read_data = NULL;
  size_t read_data_size = 0;
  ssize_t read_result;
  enum ExtractorCompressionType result = COMP_TYPE_INVALID;

  if ((MIN_COMPRESSED_HEADER < 0) || (fsize < MIN_COMPRESSED_HEADER))
  {
    return COMP_TYPE_INVALID;
  }
  if (data == NULL)
  {
    int64_t position;
    read_data_size = COMPRESSED_DATA_PROBE_SIZE;
    read_data = malloc (read_data_size);
    if (read_data == NULL)
      return -1;
#if WINDOWS
    position = _lseeki64 (fd, 0, SEEK_CUR);
#elif HAVE_LSEEK64
    position = lseek64 (fd, 0, SEEK_CUR);
#else
    position = (int64_t) lseek (fd, 0, SEEK_CUR);
#endif
    read_result = READ (fd, read_data, read_data_size);
#if WINDOWS
    position = _lseeki64 (fd, position, SEEK_SET);
#elif HAVE_LSEEK64
    position = lseek64 (fd, position, SEEK_SET);
#else
    position = lseek (fd, (off_t) position, SEEK_SET);
#endif
    if (read_result != read_data_size)
    {
      free (read_data);
      return COMP_TYPE_UNDEFINED;
    }
    data = (const void *) read_data;
  }
#if HAVE_ZLIB
  if ((fsize >= MIN_ZLIB_HEADER) && (data[0] == 0x1f) && (data[1] == 0x8b) && (data[2] == 0x08))
    result = COMP_TYPE_ZLIB;
#endif
#if HAVE_LIBBZ2
  if ((fsize >= MIN_BZ2_HEADER) && (data[0] == 'B') && (data[1] == 'Z') && (data[2] == 'h')) 
    result = COMP_TYPE_BZ2;
#endif
  if (read_data != NULL)
    free (read_data);
  return result;
}


#if 0

  enum ExtractorCompressionType compression_type = -1;
  struct CompressedFileSource *cfs = NULL;
  int fd = -1;
  struct stat64 fstatbuf;
  int64_t fsize = 0;

  /* If data is not given, then we need to read it from the file. Try opening it */
  if ((data == NULL) &&
      (filename != NULL) &&
      (0 == STAT64(filename, &fstatbuf)) &&
      (!S_ISDIR(fstatbuf.st_mode)) &&
      (-1 != (fd = file_open (filename,
             O_RDONLY | O_LARGEFILE))))
  {
    /* Empty files are of no interest */
    fsize = fstatbuf.st_size;
    if (fsize == 0) 
    {
       close(fd);
       return;
    }
  }

  /* Data is not given, and we've failed to open the file with data -> exit */
  if ((fsize == 0) && (data == NULL))
    return;
  /* fsize is now size of the data OR size of the file */
  if (data != NULL)
    fsize = size;

  errno = 0;

  /* Peek at first few bytes of the file (or of the data), and see if it's compressed. */
  compression_type = get_compression_type (data, fd, fsize);
  if (compression_type < 0)
  {
    /* errno is set by get_compression_type () */
    if (fd != -1)
      close (fd);
    return;
  }

  struct BufferedFileDataSource *bfds;
  bfds = bfds_new (data, fd, fsize);
  if (bfds == NULL)
    return;

  if (compression_type > 0)
  {
    int icr = 0;
    /* Set up a decompressor.
     * Will also report compression-related metadata to the caller.
     */
    cfs = cfs_new (bfds, fsize, compression_type, proc, proc_cls);
    if (cfs == NULL)
    {
      if (fd != -1)
        close (fd);
      errno = EILSEQ;
      return;
    }
    icr = cfs_init_decompressor (cfs, proc, proc_cls);
    if (icr < 0)
    {
      if (fd != -1)
        close (fd);
      errno = EILSEQ;
      return;
    }
    else if (icr == 0)
    {
      if (fd != -1)
        close (fd);
      errno = 0;
      return;
    }
  }


#endif



/**
 * Destroy a data source.
 *
 * @param datasource source to destroy
 */
void
EXTRACTOR_datasource_destroy_ (struct EXTRACTOR_Datasource *datasource)
{
  if (cfs != NULL)
  {
    cfs_deinit_decompressor (cfs);
    cfs_delete (cfs);
  }
  bfds_delete (bfds);
  if (-1 != fd)
    close(fd);
}

/* end of extractor_datasource.c */
