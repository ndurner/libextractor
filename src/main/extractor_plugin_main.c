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
#include "extractor_ipc.h"
#include "extractor_plugin_main.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>



/**
 * Opens a file (for later mmapping).
 * This is POSIX variant of the plugin_open_* function.
 * Closes a file is already opened, closes it before opening a new one.
 * Destroy shared memory area.
 *
 * @param plugin plugin context
 * @param shm_name name of the file to open.
 * @return file id (-1 on error). That is, the result of open() syscall.
 */ 
static int
plugin_open_file (struct EXTRACTOR_PluginList *plugin, 
                 const char *shm_name)
{
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = open (shm_name, O_RDONLY, 0);
  return plugin->shm_id;
}


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
pl_seek (struct EXTRACTOR_PluginList *plugin, int64_t pos, int whence)
{
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


static int64_t
pl_get_fsize (struct EXTRACTOR_PluginList *plugin)
{
  return plugin->fsize;
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
static int64_t
pl_read (struct EXTRACTOR_PluginList *plugin, unsigned char **data, size_t count)
{
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


/**
 * Initializes an extracting session for a plugin.
 *   opens the file/shm (only in OPMODE_FILE)
 *   sets shm_ptr to NULL (unmaps it, if it was mapped)
 *   sets position to 0
 *   initializes file size to 'fsize' (may be -1)
 *   sets seek request to 0
 *
 * @param plugin plugin context
 * @param operation_mode the mode of operation (OPMODE_*)
 * @param fsize size of the source file (may be -1)
 * @param shm_name name of the shm or file to open
 * @return 0 on success, non-0 on error.
 */ 
static int
init_state_method (struct EXTRACTOR_PluginList *plugin, 
		   uint8_t operation_mode, 
		   int64_t fsize, 
		   const char *shm_name)
{
  plugin->seek_request = 0;
  if (plugin->shm_ptr != NULL)
    munmap (plugin->shm_ptr, plugin->map_size);
  plugin->shm_ptr = NULL;
  if (operation_mode == OPMODE_FILE)
  {
    if (-1 == plugin_open_file (plugin, shm_name))
      return 1;
  }
  else if (-1 == plugin_open_shm (plugin, shm_name))
    return 1;
  plugin->fsize = fsize;
  plugin->shm_pos = 0;
  plugin->fpos = 0;
  return 0;
}


/**
 * Function called by a plugin in a child process.  Transmits
 * the meta data back to the parent process.
 *
 * @param cls closure, "int*" of the FD for transmission
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
transmit_reply (void *cls,
		const char *plugin_name,
		enum EXTRACTOR_MetaType type,
		enum EXTRACTOR_MetaFormat format,
		const char *data_mime_type,
		const char *data,
		size_t data_len)
{
  static const unsigned char meta_byte = MESSAGE_META;
  int *cpipe_out = cls;
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
 * Main loop function for plugins.  Reads a message from the plugin
 * input pipe and acts on it.
 *
 * @param plugin plugin context
 * @param in input stream with incoming requests
 * @param out output stream for sending responses
 */ 
static void
process_requests (struct EXTRACTOR_PluginList *plugin,
		  int in,
		  int out)
{
  int read_result1;
  int read_result2;
  int read_result3;
  int read_result4;
  unsigned char code;
  char *shm_name = NULL;
  size_t shm_name_len;
  int extract_reply;
  struct IpcHeader hdr;
  int do_break;
#ifdef WINDOWS
  HANDLE map;
  MEMORY_BASIC_INFORMATION mi;
#endif

  /* The point of recursing into this function is to request
   * a seek from LE server and wait for a reply. This snipper
   * requests a seek.
   */
  if (plugin->waiting_for_update == 1)
  {
    unsigned char seek_byte = MESSAGE_SEEK;
    if (write (out, &seek_byte, 1) != 1)
      return -1;
    if (write (out, &plugin->seek_request, sizeof (int64_t)) != sizeof (int64_t))
      return -1;
  }

  memset (&hdr, 0, sizeof (hdr));
  do_break = 0;
  while (!do_break)
  {
    read_result1 = read (in, &code, 1);
    if (read_result1 <= 0)
      break;
    switch (code)
    {
    case MESSAGE_INIT_STATE:
      read_result2 = read (in, &plugin->operation_mode, sizeof (uint8_t));
      read_result3 = read (in, &plugin->fsize, sizeof (int64_t));
      read_result4 = read (in, &shm_name_len, sizeof (size_t));
      if ((read_result2 < sizeof (uint8_t)) ||
          (read_result3 < sizeof (int64_t)) ||
          (read_result4 < sizeof (size_t)))
      {
        do_break = 1;
        break;
      }
      if (plugin->operation_mode != OPMODE_MEMORY &&
          plugin->operation_mode != OPMODE_DECOMPRESS &&
          plugin->operation_mode != OPMODE_FILE)
      {
        do_break = 1;
        break;
      }
      if ((plugin->operation_mode == OPMODE_MEMORY ||
          plugin->operation_mode == OPMODE_DECOMPRESS) &&
          shm_name_len > MAX_SHM_NAME)
      {
        do_break = 1;
        break;
      }
      /* Fsize may be -1 only in decompression mode */
      if (plugin->operation_mode != OPMODE_DECOMPRESS && plugin->fsize <= 0)
      {
        do_break = 1;
        break;
      }
      if (shm_name != NULL)
        free (shm_name);
      shm_name = malloc (shm_name_len);
      if (shm_name == NULL)
      {
        do_break = 1;
        break;
      }
      read_result2 = read (in, shm_name, shm_name_len);
      if (read_result2 < shm_name_len)
      {
        do_break = 1;
        break;
      }
      shm_name[shm_name_len - 1] = '\0';
      do_break = init_state_method (plugin, plugin->operation_mode, plugin->fsize, shm_name);
      /* in OPMODE_MEMORY and OPMODE_FILE we can start extracting right away,
       * there won't be UPDATED_SHM message, and we don't need it
       */
      if (!do_break && (plugin->operation_mode == OPMODE_MEMORY ||
          plugin->operation_mode == OPMODE_FILE))
      {
        extract_reply = plugin->extract_method (plugin, transmit_reply, &out);
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
      break;
    case MESSAGE_DISCARD_STATE:
      discard_state_method (plugin);
      break;
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
           (NULL == (plugin->shm_ptr = MapViewOfFile (plugin->map_handle, FILE_MAP_READ, 0, 0, 0))))
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
      else
      {
        /* This is mostly to safely skip unrelated messages */
        int64_t t;
        size_t t2;
        read_result2 = read (in, &t, sizeof (int64_t));
        read_result3 = read (in, &t2, sizeof (size_t));
        read_result4 = read (in, &t, sizeof (int64_t));
      }
      break;
    }
  }
  return 0;
}


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
  process_requests (plugin, in, out);
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
