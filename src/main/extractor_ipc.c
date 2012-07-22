/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

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
 * @file main/extractor_ipc.c
 * @brief IPC with plugin (OS-independent parts)
 * @author Christian Grothoff
 */
#include "platform.h"


/**
 * Process a reply from channel (seek request, metadata and done message)
 *
 * @param buf buffer with data from IPC channel
 * @param size number of bytes in buffer
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return number of bytes processed, -1 on error
 */
ssize_t
EXTRACTOR_IPC_process_reply_ (const void *data,
			      size_t size,
			      EXTRACTOR_ChannelMessageProcessor proc,
			      void *proc_cls)
{
  int read_result;
  unsigned char code;
  int64_t seek_position;
  struct IpcHeader hdr;
  char *mime_type;
  char *data;
  int must_read = 1;

  while (must_read)
    {
      read_result = plugin_read (plugin, &code, 1);
      if (read_result < 1)
	return -1;
      switch (code)
	{
	case MESSAGE_DONE: /* Done */
	  plugin->seek_request = -1;
	  must_read = 0;
	  break;
	case MESSAGE_SEEK: /* Seek */
	  read_result = plugin_read (plugin, 
				     &seek_position, sizeof (int64_t));
	  if (read_result < sizeof (int64_t))
	    return -1;
	  plugin->seek_request = seek_position;
	  must_read = 0;
	  break;
	case MESSAGE_META: /* Meta */
	  read_result = plugin_read (plugin, 
				     &hdr, sizeof (hdr));
	  if (read_result < sizeof (hdr)) 
	    return -1;
	  /* FIXME: check hdr for sanity */
	  if (hdr.data_len > MAX_META_DATA)
	    return -1; /* not allowing more than MAX_META_DATA meta data */
	  if (0 == hdr.mime_len)
	    {
	      mime_type = NULL;
	    }
	  else
	    {
	      if (NULL == (mime_type = malloc (hdr.mime_len)))
		return -1;
	      read_result = plugin_read (plugin, 
					 mime_type, 
					 hdr.mime_len);
	      if ( (read_result < hdr.mime_len) ||
		   ('\0' != mime_type[hdr.mime_len-1]) )
		{
		  if (NULL != mime_type)
		    free (mime_type);
		  return -1;
		}
	    }
	  if (0 == hdr.data_len)
	    {
	      data = NULL;
	    }
	  else
	    {
	      if (NULL == (data = malloc (hdr.data_len)))
		{
		  if (NULL != mime_type)
		    free (mime_type);
		  return -1;
		}
	      read_result = plugin_read (plugin, 
					 data, hdr.data_len);
	      if (read_result < hdr.data_len)
		{
		  if (NULL != mime_type)
		    free (mime_type);
		  free (data);
		  return -1;
		}
	    }
	  read_result = proc (proc_cls, 
			      plugin->short_libname, 
			      hdr.meta_type, hdr.meta_format,
			      mime_type, data, hdr.data_len);
	  if (NULL != mime_type)
	    free (mime_type);
	  if (NULL != data)
	    free (data);
	  if (0 != read_result)
	    return 1;
	  break;
	default:
	  return -1;
	}
    }
  return 0;
}

/* end of extractor_ipc.c */
