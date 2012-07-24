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
 * @file main/extractor_plugin_main.c
 * @brief main loop for an out-of-process plugin
 * @author Christian Grothoff
 */

#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#include "extractor_datasource.h"
#include "extractor_plugins.h"
#include "extractor_ipc.h"
#include "extractor_plugin_main.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>


/**
 * Closure we use for processing requests inside the helper process.
 */
struct ProcessingContext 
{
  /**
   * Our plugin handle.
   */
  struct EXTRACTOR_PluginList *plugin;

  /**
   * Shared memory area.
   */
  void *shm;

  /**
   * Overall size of the file.
   */
  uint64_t file_size;

  /**
   * Current read offset when reading from the SHM.
   */
  uint64_t read_position;

  /**
   * Current offset of the SHM in the file.
   */
  uint64_t shm_off;

  /**
   * Handle to the shared memory.
   */
  int shm_id;
  
  /**
   * Size of the shared memory map.
   */
  uint32_t shm_map_size;

  /**
   * Number of bytes ready in SHM.
   */
  uint32_t shm_ready_bytes;

  /**
   * Input stream.
   */
  int in;
  
  /**
   * Output stream.
   */ 
  int out;
};



/**
 * Moves current absolute buffer position to @pos in @whence mode.
 * Will move logical position withouth shifting the buffer, if possible.
 * Will not move beyond the end of file.
 *
 * @param plugin plugin context
 * @param pos position to move to
 * @param whence seek mode (SEEK_CUR, SEEK_SET, SEEK_END)
 * @return new absolute position, -1 on error
 */
static int64_t
plugin_env_seek (void *cls,
		 int64_t pos, 
		 int whence)
{
  struct ProcessingContext *pc = cls;

  switch (whence)
  {
  case SEEK_CUR:
    if (plugin->shm_pos + pos < plugin->map_size && plugin->shm_pos + pos >= 0)
    {
      plugin->shm_pos += pos;
      return plugin->fpos + plugin->shm_pos;
    }
    if (0 != pl_pick_next_buffer_at (plugin, plugin->fpos + plugin->shm_pos + pos, 1))
      return -1;
    plugin->shm_pos += pos;
    return plugin->fpos + plugin->shm_pos;
    break;
  case SEEK_SET:
    if (pos < 0)
      return -1;
    if (pos >= plugin->fpos && pos < plugin->fpos + plugin->map_size)
    {
      plugin->shm_pos = pos - plugin->fpos;
      return pos;
    }
    if (0 != pl_pick_next_buffer_at (plugin, pos, 1))
      return -1;
    if (pos >= plugin->fpos && pos < plugin->fpos + plugin->map_size)
    {
      plugin->shm_pos = pos - plugin->fpos;
      return pos;
    }
    return -1;
    break;
  case SEEK_END:
    while (plugin->fsize == -1)
    {
      pl_pick_next_buffer_at (plugin, plugin->fpos + plugin->map_size + pos, 0);
    }
    if (plugin->fsize + pos - 1 < plugin->fpos || plugin->fsize + pos - 1 > plugin->fpos + plugin->map_size)
    {
      if (0 != pl_pick_next_buffer_at (plugin, plugin->fsize - MAX_READ, 0))
        return -1;
    }
    plugin->shm_pos = plugin->fsize + pos - plugin->fpos;
    if (plugin->shm_pos < 0)
      plugin->shm_pos = 0;
    else if (plugin->shm_pos >= plugin->map_size)
      plugin->shm_pos = plugin->map_size - 1;
    return plugin->fpos + plugin->shm_pos - 1;
    break;
  }
  return -1;
}


/**
 * Fills @data with a pointer to the data buffer.
 * Equivalent to read(), except you don't have to allocate and free
 * a buffer, since the data is already in memory.
 * Will move the buffer, if necessary
 *
 * @param plugin plugin context
 * @param data location to store data pointer
 * @param count number of bytes to read
 * @return number of bytes (<= count) avalable in @data, -1 on error
 */
static ssize_t
plugin_env_read (void *cls,
		 unsigned char **data, size_t count)
{
  struct ProcessingContext *pc = cls;
  
  *data = NULL;
  if (count > MAX_READ)
    return -1;
  if (count > plugin->map_size - plugin->shm_pos)
  {
    int64_t actual_count;
    if (plugin->fpos + plugin->shm_pos != pl_seek (plugin, plugin->fpos + plugin->shm_pos, SEEK_SET))
      return -1;
    *data = &plugin->shm_ptr[plugin->shm_pos];
    actual_count = (count < plugin->map_size - plugin->shm_pos) ? count : (plugin->map_size - plugin->shm_pos);
    plugin->shm_pos += actual_count;
    return actual_count;
  }
  else
  {
    *data = &plugin->shm_ptr[plugin->shm_pos];
    plugin->shm_pos += count;
    return count;
  }
}


