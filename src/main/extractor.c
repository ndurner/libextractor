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


#if 0
/**
 * Open a file
 */
static int 
file_open (const char *filename, int oflag, ...)
{
  int mode;
  const char *fn;
#ifdef MINGW
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = plibc_conv_to_win_path(filename, szFile)) != ERROR_SUCCESS)
  {
    errno = ENOENT;
    SetLastError(lRet);
    return -1;
  }
  fn = szFile;
#else
  fn = filename;
#endif
  mode = 0;
#ifdef MINGW
  /* Set binary mode */
  mode |= O_BINARY;
#endif
  return OPEN(fn, oflag, mode);
}


/**
 * Initializes plugin state. Calls init_state_method()
 * directly or indirectly.
 *
 * @param plugin plugin to initialize
 * @param operation_mode operation mode
 * @param shm_name name of the shm/file
 * @param fsize file size (may be -1)
 */
static void
init_plugin_state (struct EXTRACTOR_PluginList *plugin, 
		   uint8_t operation_mode, 
		   const char *shm_name, int64_t fsize)
{
  int write_result;
  int init_state_size;
  unsigned char *init_state;
  int t;
  size_t shm_name_len = strlen (shm_name) + 1;
  
  init_state_size = 1 + sizeof (size_t) + shm_name_len + sizeof (uint8_t) + sizeof (int64_t);
  plugin->operation_mode = operation_mode;
  switch (plugin->flags)
  {
  case EXTRACTOR_OPTION_DEFAULT_POLICY:
  case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    init_state = malloc (init_state_size);
    if (init_state == NULL)
    {
      stop_process (plugin);
      return;
    }
    t = 0;
    init_state[t] = MESSAGE_INIT_STATE;
    t += 1;
    memcpy (&init_state[t], &operation_mode, sizeof (uint8_t));
    t += sizeof (uint8_t);
    memcpy (&init_state[t], &fsize, sizeof (int64_t));
    t += sizeof (int64_t);
    memcpy (&init_state[t], &shm_name_len, sizeof (size_t));
    t += sizeof (size_t);
    memcpy (&init_state[t], shm_name, shm_name_len);
    t += shm_name_len;
    write_result = plugin_write (plugin, init_state, init_state_size);
    free (init_state);
    if (write_result < init_state_size)
    {
      stop_process (plugin);
      return;
    }
    plugin->seek_request = 0;
    break;
  case EXTRACTOR_OPTION_IN_PROCESS:
    init_state_method (plugin, operation_mode, fsize, shm_name);
    return;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }
}


/**
 * Discards plugin state. Calls discard_state_method()
 * directly or indirectly.
 *
 * @param plugin plugin to initialize
 */
static void
discard_plugin_state (struct EXTRACTOR_PluginList *plugin)
{
  int write_result;
  unsigned char discard_state = MESSAGE_DISCARD_STATE;

  switch (plugin->flags)
  {
  case EXTRACTOR_OPTION_DEFAULT_POLICY:
  case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    /* This is somewhat clumsy, but it's the only stop-indicating
     * non-W32/POSIX-specific field i could think of...
     */
    if (plugin->cpipe_out != -1)
    {
      write_result = plugin_write (plugin, &discard_state, 1);
      if (write_result < 1)
      {
        stop_process (plugin);
        return;
      }
    }
    break;
  case EXTRACTOR_OPTION_IN_PROCESS:
    discard_state_method (plugin);
    return;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }
}


/**
 * Forces plugin to move the buffer window to 'pos'.
 *
 * @param plugin plugin context
 * @param pos position to move to
 * @param want_start 1 if the caller is interested in the beginning of the
 *        window, 0 if the caller is interested in its end. Window position
 *        must be aligned to page size, and this parameter controls the
 *        direction of window shift. 0 is used mostly by SEEK_END.
 * @return 0 on success, -1 on error
 */
