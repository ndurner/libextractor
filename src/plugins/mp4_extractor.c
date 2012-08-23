/*
     This file is part of libextractor.
     (C) 2012 Christian Grothoff

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
 * @file plugins/mp4_extractor.c
 * @brief plugin to support MP4 files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <mp4v2/mp4v2.h>


/**
 * Callback invoked by libmp4v2 to open the file. 
 * We cheated and passed our extractor context as
 * the filename (fingers crossed) and will simply
 * return it again to make it the handle.
 *
 * @param name "filename" to open
 * @param open mode, only reading allowed
 * @return NULL if the file is not opened for reading
 */
static void*
open_cb (const char *name,
	 MP4FileMode mode)
{
  void *ecp;

  if (FILEMODE_READ != mode)
    return NULL;
  if (1 != sscanf (name, "%p", &ecp))
    return NULL;
  return ecp;
}


/**
 * Seek callback for libmp4v2.
 *
 * @param handle the 'struct EXTRACTOR_ExtractContext'
 * @param pos target seek position (relative or absolute?)
 * @return true on failure, false on success
 */
static int
seek_cb (void *handle,
	 int64_t pos)
{
  struct EXTRACTOR_ExtractContext *ec = handle;

  fprintf (stderr, "Seek: %lld!\n", (long long) pos);
  if (-1 == 
      ec->seek (ec->cls,
		pos,
		SEEK_CUR))
    return true; /* failure */
  return false;
}


/**
 * Read callback for libmp4v2.
 *
 * @param handle the 'struct EXTRACTOR_ExtractContext'
 * @param buffer where to write data read
 * @param size desired number of bytes to read
 * @param nin where to write number of bytes read
 * @param maxChunkSize some chunk size (ignored)
 * @return true on failure, false on success
 */
static int 
read_cb (void *handle,
	 void *buffer,
	 int64_t size,
	 int64_t *nin,
	 int64_t maxChunkSize)
{
  struct EXTRACTOR_ExtractContext *ec = handle;
  void *buf;
  ssize_t ret;
 
 fprintf (stderr, "read!\n");
  *nin = 0;
  if (-1 == 
      (ret = ec->read (ec->cls,
		       &buf,
		       size)))
    return true; /* failure */
  memcpy (buffer, buf, ret);
  *nin = ret;
  return false; /* success */
}


/**
 * Write callback for libmp4v2.
 *
 * @param handle the 'struct EXTRACTOR_ExtractContext'
 * @param buffer data to write
 * @param size desired number of bytes to write
 * @param nin where to write number of bytes written
 * @param maxChunkSize some chunk size (ignored)
 * @return true on failure (always fails)
 */
static int
write_cb (void *handle,
	  const void *buffer,
	  int64_t size,
	  int64_t *nout,
	  int64_t maxChunkSize)
{
  fprintf (stderr, "Write!?\n");
  return true; /* failure  */
}


/**
 * Write callback for libmp4v2.  Does nothing.
 *
 * @param handle the 'struct EXTRACTOR_ExtractContext'
 * @return false on success (always succeeds)
 */
static int
close_cb (void *handle)
{
  fprintf (stderr, "Close!\n");
  return false; /* success */
}


#if 0
/**
 * Wrapper to replace 'stat64' call by libmp4v2.
 */
int 
stat_cb (const char * path,
	 struct stat64 * buf) 
{
  void *ecp;
  struct EXTRACTOR_ExtractContext *ec;

  fprintf (stderr, "stat!\n");
  if (1 != sscanf (path, "%p", &ecp))
    {
      errno = EINVAL;
      return -1;
    }
  ec = ecp;
  memset (buf, 0, sizeof (struct stat));
  buf->st_size = ec->get_size (ec->cls);
  return 0;
}
#endif


/**
 * Main entry method for the MP4 extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void 
EXTRACTOR_mp4_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  MP4FileProvider fp;
  MP4FileHandle mp4;
  const MP4Tags *tags;
  char ecp[128];

  if (1)
    return; /* plugin is known not to work yet;
	       see issue 138 filed against MP4v2 lib */
  snprintf (ecp, sizeof (ecp), "%p", ec);
  fp.open = &open_cb;
  fp.seek = &seek_cb;
  fp.read = &read_cb;
  fp.write = &write_cb;
  fp.close = &close_cb;
  if (NULL == (mp4 = MP4ReadProvider (ecp,
				      &fp)))
    return;
  tags = MP4TagsAlloc ();
  if (MP4TagsFetch (tags, mp4))
    {
      fprintf (stderr, "got tags!\n");
    }
  MP4Close (mp4, 0);
}

/* end of mp4_extractor.c */
