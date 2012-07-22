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
#include "extractor_ipc.h"
#include "extractor_plugins.h"


/**
 * Process a reply from channel (seek request, metadata and done message)
 *
 * @param plugin plugin this communication is about
 * @param buf buffer with data from IPC channel
 * @param size number of bytes in buffer
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return number of bytes processed, -1 on error
 */
ssize_t
EXTRACTOR_IPC_process_reply_ (struct EXTRACTOR_PluginList *plugin,
			      const void *data,
			      size_t size,
			      EXTRACTOR_ChannelMessageProcessor proc,
			      void *proc_cls)
{
  const char *cdata = data;
  unsigned char code;
  int64_t seek_position;
  struct IpcHeader hdr;
  const char *mime_type;
  const char *value;

  while (size > 0)
    {
      code = (unsigned char) cdata[0];
      switch (code)
	{
	case MESSAGE_DONE: /* Done */
	  plugin->seek_request = -1;
	  plugin->round_finished = 1;
	  return 1;
	case MESSAGE_SEEK: /* Seek */
	  if (size < 1 + sizeof (int64_t))
	    {
	      plugin->seek_request = -1;
	      return 0;
	    }
	  memcpy (&seek_position, &cdata[1], sizeof (int64_t));
	  plugin->seek_request = seek_position;
	  return 1 + sizeof (int64_t);
	case MESSAGE_META: /* Meta */
	  if (size < 1 + sizeof (hdr) )
	    {
	      plugin->seek_request = -1;
	      return 0;
	    }
	  memcpy (&hdr, &cdata[1], sizeof (hdr));
	  /* check hdr for sanity */
	  if (hdr.data_len > MAX_META_DATA)
	    return -1; /* not allowing more than MAX_META_DATA meta data */
	  if (size < 1 + sizeof (hdr) + hdr.mime_len + hdr.data_len)
	    {
	      plugin->seek_request = -1;
	      return 0;
	    }
	  if (0 == hdr.mime_len)
	    {
	      mime_type = NULL;
	    }
	  else
	    {
	      mime_type = &cdata[1 + sizeof (hdr)];
	      if ('\0' != mime_type[hdr.mime_len-1])
		return -1;		
	    }
	  if (0 == hdr.data_len)
	    value = NULL;
	  else
	    value = &cdata[1 + sizeof (hdr) + hdr.mime_len];
	  proc (proc_cls, 
		plugin, 
		&hdr,
		mime_type, value);
	  return 1 + sizeof (hdr) + hdr.mime_len + hdr.data_len;
	default:
	  return -1;
	}
    }
  return 0;
}

/* end of extractor_ipc.c */
