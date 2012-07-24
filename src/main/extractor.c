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


#if 0
/**
 * Checks the seek requests that plugins made, finds the one with
 * smallest offset from the beginning of the stream, and satisfies it.
 * 
 * @param plugins to check
 * @param cfs compressed file source to seek in
 * @param current_position current stream position
 * @param map_size number of bytes currently buffered
 * @return new stream position, -1 on error
 */
static int64_t
seek_to_new_position (struct EXTRACTOR_PluginList *plugins, 
		      struct CompressedFileSource *cfs, 
		      int64_t current_position,
		      int64_t map_size)
{
  int64_t min_pos = current_position + map_size;
  int64_t min_plugin_pos = 0x7FFFFFFFFFFFFFF;
  struct EXTRACTOR_PluginList *ppos;

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    {
      switch (ppos->flags)
	{
	case EXTRACTOR_OPTION_DEFAULT_POLICY:
	case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
	case EXTRACTOR_OPTION_IN_PROCESS:
	  if (ppos->seek_request >= 0 && ppos->seek_request <= min_pos)
	    min_pos = ppos->seek_request;
	  if (ppos->seek_request >= 0 && ppos->seek_request <= min_plugin_pos)
	    min_plugin_pos = ppos->seek_request;
	  break;
	case EXTRACTOR_OPTION_DISABLED:
	  break;
	}
    }
  if (min_plugin_pos == 0x7FFFFFFFFFFFFFF)
    return -1;
  if (min_pos < current_position - map_size)
    {
      if (1 != cfs_reset_stream (cfs))
	return -1;
      return 0;
    }
  return cfs_seek (cfs, min_pos);
}
#endif


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

};


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
  struct PluginReplyProcessor *prp = cls;

  // FIXME...
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
  uint32_t ready;
  int done;

  plugin_count = 0;
  for (pos = plugins; NULL != pos; pos = pos->next)
    plugin_count++;
  if (NULL != shm)
    ready = EXTRACTOR_IPC_shared_memory_set_ (shm, ds, 0, DEFAULT_SHM_SIZE);
  else
    ready = 0;

  prp.proc = proc;
  prp.proc_cls = proc_cls;

  /* send 'start' message */
  start.opcode = MESSAGE_EXTRACT_START;
  start.reserved = 0;
  start.reserved2 = 0;
  start.shm_ready_bytes = ready;
  start.file_size = EXTRACTOR_datasource_get_size_ (ds);
  {
    struct EXTRACTOR_Channel *channels[plugin_count];

    plugin_count = 0;
    for (pos = plugins; NULL != pos; pos = pos->next)
      {
	channels[plugin_count] = pos->channel;
	if ( (NULL != pos->channel) &&
	     (-1 == EXTRACTOR_IPC_channel_send_ (pos->channel,
						 &start,
						 sizeof (start)) ) )
	  {
	    channels[plugin_count] = NULL;
	    EXTRACTOR_IPC_channel_destroy_ (pos->channel);
	    pos->channel = NULL;
	  }
	plugin_count++;
      }
    done = 0;
    while (! done)
      {
	done = 1;

	// FIXME: need to handle 'seek' messages from plugins somewhere
	if (-1 == 
	    EXTRACTOR_IPC_channel_recv_ (channels,
					 plugin_count,
					 &process_plugin_reply,
					 &prp))
	  break;
	plugin_count = 0;
	for (pos = plugins; NULL != pos; pos = pos->next)
	  {
	    channel = channels[plugin_count];
	    // ... FIXME ...
	    plugin_count++;
	  }
	// FIXME: need to terminate once all plugins are done...
	done = 0;
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
