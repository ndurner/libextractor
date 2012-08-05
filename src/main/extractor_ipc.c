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
#include "extractor_logging.h"
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
  struct SeekRequestMessage seek;
  struct MetaMessage meta;
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
	  if (size < sizeof (struct SeekRequestMessage))
	    {
	      plugin->seek_request = -1;
	      return 0;
	    }
	  memcpy (&seek, cdata, sizeof (seek));
	  plugin->seek_request = (int64_t) seek.file_offset;
	  plugin->seek_whence = seek.whence;
	  return sizeof (struct SeekRequestMessage);
	case MESSAGE_META: /* Meta */
	  if (size < sizeof (struct MetaMessage))
	    {
	      plugin->seek_request = -1;
	      return 0;
	    }
	  memcpy (&meta, cdata, sizeof (meta));
	  /* check hdr for sanity */
	  if (meta.value_size > MAX_META_DATA)        
	    {
	      LOG ("Meta data exceeds size limit\n");
	      return -1; /* not allowing more than MAX_META_DATA meta data */
	    }
	  if (size < sizeof (meta) + meta.mime_length + meta.value_size)
	    { 
	      plugin->seek_request = -1;
	      return 0;
	    }
	  if (0 == meta.mime_length)
	    {
	      mime_type = NULL;
	    }
	  else
	    {
	      mime_type = &cdata[sizeof (struct MetaMessage)];
	      if ('\0' != mime_type[meta.mime_length - 1])
		{
		  LOG ("Mime type not 0-terminated\n");
		  return -1;		
		}
	    }
	  if (0 == meta.value_size)
	    value = NULL;
	  else
	    value = &cdata[sizeof (struct MetaMessage) + meta.mime_length];
          if (meta.meta_type >= EXTRACTOR_metatype_get_max ())
            meta.meta_type = EXTRACTOR_METATYPE_UNKNOWN;
	  proc (proc_cls, 
		plugin,
		(enum EXTRACTOR_MetaType) meta.meta_type,
		(enum EXTRACTOR_MetaFormat) meta.meta_format,
		mime_type, value, meta.value_size);
	  return sizeof (struct MetaMessage) + meta.mime_length + meta.value_size;
	default:
	  LOG ("Invalid message type %d\n", (int) code);
	  return -1;
	}
    }
  return 0;
}

/* end of extractor_ipc.c */