static int
pl_pick_next_buffer_at (struct EXTRACTOR_PluginList *plugin,
			int64_t pos,
			uint8_t want_start)
{
  if (plugin->operation_mode == OPMODE_MEMORY)
  {
    int64_t old_pos;
    int64_t gran_fix;
#if !WINDOWS
    if (plugin->shm_ptr != NULL)
      munmap (plugin->shm_ptr, plugin->map_size);
#else
    if (plugin->shm_ptr != NULL)
      UnmapViewOfFile (plugin->shm_ptr);
#endif
    plugin->shm_ptr = NULL;
    old_pos = plugin->fpos + plugin->shm_pos;
    if (pos < 0)
      pos = 0;
    if (pos > plugin->fsize)
      pos = plugin->fsize - 1;
    plugin->fpos = pos;
    plugin->map_size = MAX_READ;
    plugin->shm_pos = old_pos - plugin->fpos;
    if (want_start)
      gran_fix = -1 * (plugin->fpos % plugin->allocation_granularity);
    else
    {
      gran_fix = plugin->fpos % plugin->allocation_granularity;
      if (gran_fix > 0)
        gran_fix = plugin->allocation_granularity - gran_fix;
    }
    if (plugin->fpos + gran_fix + plugin->map_size > plugin->fsize)
      plugin->map_size = plugin->fsize - plugin->fpos - gran_fix;
    plugin->fpos += gran_fix;
#if !WINDOWS
    if ((-1 == plugin->shm_id) ||
        (NULL == (plugin->shm_ptr = mmap (NULL, plugin->map_size, PROT_READ, MAP_SHARED, plugin->shm_id, plugin->fpos))) ||
        (plugin->shm_ptr == (void *) -1))
    {
      return -1;
    }
#else
    LARGE_INTEGER off;
    off.QuadPart = plugin->fpos;
    if ((plugin->map_handle == 0) ||
       (NULL == (plugin->shm_ptr = MapViewOfFile (plugin->map_handle, FILE_MAP_READ, off.HighPart, off.LowPart, plugin->map_size))))
    {
      DWORD err = GetLastError ();
      return -1;
    }
#endif
    plugin->shm_pos -= gran_fix;
    return 0;
  }
  if (plugin->operation_mode == OPMODE_FILE)
  {
    int64_t old_pos;
    int64_t gran_fix;
#if !WINDOWS
    if (plugin->shm_ptr != NULL)
      munmap (plugin->shm_ptr, plugin->map_size);
#else
    if (plugin->shm_ptr != NULL)
      UnmapViewOfFile (plugin->shm_ptr);
#endif
    plugin->shm_ptr = NULL;
    old_pos = plugin->fpos + plugin->shm_pos;
    if (pos < 0)
      pos = 0;
    if (pos > plugin->fsize)
      pos = plugin->fsize - 1;
    plugin->fpos = pos;
    plugin->map_size = MAX_READ;
    plugin->shm_pos = old_pos - plugin->fpos;
    if (want_start)
      gran_fix = -1 * (plugin->fpos % plugin->allocation_granularity);
    else
    {
      gran_fix = plugin->fpos % plugin->allocation_granularity;
      if (gran_fix > 0)
        gran_fix = plugin->allocation_granularity - gran_fix;
    }
    if (plugin->fpos + gran_fix + plugin->map_size > plugin->fsize)
      plugin->map_size = plugin->fsize - plugin->fpos - gran_fix;
    plugin->fpos += gran_fix;
#if !WINDOWS
    if ((-1 == plugin->shm_id) ||
        (NULL == (plugin->shm_ptr = mmap (NULL, plugin->map_size, PROT_READ, MAP_SHARED, plugin->shm_id, plugin->fpos))) ||
        (plugin->shm_ptr == (void *) -1))
    {
      return -1;
    }
#else
    LARGE_INTEGER off;
    off.QuadPart = plugin->fpos;
    if ((plugin->map_handle == 0) ||
       (NULL == (plugin->shm_ptr = MapViewOfFile (plugin->map_handle, FILE_MAP_READ, off.HighPart, off.LowPart, plugin->map_size))))
    {
      DWORD err = GetLastError ();
      return -1;
    }
#endif
    plugin->shm_pos -= gran_fix;
    return 0;
  }
  if (plugin->operation_mode == OPMODE_DECOMPRESS)
  {
    if (plugin->pipe_in != 0)
    {
      int64_t old_pos;
      old_pos = plugin->fpos + plugin->shm_pos;
      plugin->seek_request = pos;
      /* Recourse into request loop to wait for shm update */
      while (plugin->fpos != pos)
      {
        plugin->waiting_for_update = 1;
        if (process_requests (plugin) < 0)
          return -1;
        plugin->waiting_for_update = 0;
      }
      plugin->shm_pos = old_pos - plugin->fpos;
    }
    else
    {
      if (pos < plugin->fpos)
      {
        if (1 != cfs_reset_stream (plugin->pass_cfs))
          return -1;
      }
      while (plugin->fpos < pos && plugin->fpos >= 0)
        plugin->fpos = cfs_seek (plugin->pass_cfs, pos);
      plugin->fsize = ((struct CompressedFileSource *)plugin->pass_cfs)->uncompressed_size;
      plugin->shm_pos = pos - plugin->fpos;
    }
    return 0;
  }
  return -1;
}




