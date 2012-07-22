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
/**
 * @file main/extractor_datasource.c
 * @brief random access and possibly decompression of data from buffer in memory or file on disk
 * @author Christian Grothoff
 */

#include "platform.h"
#include "extractor_datasource.h"

#if HAVE_LIBBZ2
#include <bzlib.h>
#define MIN_BZ2_HEADER 4
#ifndef MIN_COMPRESSED_HEADER
#define MIN_COMPRESSED_HEADER MIN_ZLIB_HEADER
#endif
#endif

#if HAVE_ZLIB
#include <zlib.h>
#define MIN_ZLIB_HEADER 12
#ifndef MIN_COMPRESSED_HEADER
#define MIN_COMPRESSED_HEADER MIN_BZ2_HEADER
#endif
#endif

#ifndef MIN_COMPRESSED_HEADER
#define MIN_COMPRESSED_HEADER -1
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/**
 * Maximum size of an IO buffer.
 */
#define MAX_READ (4 * 1024 * 1024)

/**
 * Data is read from the source and shoved into decompressor
 * in chunks this big.
 */
#define COM_CHUNK_SIZE (10 * 1024)


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
   * Position of the buffer in the file.
   */ 
  uint64_t fpos;

  /**
   * Position within the buffer.
   */ 
  uint64_t buffer_pos;

  /**
   * Number of valid bytes in the buffer (<= buffer_size)
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
   * Decompression target buffer.
   */
  char result[COM_CHUNK_SIZE];

  /** 
   * At which offset in 'result' is 'fpos'?
   */
  size_t result_pos;

  /**
   * Size of the source (same as bfds->fsize)
   */ 
  int64_t fsize;

  /**
   * Position within the (decompressed) source
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
      bfds->buffer_pos = pos;
      return 0;
    }
  position = (int64_t) LSEEK (bfds->fd, pos, SEEK_SET);
  if (position < 0)
    return -1;
  bfds->fpos = position;
  bfds->buffer_pos = 0;
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
      if (bfds->fpos + bfds->buffer_pos + pos < 0)
	return -1;
      if (bfds->fpos + bfds->buffer_pos + pos > bfds->fsize)
	return -1;
      if ( (NULL == bfds->buffer) ||
	   ( (bfds->buffer_pos + pos < pos->buffer_bytes) &&
	     (bfds->buffer_pos + pos >= 0) ) )
	{
	  bfds->buffer_pos += pos; 
	  return bfds->buffer_pos;
	}
      if (0 != bfds_pick_next_buffer_at (bfds, 
					 bfds->fpos + bfds->buffer_pos + pos))
	return -1;
      return bfds->fpos;
    case SEEK_END:
      if (pos > 0)
	return -1;
      if (bfds->fsize < - pos)
	return -1;
      pos = bfds->fsize + pos;
      /* fall-through! */
    case SEEK_SET:
      if (pos < 0)
	return -1;
      if (pos > bfds->fsize)
	return -1;
      if ( (NULL == bfds->buffer) ||
	   ( (bfds->buffer_pos <= pos) &&
	     (bfds->buffer_pos + pos->buffer_bytes > pos) ) )
	{
	  bfds->buffer_pos = pos; 
	  return bfds->buffer_pos;
	}
      if (0 != bfds_pick_next_buffer_at (bfds, pos))
	return -1;
      return bfds->fpos;
    }
  return -1;
}


/**
 * Fills 'buf_ptr' with a chunk of data. Will
 * fail if 'count' exceeds buffer size.
 *
 * @param bfds bfds
 * @param buf_ptr location to store data 
 * @param count number of bytes to read
 * @return number of bytes (<= count) available at location pointed by buf_ptr,
 *         0 for end of stream, -1 on error
 */ 
