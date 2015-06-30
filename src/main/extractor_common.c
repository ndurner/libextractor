/*
     This file is part of libextractor.
     Copyright (C) 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */
/**
 * @file main/extractor_common.c
 * @brief commonly used functions within the library
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor_common.h"
#include "extractor_logging.h"
#include "extractor.h"
#include <sys/types.h>
#include <signal.h>


/**
 * Writes 'size' bytes from 'buf' to 'fd', returns only when
 * writing is not possible, or when all 'size' bytes were written
 * (never does partial writes).
 *
 * @param fd fd to write into
 * @param buf buffer to read from
 * @param size number of bytes to write
 * @return number of bytes written (that is 'size'), or -1 on error
 */ 
ssize_t
EXTRACTOR_write_all_ (int fd,
		      const void *buf,
		      size_t size)
{
  const char *data = buf;
  size_t off = 0;
  ssize_t ret;
  
  while (off < size)
    {
      ret = write (fd, &data[off], size - off);
      if (ret <= 0)
	{
	  if (-1 == ret)
	    LOG_STRERROR ("write");
	  return -1;
	}
      off += ret;
    }
  return size;
}


/**
 * Read a buffer from a given descriptor.
 *
 * @param fd descriptor to read from
 * @param buf buffer to fill
 * @param size number of bytes to read into 'buf'
 * @return -1 on error, size on success
 */
ssize_t
EXTRACTOR_read_all_ (int fd,
		     void *buf,
		     size_t size)
{
  char *data = buf;
  size_t off = 0;
  ssize_t ret;
  
  while (off < size)
    {
      ret = read (fd, &data[off], size - off);
      if (ret <= 0)
	{
	  if (-1 == ret)
	    LOG_STRERROR ("write");
	  return -1;
	}
      off += ret;
    }
  return size;
  
}



/* end of extractor_common.c */