/**
 * Transmits information about updated shm to plugin.
 * For OPMODE_DECOMPRESS only.
 *
 * @param plugin plugin context
 * @param position current absolute position in uncompressed stream
 * @param map_size number of bytes that are available in shm
 * @param fsize total size of the uncompressed stream (might be -1)
 * @param operation_mode mode of operation
 * @return 0 on success, 1 on error
 */
static int
give_shm_to_plugin (struct EXTRACTOR_PluginList *plugin, 
		    int64_t position,
		    size_t map_size, int64_t fsize, 
		    uint8_t operation_mode)
{
  int write_result;
  int updated_shm_size = 1 + sizeof (int64_t) + sizeof (size_t) + sizeof (int64_t);
  unsigned char updated_shm[updated_shm_size];
  int t = 0;
 
  updated_shm[t] = MESSAGE_UPDATED_SHM;
  t++;
  memcpy (&updated_shm[t], &position, sizeof (int64_t));
  t += sizeof (int64_t);
  memcpy (&updated_shm[t], &map_size, sizeof (size_t));
  t += sizeof (size_t);
  memcpy (&updated_shm[t], &fsize, sizeof (int64_t));
  t += sizeof (int64_t);
  switch (plugin->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
      if (operation_mode == OPMODE_DECOMPRESS)
	{
	  if (plugin->seek_request < 0)
	    return 0;
	  write_result = plugin_write (plugin, updated_shm, updated_shm_size);
	  if (write_result < updated_shm_size)
	    {
	      stop_process (plugin);
	      return 0;
	    }
	}
      return 1;
    case EXTRACTOR_OPTION_IN_PROCESS:
      if (operation_mode == OPMODE_DECOMPRESS)
	{
	  plugin->fpos = position;
	  plugin->map_size = map_size;
	  plugin->fsize = fsize;
	}
      return 0;
    case EXTRACTOR_OPTION_DISABLED:
      return 0;
    default:
      return 1;
    }
}


/**
 * Calls _extract_method of in-process plugin.
 *
 * @param plugin plugin context
 * @param shm_ptr pointer to the data buffer
 * @param proc metadata callback
 * @param proc_cls callback cls
 */
static void
ask_in_process_plugin (struct EXTRACTOR_PluginList *plugin,
		       void *shm_ptr,
		       EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int extract_reply;

  switch (plugin->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
      return;
    case EXTRACTOR_OPTION_IN_PROCESS:
      if (plugin->seek_request >= 0)
	{
	  plugin->shm_ptr = shm_ptr;
	  extract_reply = plugin->extract_method (plugin, proc, proc_cls);
	  /* Don't leak errno from the extract method */
	  errno = 0;
	  if (1 == extract_reply)
	    plugin->seek_request = -1;
	}
      break;
    case EXTRACTOR_OPTION_DISABLED:
      return;
      break;
    }
}





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


static void
load_in_process_plugin (struct EXTRACTOR_PluginList *plugin)
{
  switch (plugin->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    case EXTRACTOR_OPTION_DISABLED:
      break;
    case EXTRACTOR_OPTION_IN_PROCESS:
      EXTRACTOR_plugin_load_ (plugin);
      break;
    }
}
#endif