static ssize_t
bfds_read (struct BufferedFileDataSource *bfds, 
	   void *buf_ptr, 
	   size_t count)
{
  char *cbuf = buf_ptr;
  uint64_t old_off;
  size_t avail;
  size_t ret;

  old_off = bfds->fpos + bfds->buffer_pos + bfds->buffer_bytes; 
  if (old_off == bfds->fsize)
    return 0; /* end of stream */
  ret = 0;
  while (count > 0)
    {
      if ( (bfds->buffer_bytes == bfds->buffer_pos) &&
	   (0 != bfds_pick_next_buffer_at (bfds, 
					   bfds->fpos + bfds->buffer_pos + bfds->buffer_bytes)) )
	{
	  /* revert to original position, invalidate buffer */
	  bfds->fpos = old_off;
	  bfds->buffer_bytes = 0;
	  bfds->buffer_pos = 0;
	  return -1; /* getting more failed */
	}
      avail = bfds->buffer_bytes - bfds->buffer_pos;
      if (avail > count)
	avail = count;
      if (0 == avail) 
	abort (); /* must not happen */
      memcpy (&cbuf[ret], &bfds->data[bfds->buffer_pos], avail);
      bfds->buffer_pos += avail;
      count -= avail;
      ret += avail;
    }
  return ret;
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
 * @return 1 on success, 0 to terminate extraction,
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
      - MAX_WBITS
#endif
      ))
    {
      return -1;
    }
  cfs->fpos = 0;
  cfs->shm_pos = 0;
  cfs->shm_buf_size = 0;
  return 1;
}


/**
 * Reset bz2-compressed data stream to the beginning.
 *
 * @return 1 on success, 0 to terminate extraction,
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
 * @return 1 on success, 0 to terminate extraction,
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
 * @return 1 on success, 0 to terminate extraction, -1 on error
 */
static int
cfs_init_decompressor_zlib (struct CompressedFileSource *cfs,
			    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  unsigned int gzip_header_length = 10;
  unsigned char hdata[12];

  /* Process gzip header */  
  if (sizeof (hdata) > bfds_read (cfs->bfds, hdata, sizeof (hdata)))
    return -1;
  if (0 != (hdata[3] & 0x4)) /* FEXTRA  set */
    gzip_header_length += 2 + (unsigned) (hdata[10] & 0xff) +
      (((unsigned) (hdata[11] & 0xff)) * 256);

  if (0 != (hdata[3] & 0x8)) 
    {
      /* FNAME set */
      char fname[1024];
      char *cptr;
      size_t len;
      ssize_t buf_bytes;
      
      if (gzip_header_length > bfds_seek (cfs->bfds, gzip_header_length, SEEK_SET))
	return -1;
      buf_bytes = bfds_read (cfs->bfds, fname, sizeof (fname));
      if (buf_bytes <= 0)
	return -1;
      if (NULL == (cptr = memchr (fname, 0, buf_bytes)))
	return -1;
      len = cptr - fname;
      if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_FILENAME,
		     EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
		     fname,
		     len))
	return 0; /* done */
      gzip_header_length += len + 1;
    }
  
  if (0 != (hdata[3] & 0x16)) 
    {
      /* FCOMMENT set */
      char fcomment[1024];
      char *cptr;
      ssize_t buf_bytes;
      size_t len;
      
      if (gzip_header_length > bfds_seek (cfs->bfds, gzip_header_length, SEEK_SET))
	return -1;
      buf_bytes = bfds_read (cfs->bfds, fcomment, sizeof (fcomment));
      if (buf_bytes <= 0)
	return -1;
      if (NULL == (cptr = memchr (fcomment, 0, buf_bytes)))
	return -1;
      len = cptr - fcomment;
      if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_COMMENT,
		     EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
		     (const char *) fcomment,
		     len))
	return 0; /* done */
      gzip_header_length += len + 1;
    }
  if (0 != (hdata[3] & 0x2)) /* FCHRC set */
    gzip_header_length += 2;
  memset (&cfs->strm, 0, sizeof (z_stream));
  
#ifdef ZLIB_VERNUM
  /* zlib will take care of its header */
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
 * @return 1 on success, 0 to terminate extraction, -1 on error
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
 * Destroy compressed file source.
 *
 * @param cfs source to destroy
 */
static void
cfs_destroy (struct CompressedFileSource *cfs)
{
  cfs_deinit_decompressor (cfs);
  free (cfs);
}