static uint64_t
plugin_env_get_size (void *cls)
{
  struct ProcessingContext *pc = cls;
  return pc->file_size;
}


/**
 * Function called by a plugin in a child process.  Transmits
 * the meta data back to the parent process.
 *
 * @param cls closure, "struct ProcessingContext" with the FD for transmission
 * @param plugin_name name of the plugin that produced this value;
 *        special values can be used (i.e. '<zlib>' for zlib being
 *        used in the main libextractor library and yielding
 *        meta data).
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data 
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return 0 to continue extracting, 1 to abort (transmission error)
 */ 
static int
plugin_env_send_proc (void *cls,
		      const char *plugin_name,
		      enum EXTRACTOR_MetaType type,
		      enum EXTRACTOR_MetaFormat format,
		      const char *data_mime_type,
		      const char *data,
		      size_t data_len)
{
  struct ProcessingContext *pc = cls;
  static const unsigned char meta_byte = MESSAGE_META;
  int cpipe_out = pc->out;
  struct IpcHeader hdr;
  size_t mime_len;

  if (NULL == data_mime_type)
    mime_len = 0;
  else
    mime_len = strlen (data_mime_type) + 1;
  if (mime_len > MAX_MIME_LEN)
    mime_len = MAX_MIME_LEN;
  hdr.meta_type = type;
  hdr.meta_format = format;
  hdr.data_len = data_len;
  hdr.mime_len = mime_len;
  if ( (sizeof (meta_byte) != 
	write_all (*cpipe_out,
		   &meta_byte, sizeof (meta_byte))) ||
       (sizeof (hdr) != 
	write_all (*cpipe_out, 
		   &hdr, sizeof (hdr))) ||
       (mime_len !=
	write_all (*cpipe_out, 
		   data_mime_type, mime_len)) ||
       (data_len !=
	write_all (*cpipe_out, 
		   data, data_len)) )
    return 1;
  return 0;
}


/**
 * Handle an init message.  The opcode itself has already been read.
 *
 * @param pc processing context
 * @return 0 on success, -1 on error
 */
static int
handle_init_message (struct ProcessingContext *pc)
{
  struct InitMessage init;

  if (NULL != pc->shm)
    return -1;
  if (sizeof (struct InitMessage) - 1
      != read (pc->in,
	       &init.reserved,
	       sizeof (struct InitMessage) - 1))
    return -1;
  if (init.shm_name_length > MAX_SHM_NAME)
    return -1;
  {
    char shm_name[init.shm_name_length + 1];

    if (init.shm_name_length 
	!= read (pc->in,
		 shm_name,
		 init.shm_name_length))
      return -1;
    shm_name[init.shm_name_length] = '\0';

    pc->shm_map_size = init.shm_map_size;
#if WINDOWS
    pc->shm_ptr = MapViewOfFile (pc->shm_id, FILE_MAP_READ, 0, 0, 0);
    if (NULL == pc->shm_ptr)
      return -1;
#else
    pc->shm_id = open (shm_name, O_RDONLY, 0);
    if (-1 == pc->shm_id)
      return -1;
    pc->shm = mmap (NULL,
		    pc->shm_map_size,
		    PROT_READ,
		    MAP_SHARED,
		    pc->shm_id, 0);
    if ( ((void*) -1) == pc->shm)
      return -1;
#endif
  }
  return 0;
}


/**
 * Handle a start message.  The opcode itself has already been read.
 *
 * @param pc processing context
 * @return 0 on success, -1 on error
 */
static int
handle_start_message (struct ProcessingContext *pc)
{
  struct StartMessage start;
  struct EXTRACTOR_ExtractContext ec;

  if (sizeof (struct StartMessage) - 1
      != read (pc->in,
	       &start.reserved,
	       sizeof (struct StartMessage) - 1))
    return -1;
  pc->shm_ready_bytes = start.shm_ready_bytes;
  pc->file_size = start.shm_file_size;
  pc->read_position = 0;
  pc->shm_off = 0;
  ec.cls = pc;
  ec.config = pc->plugin->plugin_options;
  ec.read = &plugin_env_read;
  ec.seek = &plugin_env_seek;
  ec.get_size = &plugin_env_get_size;
  ec.proc = &plugin_env_send_proc;
  pc->plugin->extract_method (&ec);
}


