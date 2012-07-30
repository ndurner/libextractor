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
#include "plibc.h"
#include "extractor.h"
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>
#include <ltdl.h>
#include "extractor_datasource.h"
#include "extractor_ipc.h"
#include "extractor_plugpath.h"
#include "extractor_plugins.h"


/**
 * Size used for the shared memory segment.
 */
#define DEFAULT_SHM_SIZE (16 * 1024)


/**
 * Closure for 'process_plugin_reply'
 */
struct PluginReplyProcessor
{
  /**
   * Function to call if we receive meta data from the plugin.
   */
  EXTRACTOR_MetaDataProcessor proc;

  /**
   * Closure for 'proc'.
   */
  void *proc_cls;

  /**
   * Are we done with processing this file? 0 to continue, 1 to terminate.
   */
  int file_finished;

};


/**
 * Send an 'update' message to the plugin.
 *
 * @param plugin plugin to notify
 * @param shm_off new offset for the SHM
 * @param data_available number of bytes available in shm
 * @param ds datastore backend we are using
 */
static void
send_update_message (struct EXTRACTOR_PluginList *plugin,
		     int64_t shm_off,
		     size_t data_available,
		     struct EXTRACTOR_Datasource *ds)
{
  struct UpdateMessage um;

  um.opcode = MESSAGE_UPDATED_SHM;
  um.reserved = 0;
  um.reserved2 = 0;
  um.shm_ready_bytes = (uint32_t) data_available;
  um.shm_off = (uint64_t) shm_off;
  um.file_size = EXTRACTOR_datasource_get_size_ (ds);
  if (sizeof (um) !=
      EXTRACTOR_IPC_channel_send_ (plugin->channel,
				   &um,
				   sizeof (um)) )
    {
      EXTRACTOR_IPC_channel_destroy_ (plugin->channel);
      plugin->channel = NULL;
      plugin->round_finished = 1;
    }  
}


/**
 * Send a 'discard state' message to the plugin and mark it as finished
 * for this round.
 *
 * @param plugin plugin to notify
 */
static void
send_discard_message (struct EXTRACTOR_PluginList *plugin)
{
  static unsigned char disc_msg = MESSAGE_DISCARD_STATE;

  if (sizeof (disc_msg) !=
      EXTRACTOR_IPC_channel_send_ (plugin->channel,
				   &disc_msg,
				   sizeof (disc_msg)) )
    {
      EXTRACTOR_IPC_channel_destroy_ (plugin->channel);
      plugin->channel = NULL;
      plugin->round_finished = 1;
    }
}


/**
 * We had some serious trouble.  Abort all channels.
 *
 * @param plugins list of plugins with channels to abort
 */
static void
abort_all_channels (struct EXTRACTOR_PluginList *plugins)
{
  struct EXTRACTOR_PluginList *pos;

  for (pos = plugins; NULL != pos; pos = pos->next)
    {
      if (NULL == pos->channel)
	continue;
      EXTRACTOR_IPC_channel_destroy_ (pos->channel);
      pos->channel = NULL;
    }
}


/**
 * Handler for a message from one of the plugins.
 *
 * @param cls closure with our 'struct PluginReplyProcessor'
 * @param plugin plugin of the channel sending the message
 * @param meta_type type of the meta data
 * @param meta_format format of the meta data
 * @param value_len number of bytes in 'value'
 * @param value 'data' send from the plugin
 * @param mime mime string send from the plugin
 */
static void
process_plugin_reply (void *cls,
		      struct EXTRACTOR_PluginList *plugin,
		      enum EXTRACTOR_MetaType meta_type,
		      enum EXTRACTOR_MetaFormat meta_format,
		      size_t value_len,
		      const void *value,
		      const char *mime)
{
  static unsigned char cont_msg = MESSAGE_CONTINUE_EXTRACTING;
  struct PluginReplyProcessor *prp = cls;

  if (0 != prp->file_finished)
    {
      /* client already aborted, ignore message, tell plugin about abort */
      return; 
    }
  if (0 != prp->proc (prp->proc_cls,
		      plugin->short_libname,
		      meta_type,
		      meta_format,
		      mime,
		      value,
		      value_len))
    {
      prp->file_finished = 1;
      send_discard_message (plugin);
      return;
    }
  if (sizeof (cont_msg) !=
      EXTRACTOR_IPC_channel_send_ (plugin->channel,
				   &cont_msg,
				   sizeof (cont_msg)) )
    {
      EXTRACTOR_IPC_channel_destroy_ (plugin->channel);
      plugin->channel = NULL;
      plugin->round_finished = 1;
    }
}