/**
 * Allocates and initializes new cfs object.
 *
 * @param bfds data source to use
 * @param fsize size of the source
 * @param compression_type type of compression used
 * @param proc metadata callback to call with meta data found upon opening
 * @param proc_cls callback cls
 * @return newly allocated cfs on success, NULL on error
 */
struct CompressedFileSource *
cfs_new (struct BufferedFileDataSource *bfds, 
	 int64_t fsize,
	 enum ExtractorCompressionType compression_type, 
	 EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  struct CompressedFileSource *cfs;

  if (NULL == (cfs = malloc (sizeof (struct CompressedFileSource))))
    return NULL;
  memset (cfs, 0, sizeof (struct CompressedFileSource));
  cfs->compression_type = compression_type;
  cfs->bfds = bfds;
  cfs->fsize = fsize;
  cfs->uncompressed_size = -1;
  if (1 != cfs_init_decompressor (cfs,
				  proc, proc_cls))
    {
      free (cfs);
      return NULL;
    }
  return cfs;
}


/**
 * Fills 'data' with new uncompressed data.  Does the actual
 * decompression. Will set uncompressed_size on the end of compressed
 * stream.
 *
 * @param cfds cfs to read from
 * @param data where to copy the data
 * @param size number of bytes available in data
 * @return number of bytes in data. 0 if no more data can be uncompressed, -1 on error
 */
static ssize_t
cfs_read_zlib (struct CompressedFileSource *cfs, 
	       void *data,
	       size_t size)
{
  char *dst = data;
  int ret;
  size_t rc;
  ssize_t in;
  char buf[COM_CHUNK_SIZE];

  if (cfs->fpos == cfs->uncompressed_size)
    return 0;
  rc = 0;
  if (strm.avail_out > 0)
    {
      /* got left-over decompressed data from previous round! */
      in = strm.avail_out;
      if (in > size)
	in = size;
      memcpy (&dst[rc], &cfs->result[cfs->result_pos], in);
      cfs->fpos += in;
      cfs->result_pos += in;
      rc += in;
    }
  ret = Z_OK;
  while ( (rc < size) && (Z_STREAM_END != ret) )
    {
      /* read block from original data source */
      in = bfds_read (cfs->bfds,
		      buf, sizeof (buf));
      if (in <= 0)
	return -1; /* unexpected EOF */
      cfs->strm.next_in = buf;
      cfs->strm.avail_in = (uInt) count;
      cfs->strm.next_out = cfs->result;
      cfs->strm.avail_out = COM_CHUNK_SIZE;
      cfs->result_pos = 0;
      ret = inflate (&cfs->strm, Z_SYNC_FLUSH);
      if ( (Z_OK != ret) && (Z_STREAM_END != ret) )
	return -1; /* unexpected error */
      /* go backwards by the number of bytes left in the buffer */
      if (-1 == bfds_seek (cfs->bfds, - cfs->strm.avail_in, SEEK_CUR))
	return -1;
      /* copy decompressed bytes to target buffer */
      in = cfs->strm.total_out;
      if (in > size - rc)
	in = size - rc;
      memcpy (&dst[rc], &cfs->result[cfs->result_pos], in);
      cfs->fpos += in;
      cfs->result_pos += in;
      rc += in;
    }
  if (Z_STREAM_END == ret)
    cfs->uncompressed_size = cfs->fpos;
  return rc;
}


/**
 * Fills 'data' with new uncompressed data.  Does the actual
 * decompression. Will set uncompressed_size on the end of compressed
 * stream.
 *
 * @param cfds cfs to read from
 * @param data where to copy the data
 * @param size number of bytes available in data
 * @return number of bytes in data. 0 if no more data can be uncompressed, -1 on error
 */
static ssize_t
cfs_read_bz2 (struct CompressedFileSource *cfs, 
	      void *data,
	      size_t size)
{
  return -1;
}