/**
 * Main loop function for plugins.  Reads a message from the plugin
 * input pipe and acts on it.
 *
 * @param pc processing context
 */ 
static void
process_requests (struct ProcessingContext *pc)
{
  while (1)
    {
      unsigned char code;
  
      if (1 != read (pc->in, &code, 1))
	break;
      switch (code)
	{
	case MESSAGE_INIT_STATE:
	  if (0 != handle_init_message (pc))
	    return;
	  break;
	case MSG_EXTRACT_START:
	  if (0 != handle_start_message (pc))
	    return;
	case MSG_UPDATED_SHM:
	  /* not allowed here, we're not waiting for SHM to move! */
	  return;
	case MSG_DISCARD_STATE:
	  /* odd, we're already in the start state... */
	  continue;
	default:
	  /* error, unexpected message */
	  return;
	}
    }
}

#if 0
    case MESSAGE_UPDATED_SHM:
      if (plugin->operation_mode == OPMODE_DECOMPRESS)
      {
        read_result2 = read (in, &plugin->fpos, sizeof (int64_t));
        read_result3 = read (in, &plugin->map_size, sizeof (size_t));
        read_result4 = read (in, &plugin->fsize, sizeof (int64_t));
        if ((read_result2 < sizeof (int64_t)) || (read_result3 < sizeof (size_t)) ||
            plugin->fpos < 0 || (plugin->operation_mode != OPMODE_DECOMPRESS && (plugin->fsize <= 0 || plugin->fpos >= plugin->fsize)))
        {
          do_break = 1;
          break;
        }
        /* FIXME: also check mapped region size (lseek for *nix, VirtualQuery for W32) */
        /* Re-map the shm */
#if !WINDOWS
        if ((-1 == plugin->shm_id) ||
            (NULL == (plugin->shm_ptr = mmap (NULL, plugin->map_size, PROT_READ, MAP_SHARED, plugin->shm_id, 0))) ||
            (plugin->shm_ptr == (void *) -1))
        {
          do_break = 1;
          break;
        }
#else
        if ((plugin->map_handle == 0) ||
           (NULL == (plugin->shm_ptr = 
        {
          do_break = 1;
          break;
        }
#endif
        if (plugin->waiting_for_update == 1)
        {
          /* We were only waiting for this one message */
          do_break = 1;
          plugin->waiting_for_update = 2;
          break;
        }
        /* Run extractor on mapped region (recursive call doesn't reach this
         * point and breaks out earlier.
         */
        extract_reply = plugin->extract_method (plugin, transmit_reply, &out);
        /* Unmap the shm */
#if !WINDOWS
        if ((plugin->shm_ptr != NULL) &&
            (plugin->shm_ptr != (void*) -1) )
          munmap (plugin->shm_ptr, plugin->map_size);
#else
        if (plugin->shm_ptr != NULL)
          UnmapViewOfFile (plugin->shm_ptr);
#endif
        plugin->shm_ptr = NULL;
        if (extract_reply == 1)
        {
          /* Tell LE that we're done */
          unsigned char done_byte = MESSAGE_DONE;
          if (write (out, &done_byte, 1) != 1)
          {
            do_break = 1;
            break;
          }
          if ((plugin->specials != NULL) &&
              (NULL != strstr (plugin->specials, "force-kill")))
          {
            /* we're required to die after each file since this
               plugin only supports a single file at a time */
#if !WINDOWS
            fsync (out);
#else
            _commit (out);
#endif
            _exit (0);
          }
        }
        else
        {
          /* Tell LE that we're not done, and we need to seek */
          unsigned char seek_byte = MESSAGE_SEEK;
          if (write (out, &seek_byte, 1) != 1)
          {
            do_break = 1;
            break;
          }
          if (write (out, &plugin->seek_request, sizeof (int64_t)) != sizeof (int64_t))
          {
            do_break = 1;
            break;
          }
        }
      }
}
#endif


#ifndef WINDOWS
/**
 * Open '/dev/null' and make the result the given
 * file descriptor.
 *
 * @param target_fd desired FD to point to /dev/null
 * @param flags open flags (O_RDONLY, O_WRONLY)
 */
static void
open_dev_null (int target_fd,
	       int flags)
{
  int fd;

  fd = open ("/dev/null", flags);
  if (-1 == fd)
    return; /* good luck */
  if (fd == target_fd)
    return; /* already done */
  if (-1 == dup2 (fd, target_fd))
  {
    (void) close (fd);
    return; /* good luck */
  }
  /* close original result from 'open' */
  (void) close (fd);
}
#endif


/**
 * 'main' function of the child process. Loads the plugin,
 * sets up its in and out pipes, then runs the request serving function.
 *
 * @param plugin extractor plugin to use
 * @param in stream to read from
 * @param out stream to write to
 */
void
EXTRACTOR_plugin_main_ (struct EXTRACTOR_PluginList *plugin, 
			int in, int out)
{
  struct ProcessingContext pc;

  if (0 != EXTRACTOR_plugin_load_ (plugin))
    {
#if DEBUG
      fprintf (stderr, "Plugin `%s' failed to load!\n", 
	       plugin->short_libname);
#endif
      return;
    }  
  if ( (NULL != plugin->specials) &&
       (NULL != strstr (plugin->specials, "close-stderr")))
    {
      (void) close (2);
#ifndef WINDOWS
      open_dev_null (2, O_WRONLY);
#endif
    }
  if ( (NULL != plugin->specials) &&
       (NULL != strstr (plugin->specials, "close-stdout")))
    {
      (void) close (1);
#ifndef WINDOWS
      open_dev_null (1, O_WRONLY);
#endif
    }
  pc.plugin = plugin;
  pc.in = in;
  pc.out = out;
  pc.shm_id = -1;
  pc.shm = NULL;
  pc.shm_map_size = 0;
  process_requests (&pc);
#if WINDOWS
  if (NULL != pc.shm_ptr)
    UnmapViewOfFile (pc.shm_ptr);
#else
  if ( (NULL != pc.shm_ptr) &&
       (((void*) 1) != pc.shm_ptr) )
    munmap (pc.shm_ptr, pc.shm_map_size);
  if (-1 != pc.shm_id)
    (void) close (pc.shm_id);
#endif
}


#if WINDOWS
/**
 * Reads plugin data from the LE server process.
 * Also initializes allocation granularity (duh...).
 *
 * @param fd the pipe to read from
 * @return newly allocated plugin context
 */ 
static struct EXTRACTOR_PluginList *
read_plugin_data (int fd)
{
  struct EXTRACTOR_PluginList *ret;
  SYSTEM_INFO si;
  size_t i;

  if (NULL == (ret = malloc (sizeof (struct EXTRACTOR_PluginList))))
    return NULL;
  GetSystemInfo (&si);
  ret->allocation_granularity = si.dwAllocationGranularity;
  read (fd, &i, sizeof (size_t));
  if (NULL == (ret->libname = malloc (i)))
    {
      free (ret);
      return NULL;
    }
  read (fd, ret->libname, i);
  ret->libname[i - 1] = '\0';
  read (fd, &i, sizeof (size_t));
  if (NULL == (ret->short_libname = malloc (i)))
    {
      free (ret->libname);
      free (ret);
      return NULL;
    }
  read (fd, ret->short_libname, i);
  ret->short_libname[i - 1] = '\0';
  read (fd, &i, sizeof (size_t));
  if (0 == i)
    {
      ret->plugin_options = NULL;
      return ret;
    }
  if (NULL == (ret->plugin_options = malloc (i)))
    {
      free (ret->short_libname);
      free (ret->libname);
      free (ret);
      return NULL;
    }
  read (fd, ret->plugin_options, i);
  ret->plugin_options[i - 1] = '\0';
  return ret;
}


/**
 * FIXME: document.
 */
void CALLBACK 
RundllEntryPoint (HWND hwnd, 
		  HINSTANCE hinst, 
		  LPSTR lpszCmdLine, 
		  int nCmdShow)
{
  struct EXTRACTOR_PluginList *plugin;
  intptr_t in_h;
  intptr_t out_h;
  int in;
  int out;

  sscanf (lpszCmdLine, "%lu %lu", &in_h, &out_h);
  in = _open_osfhandle (in_h, _O_RDONLY);
  out = _open_osfhandle (out_h, 0);
  setmode (in, _O_BINARY);
  setmode (out, _O_BINARY);
  if (NULL == (plugin = read_plugin_data (in)))
    {
      close (in);
      close (out);
      return;
    }
  plugin_main (plugin,
	       in, out);
  close (in);
  close (out);
}


/**
 * FIXME: document.
 */
void CALLBACK 
RundllEntryPointA (HWND hwnd, 
		   HINSTANCE hinst, 
		   LPSTR lpszCmdLine, 
		   int nCmdShow)
{
  return RundllEntryPoint (hwnd, hinst, lpszCmdLine, nCmdShow);
}


#endif
