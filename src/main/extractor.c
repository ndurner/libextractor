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
 * How long do we allow an individual meta data object to be?
 * Used to guard against (broken) plugns causing us to use
 * excessive amounts of memory.
 */
#define MAX_META_DATA 32 * 1024 * 1024

/**
 * Maximum length of a Mime-Type string.
 */
#define MAX_MIME_LEN 256

/**
 * Maximum length of a shared memory object name
 */
#define MAX_SHM_NAME 255

/**
 * Set to 1 to get failure info,
 * 2 for actual debug info.
 */ 
#define DEBUG 1

/**
 * Sent from LE to a plugin to initialize it (open shm,
 * reset position counters etc).
 */
#define MESSAGE_INIT_STATE 0x01

/**
 * Sent from LE to a plugin to tell it that shm contents
 * were updated. Only used for OPMODE_COMPRESS.
 */
#define MESSAGE_UPDATED_SHM 0x02

/**
 * Sent from plugin to LE to tell LE that plugin is done
 * analyzing current file and will send no more data.
 */
#define MESSAGE_DONE 0x03

/**
 * Sent from plugin to LE to tell LE that plugin needs
 * to read a different part of the source file.
 */
#define MESSAGE_SEEK 0x04

/**
 * Sent from plugin to LE to tell LE about metadata discovered.
 */
#define MESSAGE_META 0x05

/**
 * Sent from LE to plugin to make plugin discard its state (unmap
 * and close shm).
 */
#define MESSAGE_DISCARD_STATE 0x06

/**
 * Client provided a memory buffer, analyze it. Creates a shm, copies
 * buffer contents into it. Does not support seeking (all data comes
 * in one [big] chunk.
 */
#define OPMODE_MEMORY 1

/**
 * Client provided a memory buffer or a file, which contains compressed data.
 * Creates a shm of limited size and repeatedly fills it with uncompressed
 * data. Never skips data (has to uncompress every byte, discards unwanted bytes),
 * can't efficiently seek backwards. Uses MESSAGE_UPDATED_SHM and MESSAGE_SEEK.
 */
#define OPMODE_DECOMPRESS 2

/**
 * Client provided a filename. Creates a file-backed shm (on W32) or just
 * communicates the file name to each plugin, and plugin opens its own file
 * descriptor of the file (POSIX). Each plugin maps different parts of the
 * file into its memory independently.
 */
#define OPMODE_FILE 3

/**
 * Header used for our IPC replies.  A header
 * with all fields being zero is used to indicate
 * the end of the stream.
 */
struct IpcHeader
{
  /**
   * Type of the meta data.
   */
  enum EXTRACTOR_MetaType meta_type;

  /**
   * Format of the meta data.
   */
  enum EXTRACTOR_MetaFormat meta_format;

  /**
   * Number of bytes of meta data (value)
   */
  size_t data_len;
  
  /**
   * Length of the mime type string describing the meta data value's mime type,
   * including 0-terminator, 0 for mime type of "NULL".
   */
  size_t mime_len;
};


/**
 * Writes 'size' bytes from 'buf' to 'fd', returns only when
 * writing is not possible, or when all 'size' bytes were written
 * (never does partial writes).
 *
 * @param fd fd to write into
 * @param buf buffer to read from
 * @param size number of bytes to write
 * @return number of bytes written (that is 'size'), or -1 on error
 */ 
static int
write_all (int fd,
	   const void *buf,
	   size_t size)
{
  const char *data = buf;
  size_t off = 0;
  ssize_t ret;
  
  while (off < size)
    {
      ret = write (fd, &data[off], size - off);
      if (ret <= 0)
	return -1;
      off += ret;
    }
  return size;
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
 * Main loop function for plugins.
 * Reads a message from the plugin input pipe and acts on it.
 * Can be called recursively (once) in OPMODE_DECOMPRESS.
 * plugin->waiting_for_update == 1 indicates the recursive call.
 *
 * @param plugin plugin context
 * @return 0, always
 */ 
static int
process_requests (struct EXTRACTOR_PluginList *plugin)
{
  int in, out;
  int read_result1, read_result2, read_result3, read_result4;
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

  in = plugin->pipe_in;
  out = plugin->cpipe_out;

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


/**
 * 'main' function of the child process. Loads the plugin,
 * sets up its in and out pipes, then runs the request serving function.
 *
 * @param plugin extractor plugin to use
 * @param in stream to read from
 * @param out stream to write to
 */
static void
plugin_main (struct EXTRACTOR_PluginList *plugin, 
	     int in, int out)
{
  if (plugin == NULL)
  {
    close (in);
    close (out);
    return;
  }
  if (0 != EXTRACTOR_plugin_load_ (plugin))
  {
    close (in);
    close (out);
#if DEBUG
    fprintf (stderr, "Plugin `%s' failed to load!\n", plugin->short_libname);
#endif
    return;
  }  
  if ((plugin->specials != NULL) &&
      (NULL != strstr (plugin->specials, "close-stderr")))
    close (2);
  if ((plugin->specials != NULL) &&
      (NULL != strstr (plugin->specials, "close-stdout")))
    close (1);

  plugin->pipe_in = in;
  /* Compiler will complain, and it's right. This is a kind of hack...*/
  plugin->cpipe_out = out;
  process_requests (plugin);

  close (in);
  close (out);
}


/**
 * Open a file
 */
static int 
file_open(const char *filename, int oflag, ...)
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
 * Receive a reply from plugin (seek request, metadata and done message)
 *
 * @param plugin plugin context
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return 0 on success, -1 on error
 */
static int
receive_reply (struct EXTRACTOR_PluginList *plugin,
	       EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
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
  plibc_init ("GNU", PACKAGE);
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
