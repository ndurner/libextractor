/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2005, 2006, 2009, 2012 Vidyut Samanta and Christian Grothoff

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

#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>
#include <ltdl.h>
#include "extractor_datasource.h"
#include "extractor_ipc.h"
#include "extractor_logging.h"
#include "extractor_plugpath.h"
#include "extractor_plugins.h"


/**
 * Size used for the shared memory segment.
 */
#define DEFAULT_SHM_SIZE (16 * 1024)


/**
 * Closure for #process_plugin_reply()
 */
struct PluginReplyProcessor
{
  /**
   * Function to call if we receive meta data from the plugin.
   */
  EXTRACTOR_MetaDataProcessor proc;

  /**
   * Closure for @e proc.
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
  um.file_size = EXTRACTOR_datasource_get_size_ (ds, 0);
  if (sizeof (um) !=
      EXTRACTOR_IPC_channel_send_ (plugin->channel,
				   &um,
				   sizeof (um)) )
    {
      LOG ("Failed to send UPDATED_SHM message to plugin\n");
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
      LOG ("Failed to send DISCARD_STATE message to plugin\n");
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
 * @param mime mime string send from the plugin
 * @param value 'data' send from the plugin
 * @param value_len number of bytes in 'value'
 */
static void
process_plugin_reply (void *cls,
		      struct EXTRACTOR_PluginList *plugin,
		      enum EXTRACTOR_MetaType meta_type,
		      enum EXTRACTOR_MetaFormat meta_format,
		      const char *mime,
		      const void *value,
                      size_t value_len)
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
#if DEBUG
      fprintf (stderr, "Sending ABRT\n");
#endif
      send_discard_message (plugin);
      return;
    }
  if (sizeof (cont_msg) !=
      EXTRACTOR_IPC_channel_send_ (plugin->channel,
				   &cont_msg,
				   sizeof (cont_msg)) )
    {
      LOG ("Failed to send CONTINUE_EXTRACTING message to plugin\n");
      EXTRACTOR_IPC_channel_destroy_ (plugin->channel);
      plugin->channel = NULL;
      plugin->round_finished = 1;
    }
}


/**
 * Closure for the in-process callbacks.
 */
struct InProcessContext
{
  /**
   * Current plugin.
   */
  struct EXTRACTOR_PluginList *plugin;

  /**
   * Data source to use.
   */
  struct EXTRACTOR_Datasource *ds;

  /**
   * Function to call with meta data.
   */
  EXTRACTOR_MetaDataProcessor proc;

  /**
   * Closure for 'proc'.
   */
  void *proc_cls;

  /**
   * IO buffer.
   */
  char buf[DEFAULT_SHM_SIZE];

  /**
   * 0 to continue extracting, 1 if we are finished
   */
  int finished;
};


/**
 * Obtain a pointer to up to 'size' bytes of data from the file to process.
 * Callback used for in-process plugins.
 *
 * @param cls a 'struct InProcessContext'
 * @param data pointer to set to the file data, set to NULL on error
 * @param size maximum number of bytes requested
 * @return number of bytes now available in data (can be smaller than 'size'),
 *         -1 on error
 *
 */
static ssize_t
in_process_read (void *cls,
		 void **data,
		 size_t size)
{
  struct InProcessContext *ctx = cls;
  ssize_t ret;
  size_t bsize;

  bsize = sizeof (ctx->buf);
  if (size < bsize)
    bsize = size;
  ret = EXTRACTOR_datasource_read_ (ctx->ds,
				    ctx->buf,
				    bsize);
  if (-1 == ret)
    *data = NULL;
  else
    *data = ctx->buf;
  return ret;
}


/**
 * Seek in the file.  Use 'SEEK_CUR' for whence and 'pos' of 0 to
 * obtain the current position in the file.
 * Callback used for in-process plugins.
 *
 * @param cls a 'struct InProcessContext'
 * @param pos position to seek (see 'man lseek')
 * @param whence how to see (absolute to start, relative, absolute to end)
 * @return new absolute position, -1 on error (i.e. desired position
 *         does not exist)
 */
static int64_t
in_process_seek (void *cls,
		 int64_t pos,
		 int whence)
{
  struct InProcessContext *ctx = cls;

  return EXTRACTOR_datasource_seek_ (ctx->ds,
				     pos,
				     whence);
}


/**
 * Determine the overall size of the file.
 * Callback used for in-process plugins.
 *
 * @param cls a `struct InProcessContext`
 * @return overall file size, UINT64_MAX on error (i.e. IPC failure)
 */
static uint64_t
in_process_get_size (void *cls)
{
  struct InProcessContext *ctx = cls;

  return (uint64_t) EXTRACTOR_datasource_get_size_ (ctx->ds, 0);
}