/**
 * Extract keywords using the given set of plugins.
 *
 * @param plugins the list of plugins to use
 * @param shm shared memory object used by the plugins (NULL if
 *        all plugins are in-process)
 * @param ds data to process
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
do_extract (struct EXTRACTOR_PluginList *plugins, 
	    struct EXTRACTOR_SharedMemory *shm,
	    struct EXTRACTOR_Datasource *ds,
	    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  unsigned int plugin_count;
  struct EXTRACTOR_PluginList *pos;
  struct StartMessage start;
  struct EXTRACTOR_Channel *channel;
  struct PluginReplyProcessor prp;
  int64_t min_seek;
  ssize_t data_available;
  uint32_t ready;
  int done;

  plugin_count = 0;
  for (pos = plugins; NULL != pos; pos = pos->next)
    plugin_count++;
  if (NULL != shm)
    ready = EXTRACTOR_IPC_shared_memory_set_ (shm, ds, 0, DEFAULT_SHM_SIZE);
  else
    ready = 0;

  prp.file_finished = 0;
  prp.proc = proc;
  prp.proc_cls = proc_cls;

  /* send 'start' message */
  start.opcode = MESSAGE_EXTRACT_START;
  start.reserved = 0;
  start.reserved2 = 0;
  start.shm_ready_bytes = ready;
  start.file_size = EXTRACTOR_datasource_get_size_ (ds);  
  for (pos = plugins; NULL != pos; pos = pos->next)
    {
      if ( (NULL != pos->channel) &&
	   (-1 == EXTRACTOR_IPC_channel_send_ (pos->channel,
					       &start,
					       sizeof (start)) ) )
	{
	  EXTRACTOR_IPC_channel_destroy_ (pos->channel);
	  pos->channel = NULL;
	}
    }
  done = 0;
  while (! done)
    {
      struct EXTRACTOR_Channel *channels[plugin_count];
      
      /* calculate current 'channels' array */
      plugin_count = 0;
      for (pos = plugins; NULL != pos; pos = pos->next)
	{
	  channels[plugin_count] = pos->channel;
	  plugin_count++;
	}
      
      /* give plugins chance to send us meta data, seek or finished messages */
      if (-1 == 
	  EXTRACTOR_IPC_channel_recv_ (channels,
				       plugin_count,
				       &process_plugin_reply,
				       &prp))
	{
	  /* serious problem in IPC; reset *all* channels */
	  abort_all_channels (plugins);
	  break;
	}
      /* calculate minimum seek request (or set done=0 to continue here) */
      done = 1;
      min_seek = -1;
      for (pos = plugins; NULL != pos; pos = pos->next)
	{
	  if ( (1 == pos->round_finished) ||
	       (NULL == pos->channel) )
	    continue; /* inactive plugin */
	  if (-1 == pos->seek_request)
	    {
	      done = 0; /* possibly more meta data at current position! */
	      break;
	    }
	  if ( (-1 == min_seek) ||
	       (min_seek > pos->seek_request) )
	    {
	      min_seek = pos->seek_request;
	    }
	}
      if ( (1 == done) &&
	   (-1 != min_seek) )
	{
	  /* current position done, but seek requested */
	  done = 0;
	  if (-1 ==
	      (data_available = EXTRACTOR_IPC_shared_memory_set_ (shm,
								  ds,
								  min_seek,
								  DEFAULT_SHM_SIZE)))
	    {
	      abort_all_channels (plugins);
	      break;
	    }
	}
      /* if 'prp.file_finished', send 'abort' to plugins;
	 if not, send 'seek' notification to plugins in range */
      for (pos = plugins; NULL != pos; pos = pos->next)
	{
	  if (NULL == (channel = channels[plugin_count]))
	    continue;
	  if ( (-1 != pos->seek_request) &&
	       (min_seek <= pos->seek_request) &&
	       (min_seek + data_available > pos->seek_request) )
	    {
	      send_update_message (pos,
				   min_seek,
				   data_available,
				   ds);
	      pos->seek_request = -1;
	    }
	  if ( (-1 != pos->seek_request) && 
	       (1 == prp.file_finished) )
	    {
	      send_discard_message (pos);
	      pos->round_finished = 1;
	    }	      
	  if (0 == pos->round_finished)
	    done = 0; /* can't be done, plugin still active */	
	}
    }

  /* run in-process plugins */
  for (pos = plugins; NULL != pos; pos = pos->next)
    {
      if (EXTRACTOR_OPTION_IN_PROCESS != pos->flags)
	continue;
      // FIXME: initialize read/seek context...
      // pos->extract_method (FIXME);
    }
}