/**
 * Fills 'data' with new uncompressed data.  Does the actual
 * decompression. Will set uncompressed_size on the end of compressed
 * stream.
 *
 * @param cfds cfs to read from
 * @param data where to copy the data
 * @param size number of bytes available in data
 * @return number of bytes in data. 0 if no more data can be uncompressed, -1 on error
 */
static ssize_t
cfs_read (struct CompressedFileSource *cfs, 
	  void *data,
	  size_t size)
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
 * @param cfs cfs to seek on
 * @param position new starting point for the buffer
 * @return new absolute buffer position, -1 on error or EOS
 */
static int64_t
cfs_seek (struct CompressedFileSource *cfs, 
	  uint64_t position)
{
  int64_t delta;
  
  delta = position - cfs->fpos;
  if (delta < 0)
    {
      if (result_pos >= - delta)
	{
	  result_pos += delta;
	  delta = 0;
	}
      else
	{
	  if (-1 == cfs_reset_stream (cfs))
	    return -1;
	  delta = position;
	}
    }
  while (delta > 0)
    {
      char buf[COM_CHUNK_SIZE];
      size_t max;
      int64_t ret;
      
      max = (sizeof (buf) > delta) ? delta : sizeof (buf);
      ret = cfs_read (cfs, buf, max);
      if (-1 == ret)
	return -1;
      delta -= ret;      
    }
  return cfs->fpos;
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
get_compression_type (struct BufferedFileDataSource *bfds)
{
  unsigned char read_data[3];

  if (0 != bfds_seek (bfds, 0, SEEK_SET))
    return COMP_TYPE_INVALID;
  if (sizeof (read_data) !=
      bfds_read (bfds, read_data, sizeof (read_data)))
    return COMP_TYPE_UNDEFINED;

#if HAVE_ZLIB
  if ( (bdfs->fsize >= MIN_ZLIB_HEADER) && 
       (data[0] == 0x1f) && 
       (data[1] == 0x8b) && 
       (data[2] == 0x08) )
    return COMP_TYPE_ZLIB;
#endif
#if HAVE_LIBBZ2
  if ( (bdfs->fsize >= MIN_BZ2_HEADER) && 
       (data[0] == 'B') && 
       (data[1] == 'Z') && 
       (data[2] == 'h')) 
    return COMP_TYPE_BZ2;
#endif
  return COMP_TYPE_INVALID;
}


/**
 * Handle to a datasource we can use for the plugins.
 */ 
struct EXTRACTOR_Datasource
{

  /**
   * Underlying buffered data source.
   */
  struct BufferedFileDataSource *bfds;

  /**
   * Compressed file source (NULL if not applicable).
   */
  struct CompressedFileSource *cfs;

  /**
   * Underlying file descriptor, -1 for none.
   */
  int fd;
};


/**
 * Create a datasource from a file on disk.
 *
 * @param filename name of the file on disk
 * @param proc metadata callback to call with meta data found upon opening
 * @param proc_cls callback cls
 * @return handle to the datasource, NULL on error
 */
struct EXTRACTOR_Datasource *
EXTRACTOR_datasource_create_from_file_ (const char *filename,
					EXTRACTOR_MetaDataProcessor proc, 
					void *proc_cls)
{
  struct BufferedFileDataSource *bfds;
  struct EXTRACTOR_Datasource *ds;
  enum ExtractorCompressionType ct;
  int fd;
  struct stat sb;
  int64_t fsize;

  if (-1 == (fd = open (filename, O_RDONLY | O_LARGEFILE)))
    return NULL;
  if ( (0 != fstat (fd, &sb)) ||
       (S_ISDIR (fstatbuf.st_mode)) )       
    {
      (void) close (fd);
      return NULL;
    }
  fsize = (int64_t) sb.st_size;
  if (0 == fsize)
    {
      (void) close (fd);
      return NULL;
    }
  bfds = bfds_new (NULL, fd, fsize);
  if (NULL == bfds)
    {
      (void) close (fd);
      return NULL;
    }
  if (NULL == (ds = malloc (sizeof (struct EXTRACTOR_Datasource))))
    {
      bfds_delete (bfds);
      return NULL;
    }
  ds->bfds = bfds;
  ds->fd;
  ct = get_compression_type (bfds);
  if ( (COMP_TYPE_ZLIB == ct) ||
       (COMP_TYPE_BZ2 == ct) )
    ds->cfs = cfs_new (bfds, fsize, ct, proc, proc_cls);
  if (NULL == ds->cfs)
    {
      bfds_delete (bfds);
      free (ds);
      (void) close (fd);
      return NULL;
    }
  return ds;
}


/**
 * Create a datasource from a buffer in memory.
 *
 * @param buf data in memory
 * @param size number of bytes in 'buf'
 * @param proc metadata callback to call with meta data found upon opening
 * @param proc_cls callback cls
 * @return handle to the datasource
 */
struct EXTRACTOR_Datasource *
EXTRACTOR_datasource_create_from_buffer_ (const char *buf,
					  size_t size,
					  EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  struct BufferedFileDataSource *bfds;
  struct EXTRACTOR_Datasource *ds;
  enum ExtractorCompressionType ct;

  if (0 == size)
    return NULL;
  if (NULL == (bfds = bfds_new (buf, -1, size)))
    return NULL;
  if (NULL == (ds = malloc (sizeof (struct EXTRACTOR_Datasource))))
    {
      bfds_delete (bfds);
      return NULL;
    }
  ds->bfds = bfds;
  ds->fd;
  ct = get_compression_type (bfds);
  if ( (COMP_TYPE_ZLIB == ct) ||
       (COMP_TYPE_BZ2 == ct) )
    ds->cfs = cfs_new (bfds, fsize, ct, proc, proc_cls);
  if (NULL == ds->cfs)
    {
      bfds_delete (bfds);
      free (ds);
      return NULL;
    }
  return ds;
}


/**
 * Destroy a data source.
 *
 * @param ds source to destroy
 */
void
EXTRACTOR_datasource_destroy_ (struct EXTRACTOR_Datasource *ds)
{
  if (NULL != ds->cfs)
    cfs_destroy (ds->cfs);
  bfds_delete (ds->bfds);
  if (-1 != ds->fd)
    (void) close (ds->fd);
  free (ds);
}


/**
 * Make 'size' bytes of data from the data source available at 'data'.
 *
 * @param cls must be a 'struct EXTRACTOR_Datasource'
 * @param data where the data should be copied to
 * @param size maximum number of bytes requested
 * @return number of bytes now available in data (can be smaller than 'size'),
 *         -1 on error
 */
ssize_t
EXTRACTOR_datasource_read_ (void *cls,
			    void *data,
			    size_t size)
{
  struct EXTRACTOR_Datasource *ds = cls;

  if (NULL != ds->cfs)
    return cfs_read (ds->cfs, data, size);
  return bdfs_read (ds->bdfs, data, size);
}


/**
 * Seek in the datasource.  Use 'SEEK_CUR' for whence and 'pos' of 0 to
 * obtain the current position in the file.
 * 
 * @param cls must be a 'struct EXTRACTOR_Datasource'
 * @param pos position to seek (see 'man lseek')
 * @param whence how to see (absolute to start, relative, absolute to end)
 * @return new absolute position, UINT64_MAX on error (i.e. desired position
 *         does not exist)
 */
int64_t
EXTRACTOR_datasource_seek_ (void *cls,
			    uint64_t pos,
			    int whence)
{
  struct EXTRACTOR_Datasource *ds = cls;

  if (NULL != ds->cfs)
    return cfs_seek (ds->cfs, pos, whence);
  return bdfs_seek (ds->bdfs, pos, whence);
}


/**
 * Determine the overall size of the data source (after compression).
 * 
 * @param cls must be a 'struct EXTRACTOR_Datasource'
 * @return overall file size, UINT64_MAX on error or unknown
 */ 
int64_t 
EXTRACTOR_datasource_get_size_ (void *cls)
{
  struct EXTRACTOR_Datasource *ds = cls;

  if (NULL != ds->cfs)
    return cfs_seek (ds->cfs, pos, whence);
  return bdfs_seek (ds->bdfs, pos, whence);
}


/* end of extractor_datasource.c */