/**
 * Type of a function that libextractor calls for each
 * meta data item found.
 * Callback used for in-process plugins.
 *
 * @param cls a 'struct InProcessContext'
 * @param plugin_name name of the plugin that produced this value;
 *        special values can be used (i.e. '&lt;zlib&gt;' for zlib being
 *        used in the main libextractor library and yielding
 *        meta data).
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return 0 to continue extracting, 1 to abort
 */
static int
in_process_proc (void *cls,
		 const char *plugin_name,
		 enum EXTRACTOR_MetaType type,
		 enum EXTRACTOR_MetaFormat format,
		 const char *data_mime_type,
		 const char *data,
		 size_t data_len)
{
  struct InProcessContext *ctx = cls;
  int ret;

  if (0 != ctx->finished)
    return 1;
  ret = ctx->proc (ctx->proc_cls,
		   plugin_name,
		   type,
		   format,
		   data_mime_type,
		   data,
		   data_len);
  if (0 != ret)
    ctx->finished = 1;
  return ret;
}


/**
 * Extract keywords using the given set of plugins.
 *
 * @param plugins the list of plugins to use
 * @param shm shared memory object used by the plugins (NULL if
 *        all plugins are in-process)
 * @param ds data to process
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to @a proc
 */
static void
do_extract (struct EXTRACTOR_PluginList *plugins,
	    struct EXTRACTOR_SharedMemory *shm,
	    struct EXTRACTOR_Datasource *ds,
	    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  unsigned int plugin_count;
  unsigned int plugin_off;
  struct EXTRACTOR_PluginList *pos;
  struct StartMessage start;
  struct EXTRACTOR_Channel *channel;
  struct PluginReplyProcessor prp;
  struct InProcessContext ctx;
  struct EXTRACTOR_ExtractContext ec;
  int64_t min_seek;
  int64_t end;
  ssize_t data_available;
  ssize_t ready;
  int done;
  int have_in_memory;

  plugin_count = 0;
  for (pos = plugins; NULL != pos; pos = pos->next)
    plugin_count++;
  if (NULL != shm)
    ready = EXTRACTOR_IPC_shared_memory_set_ (shm,
                                              ds,
                                              0,
                                              DEFAULT_SHM_SIZE);
  else
    ready = 0;
  if (-1 == ready)
    return; /* failed to ready _any_ data!? */
  have_in_memory = 0;
  prp.file_finished = 0;
  prp.proc = proc;
  prp.proc_cls = proc_cls;

  /* send 'start' message */
  start.opcode = MESSAGE_EXTRACT_START;
  start.reserved = 0;
  start.reserved2 = 0;
  start.shm_ready_bytes = (uint32_t) ready;
  start.file_size = EXTRACTOR_datasource_get_size_ (ds, 0);
  for (pos = plugins; NULL != pos; pos = pos->next)
    {
      if (EXTRACTOR_OPTION_IN_PROCESS == pos->flags)
	have_in_memory = 1;
      if ( (NULL != pos->channel) &&
	   (-1 == EXTRACTOR_IPC_channel_send_ (pos->channel,
					       &start,
					       sizeof (start)) ) )
	{
	  LOG ("Failed to send EXTRACT_START message to plugin\n");
	  EXTRACTOR_IPC_channel_destroy_ (pos->channel);
	  pos->channel = NULL;
	}
    }
  if (-1 == ready)
    {
      LOG ("Failed to initialize IPC shared memory, cannot extract\n");
      done = 1;
    }
  else
    done = 0;
  while (! done)
    {
      struct EXTRACTOR_Channel *channels[plugin_count];

      /* calculate current 'channels' array */
      plugin_off = 0;
      for (pos = plugins; NULL != pos; pos = pos->next)
	{
	  if (-1 == pos->seek_request)
	    {
	      /* channel is not seeking, must be running or done */
	      channels[plugin_off] = pos->channel;
	    }
	  else
	    {
	      /* not running this round, seeking! */
	      channels[plugin_off] = NULL;
	    }
	  plugin_off++;
	}
      /* give plugins chance to send us meta data, seek or finished messages */
      if (-1 ==
	  EXTRACTOR_IPC_channel_recv_ (channels,
				       plugin_count,
				       &process_plugin_reply,
				       &prp))
	{
	  /* serious problem in IPC; reset *all* channels */
	  LOG ("Failed to receive message from channels; full reset\n");
	  abort_all_channels (plugins);
	  break;
	}

      /* calculate minimum seek request (or set done=0 to continue here) */
      done = 1;
      min_seek = -1;
      plugin_off = 0;
      for (pos = plugins; NULL != pos; pos = pos->next)
	{
	  plugin_off++;
	  if ( (1 == pos->round_finished) ||
	       (NULL == pos->channel) )
          {
            continue; /* inactive plugin */
          }
	  if (-1 == pos->seek_request)
	    {
	      /* possibly more meta data at current position, at least
		 this plugin is still working on it... */
	      done = 0;
	      break;
	    }
	  if (-1 != pos->seek_request)
	    {
	      if (SEEK_END == pos->seek_whence)
		{
		  /* convert distance from end to absolute position */
		  pos->seek_whence = 0;
		  end = EXTRACTOR_datasource_get_size_ (ds, 1);
		  if (pos->seek_request > end)
		    {
		      LOG ("Cannot seek to before the beginning of the file!\n");
		      pos->seek_request = 0;
		    }
		  else
		    {
		      pos->seek_request = end - pos->seek_request;
		    }
		}
	      if ( (-1 == min_seek) ||
		   (min_seek > pos->seek_request) )
		{
		  min_seek = pos->seek_request;
		}
	    }
	}
      data_available = -1;
      if ( (1 == done) &&
	   (-1 != min_seek) &&
           (NULL != shm) )
	{
	  /* current position done, but seek requested */
	  done = 0;
	  if (-1 ==
	      (data_available = EXTRACTOR_IPC_shared_memory_set_ (shm,
								  ds,
								  min_seek,
								  DEFAULT_SHM_SIZE)))
	    {
	      LOG ("Failed to seek; full reset\n");
	      abort_all_channels (plugins);
	      break;
	    }
	}
      /* if 'prp.file_finished', send 'abort' to plugins;
	 if not, send 'seek' notification to plugins in range */
      for (pos = plugins; NULL != pos; pos = pos->next)
	{
	  if (NULL == (channel = pos->channel))
	    {
	      /* Skipping plugin: channel down */
	      continue;
	    }
	  if ( (-1 != pos->seek_request) &&
	       (1 == prp.file_finished) )
	    {
	      send_discard_message (pos);
	      pos->round_finished = 1;
	      pos->seek_request = -1;
	    }
	  if ( (-1 != data_available) &&
	       (-1 != pos->seek_request) &&
	       (min_seek <= pos->seek_request) &&
	       ( (min_seek + data_available > pos->seek_request) ||
		 (min_seek == EXTRACTOR_datasource_get_size_ (ds, 0))) )
	    {
	      /* Notify plugin about seek to 'min_seek' */
	      send_update_message (pos,
				   min_seek,
				   data_available,
				   ds);
	      pos->seek_request = -1;
	    }
	  if (0 == pos->round_finished)
	    done = 0; /* can't be done, plugin still active */
	}
    }

  if (0 == have_in_memory)
    return;
  /* run in-process plugins */
  ctx.finished = 0;
  ctx.ds = ds;
  ctx.proc = proc;
  ctx.proc_cls = proc_cls;
  ec.cls = &ctx;
  ec.read = &in_process_read;
  ec.seek = &in_process_seek;
  ec.get_size = &in_process_get_size;
  ec.proc = &in_process_proc;
  for (pos = plugins; NULL != pos; pos = pos->next)
    {
      if (EXTRACTOR_OPTION_IN_PROCESS != pos->flags)
	continue;
      if (-1 == EXTRACTOR_plugin_load_ (pos))
        continue;
      ctx.plugin = pos;
      ec.config = pos->plugin_options;
      if (-1 == EXTRACTOR_datasource_seek_ (ds, 0, SEEK_SET))
	{
	  LOG ("Failed to seek to 0 for in-memory plugins\n");
	  return;
	}
      pos->extract_method (&ec);
      if (1 == ctx.finished)
	break;
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
 * @param proc_cls cls argument to @a proc
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
  int have_oop;

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
  have_oop = 0;
  for (pos = plugins; NULL != pos; pos = pos->next)
    {
      if (NULL == shm)
	shm = pos->shm;
      if (EXTRACTOR_OPTION_IN_PROCESS != pos->flags)
	have_oop = 1;
      pos->round_finished = 0;
    }
  if ( (NULL == shm) &&
       (1 == have_oop) )
    {
      /* need to create shared memory segment */
      shm = EXTRACTOR_IPC_shared_memory_create_ (DEFAULT_SHM_SIZE);
      if (NULL == shm)
	{
	  LOG ("Failed to setup IPC\n");
	  EXTRACTOR_datasource_destroy_ (datasource);
	  return;
	}
    }
  for (pos = plugins; NULL != pos; pos = pos->next)
    if ( (NULL == pos->channel) &&
         (NULL != shm) &&
	 (EXTRACTOR_OPTION_IN_PROCESS != pos->flags) )
      {
	if (NULL == pos->shm)
	  {
	    pos->shm = shm;
	    (void) EXTRACTOR_IPC_shared_memory_change_rc_ (shm, 1);
	  }
	pos->channel = EXTRACTOR_IPC_channel_create_ (pos,
						      shm);
      }
  do_extract (plugins,
              shm,
              datasource,
              proc,
              proc_cls);
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
  plibc_set_stat_size_size (sizeof (((struct stat *) 0)->st_size));
  plibc_set_stat_time_size (sizeof (((struct stat *) 0)->st_mtime));
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