/**
 * Extract keywords from a file using the given set of plugins.
 * If needed, opens the file and loads its data (via mmap).  Then
 * decompresses it if the data is compressed.  Finally runs the
 * plugins on the (now possibly decompressed) data.
 *
 * @param plugins the list of plugins to use
 * @param filename the name of the file, can be NULL if data is not NULL
 * @param data data of the file in memory, can be NULL (in which
 *        case libextractor will open file) if filename is not NULL
 * @param size number of bytes in data, ignored if data is NULL
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
void
EXTRACTOR_extract (struct EXTRACTOR_PluginList *plugins,
		   const char *filename,
		   const void *data,
		   size_t size,
		   EXTRACTOR_MetaDataProcessor proc,
		   void *proc_cls)
{
  struct EXTRACTOR_Datasource *datasource;
  struct EXTRACTOR_SharedMemory *shm;
  struct EXTRACTOR_PluginList *pos;

  if (NULL == plugins)
    return;
  if (NULL == filename)
    datasource = EXTRACTOR_datasource_create_from_buffer_ (data, size,
							   proc, proc_cls);
  else
    datasource = EXTRACTOR_datasource_create_from_file_ (filename,
							 proc, proc_cls);
  if (NULL == datasource)
    return;  
  shm = NULL;
  for (pos = plugins; NULL != pos; pos = pos->next)
    if (NULL != (shm = pos->shm))
      break;
  if (NULL == shm)
    shm = EXTRACTOR_IPC_shared_memory_create_ (DEFAULT_SHM_SIZE);
  for (pos = plugins; NULL != pos; pos = pos->next)
    if ( (NULL == pos->shm) &&
	 (EXTRACTOR_OPTION_IN_PROCESS == pos->flags) )
      {
	pos->shm = shm;
	(void) EXTRACTOR_IPC_shared_memory_change_rc_ (shm, 1);
	pos->channel = EXTRACTOR_IPC_channel_create_ (pos,
						      shm);
      }
  do_extract (plugins, shm, datasource, proc, proc_cls);
  EXTRACTOR_datasource_destroy_ (datasource);
}


/**
 * Initialize gettext and libltdl (and W32 if needed).
 */
void __attribute__ ((constructor)) 
EXTRACTOR_ltdl_init () 
{
  int err;

#if ENABLE_NLS
  BINDTEXTDOMAIN (PACKAGE, LOCALEDIR);
#endif
  err = lt_dlinit ();
  if (err > 0) 
    {
#if DEBUG
      fprintf (stderr,
	       _("Initialization of plugin mechanism failed: %s!\n"),
	       lt_dlerror ());
#endif
      return;
    }
#if WINDOWS
  plibc_init_utf8 ("GNU", PACKAGE, 1);
#endif
}


/**
 * Deinit.
 */
void __attribute__ ((destructor)) 
EXTRACTOR_ltdl_fini () {
#if WINDOWS
  plibc_shutdown ();
#endif
  lt_dlexit ();
}

/* end of extractor.c */