/**
 * Extract keywords using the given set of plugins.
 *
 * @param plugins the list of plugins to use
 * @param data data to process, or NULL if fds is not -1
 * @param fd file to read data from, or -1 if data is not NULL
 * @param filename name of the file to which fd belongs
 * @param cfs compressed file source for compressed stream (may be NULL)
 * @param fsize size of the file or data buffer
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
do_extract (struct EXTRACTOR_PluginList *plugins, 
	    const char *data, 
	    int fd,
	    const char *filename, 
	    struct CompressedFileSource *cfs, 
	    int64_t fsize, 
	    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int operation_mode;
  int plugin_count = 0;
  int shm_result;
  unsigned char *shm_ptr;
#if !WINDOWS
  int shm_id;
#else
  HANDLE map_handle;
#endif
  char shm_name[MAX_SHM_NAME + 1];

  struct EXTRACTOR_PluginList *ppos;

  int64_t position = 0;
  int64_t preserve = 0;
  size_t map_size;
  ssize_t read_result;
  int kill_plugins = 0;

  if (cfs != NULL)
    operation_mode = OPMODE_DECOMPRESS;
  else if (data != NULL)
    operation_mode = OPMODE_MEMORY;
  else if (fd != -1)
    operation_mode = OPMODE_FILE;
  else
    return;

  map_size = (fd == -1) ? fsize : MAX_READ;

  /* Make a shared memory object. Even if we're running in-process. Simpler that way.
   * This is only for reading-from-memory case. For reading-from-file we will use
   * the file itself; for uncompressing-on-the-fly the decompressor will make its own
   * shared memory object and uncompress into it directly.
   */
  if (operation_mode == OPMODE_MEMORY)
  {
    operation_mode = OPMODE_MEMORY;
#if !WINDOWS
    shm_result = make_shm_posix ((void **) &shm_ptr, &shm_id, shm_name, MAX_SHM_NAME,
        fsize);
#else  
    shm_result = make_shm_w32 ((void **) &shm_ptr, &map_handle, shm_name, MAX_SHM_NAME,
        fsize);
#endif
    if (shm_result != 0)
      return;
    memcpy (shm_ptr, data, fsize);
  }
  else if (operation_mode == OPMODE_FILE)
  {
#if WINDOWS
    shm_result = make_file_backed_shm_w32 (&map_handle, (HANDLE) _get_osfhandle (fd), shm_name, MAX_SHM_NAME);
    if (shm_result != 0)
      return;
#endif
  }

  /* This four-loops-instead-of-one construction is intended to increase parallelism */
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
  {
    start_process (ppos);
    plugin_count += 1;
  }

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    load_in_process_plugin (ppos);

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    write_plugin_data (ppos);

  if (operation_mode == OPMODE_DECOMPRESS)
  {
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      init_plugin_state (ppos, operation_mode, cfs->shm_name, -1);
  }
  else if (operation_mode == OPMODE_FILE)
  {
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
#if !WINDOWS
      init_plugin_state (ppos, operation_mode, filename, fsize);
#else
      init_plugin_state (ppos, operation_mode, shm_name, fsize);
#endif
  }
  else
  {
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      init_plugin_state (ppos, operation_mode, shm_name, fsize);
  }

  if (operation_mode == OPMODE_FILE || operation_mode == OPMODE_MEMORY)
  {
    int plugins_not_ready = 0;
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      plugins_not_ready += give_shm_to_plugin (ppos, position, map_size, fsize, operation_mode);
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      ask_in_process_plugin (ppos, shm_ptr, proc, proc_cls);
    while (plugins_not_ready > 0 && !kill_plugins)
    {
      int ready = wait_for_reply (plugins, proc, proc_cls);
      if (ready <= 0)
        kill_plugins = 1;
      plugins_not_ready -= ready;
    }
  }
  else
  {
    read_result = cfs_read (cfs, preserve);
    if (read_result > 0)
    while (1)
    {
      int plugins_not_ready = 0;

      map_size = cfs->shm_buf_size;
      for (ppos = plugins; NULL != ppos; ppos = ppos->next)
        plugins_not_ready += give_shm_to_plugin (ppos, position, map_size, cfs->uncompressed_size, operation_mode);
      /* Can't block in in-process plugins, unless we ONLY have one plugin */
      if (plugin_count == 1)
        for (ppos = plugins; NULL != ppos; ppos = ppos->next)
        {
          /* Pass this way. we'll need it to call cfs functions later on */
          /* This is a special case */
          ppos->pass_cfs = cfs;
          ask_in_process_plugin (ppos, cfs->shm_ptr, proc, proc_cls);
        }
      while (plugins_not_ready > 0 && !kill_plugins)
      {
        int ready = wait_for_reply (plugins, proc, proc_cls);
        if (ready <= 0)
          kill_plugins = 1;
        plugins_not_ready -= ready;
      }
      if (kill_plugins)
        break;
      position = seek_to_new_position (plugins, cfs, position, map_size);
      if (position < 0 || position == cfs->uncompressed_size)
        break;
    }
  }

  if (kill_plugins)
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      stop_process (ppos);
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    discard_plugin_state (ppos);

  if (operation_mode == OPMODE_MEMORY)
  {
#if WINDOWS
    destroy_shm_w32 (shm_ptr, map_handle);
#else
    destroy_shm_posix (shm_ptr, shm_id, (fd == -1) ? fsize : MAX_READ, shm_name);
#endif
  }
  else if (operation_mode == OPMODE_FILE)
  {
#if WINDOWS
    destroy_file_backed_shm_w32 (map_handle);
#endif
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

  if (NULL == filename)
    datasource = EXTRACTOR_datasource_create_from_buffer_ (data, size);
  else
    datasource = EXTRACTOR_datasource_create_from_file_ (filename);
  if (NULL == datasource)
    return;
  do_extract (plugins, datasource, proc, proc_cls);
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
  BINDTEXTDOMAIN ("iso-639", ISOLOCALEDIR); /* used by wordextractor */
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
