/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006, 2009 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
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
#if !WINDOWS
#include <sys/wait.h>
#include <sys/shm.h>
#endif
#include <signal.h>
#include <ltdl.h>

#if HAVE_LIBBZ2
#include <bzlib.h>
#endif

#if HAVE_ZLIB
#include <zlib.h>
#endif

#include "extractor_plugpath.h"
#include "extractor_plugins.h"


/**
 * How many bytes do we actually try to scan? (from the beginning
 * of the file).  Limit to 32 MB.
 */
#define MAX_READ 32 * 1024 * 1024

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
  enum EXTRACTOR_MetaType meta_type;
  enum EXTRACTOR_MetaFormat meta_format;
  size_t data_len;
  size_t mime_len;
};

#if !WINDOWS
/**
 * Opens a shared memory object (for later mmapping).
 * This is POSIX variant of the the plugin_open_* function. Shm is always memory-backed.
 * Closes a shm is already opened, closes it before opening a new one.
 *
 * @param plugin plugin context
 * @param shm_name name of the shm.
 * @return shm id (-1 on error). That is, the result of shm_open() syscall.
 */ 
static int
plugin_open_shm (struct EXTRACTOR_PluginList *plugin, const char *shm_name)
{
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = shm_open (shm_name, O_RDONLY, 0);
  return plugin->shm_id;
}

/**
 * Opens a file (for later mmapping).
 * This is POSIX variant of the plugin_open_* function.
 * Closes a file is already opened, closes it before opening a new one.
 *
 * @param plugin plugin context
 * @param shm_name name of the file to open.
 * @return file id (-1 on error). That is, the result of open() syscall.
 */ 
static int
plugin_open_file (struct EXTRACTOR_PluginList *plugin, const char *shm_name)
{
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = open (shm_name, O_RDONLY, 0);
  return plugin->shm_id;
}
#else
/**
 * Opens a shared memory object (for later mmapping).
 * This is W32  variant of the plugin_open_* function.
 * Opened shm might be memory-backed or file-backed (depending on how
 * it was created). shm_name is never a file name, unlike POSIX.
 * Closes a shm is already opened, closes it before opening a new one.
 *
 * @param plugin plugin context
 * @param shm_name name of the shared memory object.
 * @return memory-mapped file handle (NULL on error). That is, the result of OpenFileMapping() syscall.
 */ 
HANDLE
plugin_open_shm (struct EXTRACTOR_PluginList *plugin, const char *shm_name)
{
  if (plugin->map_handle != 0)
    CloseHandle (plugin->map_handle);
  plugin->map_handle = OpenFileMapping (FILE_MAP_READ, FALSE, shm_name);
  return plugin->map_handle;
}
/**
 * Another name for plugin_open_shm().
 */ 
HANDLE
plugin_open_file (struct EXTRACTOR_PluginList *plugin, const char *shm_name)
{
  return plugin_open_shm (plugin, shm_name);
}
#endif

/**
 * Writes @size bytes from @buf into @fd, returns only when
 * writing is not possible, or when all @size bytes were written
 * (never does partial writes).
 *
 * @param fd fd to write into
 * @param buf buffer to read from
 * @param size number of bytes to write
 * @return number of bytes written (that is - @size), or -1 on error
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
  int *cpipe_out = cls;
  struct IpcHeader hdr;
  size_t mime_len;
  unsigned char meta_byte = MESSAGE_META;
  unsigned char zero_byte = 0;

  if (data_mime_type == NULL)
    mime_len = 0;
  else
    mime_len = strlen (data_mime_type) + 1;
  if (mime_len > MAX_MIME_LEN)
    mime_len = MAX_MIME_LEN;
  hdr.meta_type = type;
  hdr.meta_format = format;
  hdr.data_len = data_len;
  hdr.mime_len = mime_len;
  if ((1 != write_all (*cpipe_out, &meta_byte, 1)) ||
      (sizeof(hdr) != write_all (*cpipe_out, &hdr, sizeof(hdr))) ||
      (mime_len -1 != write_all (*cpipe_out, data_mime_type, mime_len - 1)) ||
      (1 != write_all (*cpipe_out, &zero_byte, 1)) ||
      (data_len != write_all (*cpipe_out, data, data_len)))
    return 1;
  return 0;
}

/**
 * Initializes an extracting session for a plugin.
 *   opens the file/shm (only in OPMODE_FILE)
 *   sets shm_ptr to NULL (unmaps it, if it was mapped)
 *   sets position to 0
 *   initializes file size to @fsize (may be -1)
 *   sets seek request to 0
 *
 * @param plugin plugin context
 * @param operation_mode the mode of operation (OPMODE_*)
 * @param fsize size of the source file (may be -1)
 * @param shm_name name of the shm or file to open
 * @return 0 on success, non-0 on error.
 */ 
static int
init_state_method (struct EXTRACTOR_PluginList *plugin, uint8_t operation_mode, int64_t fsize, const char *shm_name)
{
  plugin->seek_request = 0;
#if !WINDOWS
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
#else
  if (plugin->shm_ptr != NULL)
    UnmapViewOfFile (plugin->shm_ptr);
  plugin->shm_ptr = NULL;
  if (INVALID_HANDLE_VALUE == plugin_open_shm (plugin, shm_name))
    return 1;
#endif
  plugin->fsize = fsize;
  plugin->shm_pos = 0;
  plugin->fpos = 0;
  return 0;
}

/**
 * Deinitializes an extracting session for a plugin.
 *   unmaps shm_ptr (if was mapped)
 *   closes file/shm (if it was opened)
 *   sets map size and shm_ptr to NULL.
 *
 * @param plugin plugin context
 */ 
static void
discard_state_method (struct EXTRACTOR_PluginList *plugin)
{
#if !WINDOWS
  if (plugin->shm_ptr != NULL && plugin->map_size > 0)
    munmap (plugin->shm_ptr, plugin->map_size);
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = -1;
#else
  if (plugin->shm_ptr != NULL)
    UnmapViewOfFile (plugin->shm_ptr);
  if (plugin->map_handle != 0)
    CloseHandle (plugin->map_handle);
  plugin->map_handle = 0;
#endif
  plugin->map_size = 0;
  plugin->shm_ptr = NULL;
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
  void *shm_ptr = NULL;
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
plugin_main (struct EXTRACTOR_PluginList *plugin, int in, int out)
{
  if (plugin == NULL)
  {
    close (in);
    close (out);
    return;
  }
  if (0 != plugin_load (plugin))
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

#if !WINDOWS

/**
 * Start the process for the given plugin.
 */ 
static void
start_process (struct EXTRACTOR_PluginList *plugin)
{
  int p1[2];
  int p2[2];
  pid_t pid;
  int status;

  switch (plugin->flags)
  {
  case EXTRACTOR_OPTION_DEFAULT_POLICY:
    if (-1 != plugin->cpid && 0 != plugin->cpid)
      return;
    break;
  case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    if (0 != plugin->cpid)
      return;
    break;
  case EXTRACTOR_OPTION_IN_PROCESS:
    return;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }

  plugin->cpid = -1;
  if (0 != pipe (p1))
  {
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }
  if (0 != pipe (p2))
  {
    close (p1[0]);
    close (p1[1]);
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }
  pid = fork ();
  plugin->cpid = pid;
  if (pid == -1)
  {
    close (p1[0]);
    close (p1[1]);
    close (p2[0]);
    close (p2[1]);
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }
  if (pid == 0)
  {
    close (p1[1]);
    close (p2[0]);
    plugin_main (plugin, p1[0], p2[1]);
    _exit (0);
  }
  close (p1[0]);
  close (p2[1]);
  plugin->cpipe_in = fdopen (p1[1], "w");
  if (plugin->cpipe_in == NULL)
  {
    perror ("fdopen");
    (void) kill (plugin->cpid, SIGKILL);
    waitpid (plugin->cpid, &status, 0);
    close (p1[1]);
    close (p2[0]);
    plugin->cpid = -1;
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }
  plugin->cpipe_out = p2[0];
}

/**
 * Stop the child process of this plugin.
 */
static void
stop_process (struct EXTRACTOR_PluginList *plugin)
{
  int status;

#if DEBUG
  if (plugin->cpid == -1)
    fprintf (stderr,
	     "Plugin `%s' choked on this input\n",
	     plugin->short_libname);
#endif
  if ( (plugin->cpid == -1) ||
       (plugin->cpid == 0) )
    return;
  kill (plugin->cpid, SIGKILL);
  waitpid (plugin->cpid, &status, 0);
  plugin->cpid = -1;
  close (plugin->cpipe_out);
  fclose (plugin->cpipe_in);
  plugin->cpipe_out = -1;
  plugin->cpipe_in = NULL;

  if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
    plugin->flags = EXTRACTOR_OPTION_DISABLED;

  plugin->seek_request = -1;
}

static int
write_plugin_data (const struct EXTRACTOR_PluginList *plugin)
{
  /* This function is only necessary on W32. On POSIX
   * systems plugin inherits its own data from the parent */
  return 0;
}

#define plugin_write(plug, buf, size) write_all (fileno (plug->cpipe_in), buf, size)

#else /* WINDOWS */

#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

/* Copyright Bob Byrnes  <byrnes <at> curl.com>
   http://permalink.gmane.org/gmane.os.cygwin.patches/2121
*/
/* Create a pipe, and return handles to the read and write ends,
   just like CreatePipe, but ensure that the write end permits
   FILE_READ_ATTRIBUTES access, on later versions of win32 where
   this is supported.  This access is needed by NtQueryInformationFile,
   which is used to implement select and nonblocking writes.
   Note that the return value is either NO_ERROR or GetLastError,
   unlike CreatePipe, which returns a bool for success or failure.  */
static int
create_selectable_pipe (PHANDLE read_pipe_ptr, PHANDLE write_pipe_ptr,
                        LPSECURITY_ATTRIBUTES sa_ptr, DWORD psize,
                        DWORD dwReadMode, DWORD dwWriteMode)
{
  /* Default to error. */
  *read_pipe_ptr = *write_pipe_ptr = INVALID_HANDLE_VALUE;

  HANDLE read_pipe = INVALID_HANDLE_VALUE, write_pipe = INVALID_HANDLE_VALUE;

  /* Ensure that there is enough pipe buffer space for atomic writes.  */
  if (psize < PIPE_BUF)
    psize = PIPE_BUF;

  char pipename[MAX_PATH];

  /* Retry CreateNamedPipe as long as the pipe name is in use.
   * Retrying will probably never be necessary, but we want
   * to be as robust as possible.  */
  while (1)
  {
    static volatile LONG pipe_unique_id;

    snprintf (pipename, sizeof pipename, "\\\\.\\pipe\\gnunet-%d-%ld",
              getpid (), InterlockedIncrement ((LONG *) & pipe_unique_id));
    /* Use CreateNamedPipe instead of CreatePipe, because the latter
     * returns a write handle that does not permit FILE_READ_ATTRIBUTES
     * access, on versions of win32 earlier than WinXP SP2.
     * CreatePipe also stupidly creates a full duplex pipe, which is
     * a waste, since only a single direction is actually used.
     * It's important to only allow a single instance, to ensure that
     * the pipe was not created earlier by some other process, even if
     * the pid has been reused.  We avoid FILE_FLAG_FIRST_PIPE_INSTANCE
     * because that is only available for Win2k SP2 and WinXP.  */
    read_pipe = CreateNamedPipeA (pipename, PIPE_ACCESS_INBOUND | dwReadMode, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1,   /* max instances */
                                  psize,        /* output buffer size */
                                  psize,        /* input buffer size */
                                  NMPWAIT_USE_DEFAULT_WAIT, sa_ptr);

    if (read_pipe != INVALID_HANDLE_VALUE)
    {
      break;
    }

    DWORD err = GetLastError ();

    switch (err)
    {
    case ERROR_PIPE_BUSY:
      /* The pipe is already open with compatible parameters.
       * Pick a new name and retry.  */
      continue;
    case ERROR_ACCESS_DENIED:
      /* The pipe is already open with incompatible parameters.
       * Pick a new name and retry.  */
      continue;
    case ERROR_CALL_NOT_IMPLEMENTED:
      /* We are on an older Win9x platform without named pipes.
       * Return an anonymous pipe as the best approximation.  */
      if (CreatePipe (read_pipe_ptr, write_pipe_ptr, sa_ptr, psize))
      {
        return 0;
      }
      err = GetLastError ();
      return err;
    default:
      return err;
    }
    /* NOTREACHED */
  }

  /* Open the named pipe for writing.
   * Be sure to permit FILE_READ_ATTRIBUTES access.  */
  write_pipe = CreateFileA (pipename, GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0,  /* share mode */
                            sa_ptr, OPEN_EXISTING, dwWriteMode, /* flags and attributes */
                            0); /* handle to template file */

  if (write_pipe == INVALID_HANDLE_VALUE)
  {
    /* Failure. */
    DWORD err = GetLastError ();

    CloseHandle (read_pipe);
    return err;
  }

  /* Success. */
  *read_pipe_ptr = read_pipe;
  *write_pipe_ptr = write_pipe;
  return 0;
}

/**
 * Writes @size bytes from @buf to @h, using @ov for
 * overlapped i/o. Deallocates @old_buf and sets it to NULL,
 * if necessary.
 * Writes asynchronously, but sequentially (only one writing
 * operation may be active at any given moment, but it will
 * be done in background). Thus it is intended to be used
 * for writing a few big chunks rather than a lot of small pieces.
 *
 * The extravagant interface is mainly because this function
 * does not use a separate struct to group together overlapped
 * structure, buffer pointer and the handle.
 *
 * @param h pipe handle
 * @param ov overlapped structure pointer
 * @param buf buffer to read from. Will be copied internally
 * @param size number of bytes to write
 * @param old_buf pointer where a copy of previous buffer is stored,
 *        and where a copy of @buf will be stored.
 *
 * @return number of bytes written, -1 on error
 */ 
static int
write_to_pipe (HANDLE h, OVERLAPPED *ov, unsigned char *buf, size_t size, unsigned char **old_buf)
{
  DWORD written;
  BOOL bresult;
  DWORD err;

  if (WAIT_OBJECT_0 != WaitForSingleObject (ov->hEvent, INFINITE))
    return -1;
  
  ResetEvent (ov->hEvent);

  if (*old_buf != NULL)
    free (*old_buf);

  *old_buf = malloc (size);
  if (*old_buf == NULL)
    return -1;
  memcpy (*old_buf, buf, size);
  written = 0;
  ov->Offset = 0;
  ov->OffsetHigh = 0;
  ov->Pointer = 0;
  ov->Internal = 0;
  ov->InternalHigh = 0;
  bresult = WriteFile (h, *old_buf, size, &written, ov);

  if (bresult == TRUE)
  {
    SetEvent (ov->hEvent);
    free (*old_buf);
    *old_buf = NULL;
    return written;
  }

  err = GetLastError ();
  if (err == ERROR_IO_PENDING)
    return size;
  SetEvent (ov->hEvent);
  *old_buf = NULL;
  SetLastError (err);
  return -1;
}

#define plugin_write(plug, buf, size) write_to_pipe (plug->cpipe_in, &plug->ov_write, buf, size, &plug->ov_write_buffer)

/**
 * Communicates plugin data (library name, options) to the plugin
 * process. This is only necessary on W32, where this information
 * is not inherited by the plugin, because it is not forked.
 *
 * @param plugin plugin context
 *
 * @return 0 on success, -1 on failure
 */ 
static int
write_plugin_data (struct EXTRACTOR_PluginList *plugin)
{
  size_t libname_len, shortname_len, opts_len;
  DWORD len;
  char *str;
  size_t total_len = 0;
  unsigned char *buf, *ptr;

  switch (plugin->flags)
  {
  case EXTRACTOR_OPTION_DEFAULT_POLICY:
    break;
  case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    break;
  case EXTRACTOR_OPTION_IN_PROCESS:
    return 0;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return 0;
    break;
  }

  libname_len = strlen (plugin->libname) + 1;
  total_len += sizeof (size_t) + libname_len;
  shortname_len = strlen (plugin->short_libname) + 1;
  total_len += sizeof (size_t) + shortname_len;
  if (plugin->plugin_options != NULL)
  {
    opts_len = strlen (plugin->plugin_options) + 1;
    total_len += opts_len;
  }
  else
  {
    opts_len = 0;
  }
  total_len += sizeof (size_t);

  buf = malloc (total_len);
  if (buf == NULL)
    return -1;
  ptr = buf;
  memcpy (ptr, &libname_len, sizeof (size_t));
  ptr += sizeof (size_t);
  memcpy (ptr, plugin->libname, libname_len);
  ptr += libname_len;
  memcpy (ptr, &shortname_len, sizeof (size_t));
  ptr += sizeof (size_t);
  memcpy (ptr, plugin->short_libname, shortname_len);
  ptr += shortname_len;
  memcpy (ptr, &opts_len, sizeof (size_t));
  ptr += sizeof (size_t);
  if (opts_len > 0)
  {
    memcpy (ptr, plugin->plugin_options, opts_len);
    ptr += opts_len;
  }
  if (total_len != write_to_pipe (plugin->cpipe_in, &plugin->ov_write, buf, total_len, &plugin->ov_write_buffer))
  {
    free (buf);
    return -1;
  }
  free (buf);
  return 0;
}

/**
 * Reads plugin data from the LE server process.
 * Also initializes allocation granularity (duh...).
 *
 * @param fd the pipe to read from
 *
 * @return newly allocated plugin context
 */ 
static struct EXTRACTOR_PluginList *
read_plugin_data (int fd)
{
  struct EXTRACTOR_PluginList *ret;
  size_t i;

  ret = malloc (sizeof (struct EXTRACTOR_PluginList));
  if (ret == NULL)
    return NULL;
  read (fd, &i, sizeof (size_t));
  ret->libname = malloc (i);
  if (ret->libname == NULL)
  {
    free (ret);
    return NULL;
  }
  read (fd, ret->libname, i);
  ret->libname[i - 1] = '\0';

  read (fd, &i, sizeof (size_t));
  ret->short_libname = malloc (i);
  if (ret->short_libname == NULL)
  {
    free (ret->libname);
    free (ret);
    return NULL;
  }
  read (fd, ret->short_libname, i);
  ret->short_libname[i - 1] = '\0';

  read (fd, &i, sizeof (size_t));
  if (i == 0)
  {
    ret->plugin_options = NULL;
  }
  else
  {
    ret->plugin_options = malloc (i);
    if (ret->plugin_options == NULL)
    {
      free (ret->short_libname);
      free (ret->libname);
      free (ret);
      return NULL;
    }
    read (fd, ret->plugin_options, i);
    ret->plugin_options[i - 1] = '\0';
  }
#if WINDOWS
  {
    SYSTEM_INFO si;
    GetSystemInfo (&si);
    ret->allocation_granularity = si.dwAllocationGranularity;
  }
#else
  ret->allocation_granularity = sysconf (_SC_PAGE_SIZE);
#endif
  return ret;
}

/**
 * Start the process for the given plugin.
 */ 
static void
start_process (struct EXTRACTOR_PluginList *plugin)
{
  HANDLE p1[2];
  HANDLE p2[2];
  STARTUPINFO startup;
  PROCESS_INFORMATION proc;
  char cmd[MAX_PATH + 1];
  char arg1[10], arg2[10];
  HANDLE p10_os_inh = INVALID_HANDLE_VALUE, p21_os_inh = INVALID_HANDLE_VALUE;
  SECURITY_ATTRIBUTES sa;

  switch (plugin->flags)
  {
  case EXTRACTOR_OPTION_DEFAULT_POLICY:
    if (plugin->hProcess != INVALID_HANDLE_VALUE && plugin->hProcess != 0)
      return;
    break;
  case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    if (plugin->hProcess != 0)
      return;
    break;
  case EXTRACTOR_OPTION_IN_PROCESS:
    return;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }

  sa.nLength = sizeof (sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = FALSE;

  plugin->hProcess = NULL;

  if (0 != create_selectable_pipe (&p1[0], &p1[1], &sa, 1024, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
  {
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }
  if (0 != create_selectable_pipe (&p2[0], &p2[1], &sa, 1024, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
  {
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }

  memset (&startup, 0, sizeof (STARTUPINFO));

  if (!DuplicateHandle (GetCurrentProcess (), p1[0], GetCurrentProcess (),
      &p10_os_inh, 0, TRUE, DUPLICATE_SAME_ACCESS)
      || !DuplicateHandle (GetCurrentProcess (), p2[1], GetCurrentProcess (),
      &p21_os_inh, 0, TRUE, DUPLICATE_SAME_ACCESS))
  {
    if (p10_os_inh != INVALID_HANDLE_VALUE)
      CloseHandle (p10_os_inh);
    if (p21_os_inh != INVALID_HANDLE_VALUE)
      CloseHandle (p21_os_inh);
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    CloseHandle (p2[0]);
    CloseHandle (p2[1]);
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }

  /* TODO: write our own plugin-hosting executable? rundll32, for once, has smaller than usual stack size.
   * Also, users might freak out seeing over 9000 rundll32 processes (seeing over 9000 processes named
   * "libextractor_plugin_helper" is probably less confusing).
   */
  snprintf(cmd, MAX_PATH + 1, "rundll32.exe libextractor-3.dll,RundllEntryPoint@16 %lu %lu", p10_os_inh, p21_os_inh);
  cmd[MAX_PATH] = '\0';
  if (CreateProcessA (NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL,
      &startup, &proc))
  {
    plugin->hProcess = proc.hProcess;
    CloseHandle (proc.hThread);
  }
  else
  {
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    CloseHandle (p2[0]);
    CloseHandle (p2[1]);
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }
  CloseHandle (p1[0]);
  CloseHandle (p2[1]);
  CloseHandle (p10_os_inh);
  CloseHandle (p21_os_inh);

  plugin->cpipe_in = p1[1];
  plugin->cpipe_out = p2[0];

  memset (&plugin->ov_read, 0, sizeof (OVERLAPPED));
  memset (&plugin->ov_write, 0, sizeof (OVERLAPPED));

  plugin->ov_write_buffer = NULL;

  plugin->ov_write.hEvent = CreateEvent (NULL, TRUE, TRUE, NULL);
  plugin->ov_read.hEvent = CreateEvent (NULL, TRUE, TRUE, NULL);
}

/**
 * Stop the child process of this plugin.
 */
static void
stop_process (struct EXTRACTOR_PluginList *plugin)
{
  int status;
  HANDLE process;

#if DEBUG
  if (plugin->hProcess == INVALID_HANDLE_VALUE)
    fprintf (stderr,
	     "Plugin `%s' choked on this input\n",
	     plugin->short_libname);
#endif
  if (plugin->hProcess == INVALID_HANDLE_VALUE ||
      plugin->hProcess == NULL)
    return;
  TerminateProcess (plugin->hProcess, 0);
  CloseHandle (plugin->hProcess);
  plugin->hProcess = INVALID_HANDLE_VALUE;
  CloseHandle (plugin->cpipe_out);
  CloseHandle (plugin->cpipe_in);
  plugin->cpipe_out = INVALID_HANDLE_VALUE;
  plugin->cpipe_in = INVALID_HANDLE_VALUE;
  CloseHandle (plugin->ov_read.hEvent);
  CloseHandle (plugin->ov_write.hEvent);
  if (plugin->ov_write_buffer != NULL)
  {
    free (plugin->ov_write_buffer);
    plugin->ov_write_buffer = NULL;
  }

  if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
    plugin->flags = EXTRACTOR_OPTION_DISABLED;

  plugin->seek_request = -1;
}

#endif /* WINDOWS */

/**
 * Remove a plugin from a list.
 *
 * @param prev the current list of plugins
 * @param library the name of the plugin to remove
 * @return the reduced list, unchanged if the plugin was not loaded
 */
struct EXTRACTOR_PluginList *
EXTRACTOR_plugin_remove(struct EXTRACTOR_PluginList * prev,
			const char * library)
{
  struct EXTRACTOR_PluginList *pos;
  struct EXTRACTOR_PluginList *first;

  pos = prev;
  first = prev;
  while ((pos != NULL) && (0 != strcmp (pos->short_libname, library)))
    {
      prev = pos;
      pos = pos->next;
    }
  if (pos != NULL)
    {
      /* found, close library */
      if (first == pos)
	first = pos->next;
      else
	prev->next = pos->next;
      /* found */
      stop_process (pos);
      free (pos->short_libname);
      free (pos->libname);
      free (pos->plugin_options);
      if (NULL != pos->libraryHandle) 
	lt_dlclose (pos->libraryHandle);      
      free (pos);
    }
#if DEBUG
  else
    fprintf(stderr,
	    "Unloading plugin `%s' failed!\n",
	    library);
#endif
  return first;
}


/**
 * Remove all plugins from the given list (destroys the list).
 *
 * @param plugin the list of plugins
 */
void 
EXTRACTOR_plugin_remove_all(struct EXTRACTOR_PluginList *plugins)
{
  while (plugins != NULL)
    plugins = EXTRACTOR_plugin_remove (plugins, plugins->short_libname);
}



/**
 * Open a file
 */
static int file_open(const char *filename, int oflag, ...)
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

#if WINDOWS

/**
 * Setup a shared memory segment.
 *
 * @param ptr set to the location of the map segment
 * @param map where to store the map handle
 * @param fn name of the mapping
 * @param fn_size size available in fn
 * @param size number of bytes to allocated for the mapping
 * @return 0 on success
 */
static int
make_shm_w32 (void **ptr, HANDLE *map, char *fn, size_t fn_size, size_t size)
{
  const char *tpath = "Local\\";
  snprintf (fn, fn_size, "%slibextractor-shm-%u-%u", tpath, getpid(),
      (unsigned int) RANDOM());
  *map = CreateFileMapping (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, fn);
  *ptr = MapViewOfFile (*map, FILE_MAP_WRITE, 0, 0, size);
  if (*ptr == NULL)
  {
    CloseHandle (*map);
    return 1;
  }
  return 0;
}

/**
 * Setup a file-backed shared memory segment.
 *
 * @param map where to store the map handle
 * @param file handle of the file to back the shm
 * @param fn name of the mapping
 * @param fn_size size available in fn
 * @param size number of bytes to allocated for the mapping
 * @return 0 on success
 */
static int
make_file_backed_shm_w32 (HANDLE *map, HANDLE file, char *fn, size_t fn_size)
{
  const char *tpath = "Local\\";
  snprintf (fn, fn_size, "%slibextractor-shm-%u-%u", tpath, getpid(),
      (unsigned int) RANDOM());
  *map = CreateFileMapping (file, NULL, PAGE_READONLY, 0, 0, fn);
  if (*map == NULL)
  {
    DWORD err = GetLastError ();
    return 1;
  }
  return 0;
}

static void
destroy_shm_w32 (void *ptr, HANDLE map)
{
  UnmapViewOfFile (ptr);
  CloseHandle (map);
}

static void
destroy_file_backed_shm_w32 (HANDLE map)
{
  CloseHandle (map);
}

#else

/**
 * Setup a shared memory segment.
 *
 * @param ptr set to the location of the shm segment
 * @param shmid where to store the shm ID
 * @param fn name of the shared segment
 * @param fn_size size available in fn
 * @param size number of bytes to allocated for the segment
 * @return 0 on success
 */
static int
make_shm_posix (void **ptr, int *shmid, char *fn, size_t fn_size, size_t size)
{
  const char *tpath;
#if SOMEBSD
  /* this works on FreeBSD, not sure about others... */
  tpath = getenv ("TMPDIR");
  if (tpath == NULL)
    tpath = "/tmp/";
#else
  tpath = "/"; /* Linux */
#endif 
  snprintf (fn, fn_size, "%slibextractor-shm-%u-%u", tpath, getpid(),
      (unsigned int) RANDOM());
  *shmid = shm_open (fn, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  *ptr = NULL;
  if (-1 == *shmid)
    return 1;
  if ((0 != ftruncate (*shmid, size)) ||
      (NULL == (*ptr = mmap (NULL, size, PROT_WRITE, MAP_SHARED, *shmid, 0))) ||
      (*ptr == (void*) -1) )
  {
    close (*shmid);
    *shmid = -1;
    shm_unlink (fn);
    return 1;
  }
  return 0;
}

static void
destroy_shm_posix (void *ptr, int shm_id, size_t size, char *shm_name)
{
  if (NULL != ptr)
    munmap (ptr, size);
  if (shm_id != -1)
    close (shm_id);
  shm_unlink (shm_name);
}
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/**
 * A poor attempt to abstract the data source (file or a memory buffer)
 * for the decompressor.
 */ 
struct BufferedFileDataSource
{
  /**
   * Descriptor of the file to read data from (may be -1)
   */ 
  int fd;

  /**
   * Pointer to the buffer to read from (may be NULL)
   */ 
  const unsigned char *data;

  /**
   * Size of the file (or the data buffer)
   */ 
  int64_t fsize;

  /**
   * Position within the file or the data buffer
   */ 
  int64_t fpos;

  /**
   * A buffer to read into. For fd != -1: when data != NULL,
   * data is used directly.
   */ 
  unsigned char *buffer;

  /**
   * Position within the buffer.
   */ 
  int64_t buffer_pos;

  /**
   * Number of bytes in the buffer (<= buffer_size)
   */ 
  int64_t buffer_bytes;

  /**
   * Allocated size of the buffer
   */ 
  int64_t buffer_size;
};

/**
 * Creates a bfds
 *
 * @param data data buffer to use as a source (NULL if fd != -1)
 * @param fd file descriptor to use as a source (-1 if data != NULL)
 * @param fsize size of the file (or the buffer)
 * @return newly allocated bfds
 */ 
struct BufferedFileDataSource *
bfds_new (const unsigned char *data, int fd, int64_t fsize);

/**
 * Unallocates bfds
 *
 * @param bfds bfds to deallocate
 */ 
void
bfds_delete (struct BufferedFileDataSource *bfds);

/**
 * Makes bfds seek to @pos and read a chunk of bytes there.
 * Changes bfds->fpos, bfds->buffer_bytes and bfds->buffer_pos.
 * Does almost nothing for memory-backed bfds.
 *
 * @param bfds bfds
 * @param pos position
 * @return 0 on success, -1 on error
 */ 
int
bfds_pick_next_buffer_at (struct BufferedFileDataSource *bfds, int64_t pos);

/**
 * Makes bfds seek to @pos in @whence mode.
 * Will try to seek within the buffer, will move the buffer location if
 * the seek request falls outside of the buffer range.
 *
 * @param bfds bfds
 * @param pos position to seek to
 * @param whence one of the seek constants (SEEK_CUR, SEEK_SET, SEEK_END)
 * @return new absolute position
 */ 
int64_t
bfds_seek (struct BufferedFileDataSource *bfds, int64_t pos, int whence);

/**
 * Fills @buf_ptr with a pointer to a chunk of data.
 * Same as read() but there's no need to allocate or de-allocate the
 * memory (since data IS already in memory).
 * Will seek if necessary. Will fail if @count exceeds buffer size.
 *
 * @param bfds bfds
 * @param buf_ptr location to store data pointer
 * @param count number of bytes to read
 * @return number of bytes (<= count) available at location pointed by buf_ptr
 */ 
int64_t
bfds_read (struct BufferedFileDataSource *bfds, unsigned char **buf_ptr, int64_t count);

struct BufferedFileDataSource *
bfds_new (const unsigned char *data, int fd, int64_t fsize)
{
  struct BufferedFileDataSource *result;
  result = malloc (sizeof (struct BufferedFileDataSource));
  if (result == NULL)
    return NULL;
  memset (result, 0, sizeof (struct BufferedFileDataSource));
  result->data = data;
  result->fsize = fsize;
  result->fd = fd;
  result->buffer_size = fsize;
  if (result->data == NULL)
  {
    if (result->buffer_size > MAX_READ)
      result->buffer_size = MAX_READ;
    result->buffer = malloc (result->buffer_size);
    if (result->buffer == NULL)
    {
      free (result);
      return NULL;
    }
  }
  bfds_pick_next_buffer_at (result, 0);
  return result;
}

void
bfds_delete (struct BufferedFileDataSource *bfds)
{
  if (bfds->buffer)
    free (bfds->buffer);
  free (bfds);
}

int
bfds_pick_next_buffer_at (struct BufferedFileDataSource *bfds, int64_t pos)
{
  int64_t position, rd;
  if (bfds->data != NULL)
  {
    bfds->buffer_bytes = bfds->fsize;
    return 0;
  }
#if WINDOWS
  position = _lseeki64 (bfds->fd, pos, SEEK_SET);
#elif HAVE_LSEEK64
  position = lseek64 (bfds->fd, pos, SEEK_SET);
#else
  position = (int64_t) lseek (bfds->fd, pos, SEEK_SET);
#endif
  if (position < 0)
    return -1;
  bfds->fpos = position;
  rd = read (bfds->fd, bfds->buffer, bfds->buffer_size);
  if (rd < 0)
    return -1;
  bfds->buffer_bytes = rd;
  return 0;
}

int64_t
bfds_seek (struct BufferedFileDataSource *bfds, int64_t pos, int whence)
{
  switch (whence)
  {
  case SEEK_CUR:
    if (bfds->data == NULL)
    {
      if (0 != bfds_pick_next_buffer_at (bfds, bfds->fpos + bfds->buffer_pos + pos))
        return -1;
      bfds->buffer_pos = 0;
      return bfds->fpos;
    }
    bfds->buffer_pos += pos; 
    return bfds->buffer_pos;
    break;
  case SEEK_SET:
    if (pos < 0)
      return -1;
    if (bfds->data == NULL)
    {
      if (0 != bfds_pick_next_buffer_at (bfds, pos))
        return -1;
      bfds->buffer_pos = 0;
      return bfds->fpos;
    }
    bfds->buffer_pos = pos; 
    return bfds->buffer_pos;
    break;
  case SEEK_END:
    if (bfds->data == NULL)
    {
      if (0 != bfds_pick_next_buffer_at (bfds, bfds->fsize + pos))
        return -1;
      bfds->buffer_pos = 0;
      return bfds->fpos;
    }
    bfds->buffer_pos = bfds->fsize + pos; 
    return bfds->buffer_pos;
    break;
  }
  return -1;
}

int64_t
bfds_read (struct BufferedFileDataSource *bfds, unsigned char **buf_ptr, int64_t count)
{
  if (count > MAX_READ)
    return -1;
  if (count > bfds->buffer_bytes - bfds->buffer_pos)
  {
    if (bfds->fpos + bfds->buffer_pos != bfds_seek (bfds, bfds->fpos + bfds->buffer_pos, SEEK_SET))
      return -1;
    if (bfds->data == NULL)
    {
      *buf_ptr = &bfds->buffer[bfds->buffer_pos];
      bfds->buffer_pos += count < bfds->buffer_bytes ? count : bfds->buffer_bytes;
      return (count < bfds->buffer_bytes ? count : bfds->buffer_bytes);
    }
    else
    {
      int64_t ret = count < (bfds->buffer_bytes - bfds->buffer_pos) ? count : (bfds->buffer_bytes - bfds->buffer_pos);
      *buf_ptr = &bfds->data[bfds->buffer_pos];
      bfds->buffer_pos += ret;
      return ret;
    }
  }
  else
  {
    if (bfds->data == NULL)
      *buf_ptr = &bfds->buffer[bfds->buffer_pos];
    else
      *buf_ptr = &bfds->data[bfds->buffer_pos];
    bfds->buffer_pos += count;
    return count;
  }
}

#if HAVE_ZLIB
#define MIN_ZLIB_HEADER 12
#endif
#if HAVE_LIBBZ2
#define MIN_BZ2_HEADER 4
#endif
#if !defined (MIN_COMPRESSED_HEADER) && HAVE_ZLIB
#define MIN_COMPRESSED_HEADER MIN_ZLIB_HEADER
#endif
#if !defined (MIN_COMPRESSED_HEADER) && HAVE_LIBBZ2
#define MIN_COMPRESSED_HEADER MIN_BZ2_HEADER
#endif
#if !defined (MIN_COMPRESSED_HEADER)
#define MIN_COMPRESSED_HEADER -1
#endif

#define COMPRESSED_DATA_PROBE_SIZE 3

enum ExtractorCompressionType
{
  COMP_TYPE_UNDEFINED = -1,
  COMP_TYPE_INVALID = 0,
  COMP_TYPE_ZLIB = 1,
  COMP_TYPE_BZ2 = 2
};

/**
 * An object from which uncompressed data can be read
 */ 
struct CompressedFileSource
{
  /**
   * The type of compression used in the source
   */ 
  enum ExtractorCompressionType compression_type;
  /**
   * The source of data
   */ 
  struct BufferedFileDataSource *bfds;
  /**
   * Size of the source (same as bfds->fsize)
   */ 
  int64_t fsize;
  /**
   * Position within the source
   */ 
  int64_t fpos;

  /**
   * Total size of the uncompressed data. Remains -1 until
   * decompression is finished.
   */ 
  int64_t uncompressed_size;

  /*
  unsigned char *buffer;
  int64_t buffer_bytes;
  int64_t buffer_len;
  */

#if WINDOWS
  /**
   * W32 handle of the shm into which data is uncompressed
   */ 
  HANDLE shm;
#else
  /**
   * POSIX id of the shm into which data is uncompressed
   */ 
  int shm;
#endif
  /**
   * Name of the shm
   */ 
  char shm_name[MAX_SHM_NAME + 1];
  /**
   * Pointer to the mapped region of the shm (covers the whole shm)
   */ 
  void *shm_ptr;
  /**
   * Position within shm
   */ 
  int64_t shm_pos;
  /**
   * Allocated size of the shm
   */ 
  int64_t shm_size;
  /**
   * Number of bytes in shm (<= shm_size)
   */ 
  size_t shm_buf_size;

#if HAVE_ZLIB
  /**
   * ZLIB stream object
   */ 
  z_stream strm;
  /**
   * Length of gzip header (may be 0, in that case ZLIB parses the header)
   */ 
  int gzip_header_length;
#endif
#if HAVE_LIBBZ2
  /**
   * BZ2 stream object
   */ 
  bz_stream bstrm;
#endif
};

int
cfs_delete (struct CompressedFileSource *cfs)
{
#if WINDOWS
  destroy_shm_w32 (cfs->shm_ptr, cfs->shm);
#else
  destroy_shm_posix (cfs->shm_ptr, cfs->shm, cfs->shm_size, cfs->shm_name);
#endif
  free (cfs);
}

int
cfs_reset_stream_zlib (struct CompressedFileSource *cfs)
{
  if (cfs->gzip_header_length != bfds_seek (cfs->bfds, cfs->gzip_header_length, SEEK_SET))
    return 0;
  cfs->strm.next_in = NULL;
  cfs->strm.avail_in = 0;
  cfs->strm.total_in = 0;
  cfs->strm.zalloc = NULL;
  cfs->strm.zfree = NULL;
  cfs->strm.opaque = NULL;

  /*
   * note: maybe plain inflateInit(&strm) is adequate,
   * it looks more backward-compatible also ;
   *
   * ZLIB_VERNUM isn't defined by zlib version 1.1.4 ;
   * there might be a better check.
   */
  if (Z_OK != inflateInit2 (&cfs->strm,
#ifdef ZLIB_VERNUM
      15 + 32
#else
      -MAX_WBITS
#endif
      ))
  {
    return -1;
  }

  cfs->fpos = cfs->gzip_header_length;
  cfs->shm_pos = 0;
  cfs->shm_buf_size = 0;

#if HAVE_ZLIB
  z_stream strm;
#endif
  return 1;
}

static int
cfs_reset_stream_bz2 (struct CompressedFileSource *cfs)
{
  return -1;
}

/**
 * Resets the compression stream to begin uncompressing
 * from the beginning. Used at initialization time, and when
 * seeking backward.
 *
 * @param cfs cfs to reset
 * @return 1 on success, -1 on error
 */
int
cfs_reset_stream (struct CompressedFileSource *cfs)
{
  switch (cfs->compression_type)
  {
  case COMP_TYPE_ZLIB:
    return cfs_reset_stream_zlib (cfs);
  case COMP_TYPE_BZ2:
    return cfs_reset_stream_bz2 (cfs);
  default:
    return -1;
  }
}


static int
cfs_init_decompressor_zlib (struct CompressedFileSource *cfs, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  /* Process gzip header */
  unsigned int gzip_header_length = 10;
  unsigned char *pdata;
  unsigned char data[12];
  
  if (12 > bfds_read (cfs->bfds, &pdata, 12))
    return -1;
  memcpy (data, pdata, 12);
  
  if (data[3] & 0x4) /* FEXTRA  set */
    gzip_header_length += 2 + (unsigned) (data[10] & 0xff) +
      (((unsigned) (data[11] & 0xff)) * 256);

  if (data[3] & 0x8) /* FNAME set */
  {
    int64_t fp = cfs->fpos;
    int64_t buf_bytes;
    int len;
    unsigned char *buf, *cptr;
    if (gzip_header_length > bfds_seek (cfs->bfds, gzip_header_length, SEEK_SET))
      return -1;
    buf_bytes = bfds_read (cfs->bfds, &buf, 1024);
    if (buf_bytes <= 0)
      return -1;
    cptr = buf;

    len = 0;
    /* stored file name is here */
    while (len < buf_bytes)
    {
      if ('\0' == *cptr)
      break;
      cptr++;
      len++;
    }

    if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_FILENAME,
        EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
        (const char *) buf,
        len))
      return 0; /* done */

    /* FIXME: check for correctness */
    //gzip_header_length = (cptr - data) + 1;
    gzip_header_length += len + 1;
  }

  if (data[3] & 0x16) /* FCOMMENT set */
  {
    int64_t fp = cfs->fpos;
    int64_t buf_bytes;
    int len;
    unsigned char *buf, *cptr;
    if (gzip_header_length > bfds_seek (cfs->bfds, gzip_header_length, SEEK_SET))
      return -1;
    buf_bytes = bfds_read (cfs->bfds, &buf, 1024);
    if (buf_bytes <= 0)
      return -1;
    cptr = buf;

    len = 0;
    /* stored file name is here */
    while (len < buf_bytes)
    {
      if ('\0' == *cptr)
      break;
      cptr++;
      len++;
    }

    if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_COMMENT,
        EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
        (const char *) buf,
        len))
      return 0; /* done */

    /* FIXME: check for correctness */
    //gzip_header_length = (cptr - data) + 1;
    gzip_header_length += len + 1;
  }

  if (data[3] & 0x2) /* FCHRC set */
    gzip_header_length += 2;

  memset (&cfs->strm, 0, sizeof (z_stream));

#ifdef ZLIB_VERNUM
  gzip_header_length = 0;
#endif

  cfs->gzip_header_length = gzip_header_length;
  return cfs_reset_stream_zlib (cfs);
}

static int
cfs_deinit_decompressor_zlib (struct CompressedFileSource *cfs)
{
  inflateEnd (&cfs->strm);
  return 1;
}

static int
cfs_init_decompressor_bz2 (struct CompressedFileSource *cfs, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  return -1;
}

static int
cfs_deinit_decompressor_bz2 (struct CompressedFileSource *cfs)
{
  return -1;
}

/**
 * Initializes decompression object. Might report metadata about
 * compresse stream, if available. Resets the stream to the beginning.
 *
 * @param cfs cfs to initialize
 * @param proc callback for metadata
 * @param proc_cls callback cls
 * @return 1 on success, -1 on error
 */
static int
cfs_init_decompressor (struct CompressedFileSource *cfs, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  switch (cfs->compression_type)
  {
  case COMP_TYPE_ZLIB:
    return cfs_init_decompressor_zlib (cfs, proc, proc_cls);
  case COMP_TYPE_BZ2:
    return cfs_init_decompressor_bz2 (cfs, proc, proc_cls);
  default:
    return -1;
  }
}

/**
 * Deinitializes decompression object.
 *
 * @param cfs cfs to deinitialize
 * @return 1 on success, -1 on error
 */
static int
cfs_deinit_decompressor (struct CompressedFileSource *cfs)
{
  switch (cfs->compression_type)
  {
  case COMP_TYPE_ZLIB:
    return cfs_deinit_decompressor_zlib (cfs);
  case COMP_TYPE_BZ2:
    return cfs_deinit_decompressor_bz2 (cfs);
  default:
    return -1;
  }
}

/**
 * Allocates and initializes new cfs object.
 *
 * @param bfds data source to use
 * @param fsize size of the source
 * @param compression_type type of compression used
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return newly allocated cfs on success, NULL on error
 */
struct CompressedFileSource *
cfs_new (struct BufferedFileDataSource *bfds, int64_t fsize, enum ExtractorCompressionType compression_type, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int shm_result;
  size_t map_size;
  struct CompressedFileSource *cfs;
  cfs = malloc (sizeof (struct CompressedFileSource));
  if (cfs == NULL)
    return NULL;
  memset (cfs, 0, sizeof (struct CompressedFileSource));
  cfs->compression_type = compression_type;
  cfs->bfds = bfds;
  cfs->fsize = fsize;
  cfs->uncompressed_size = -1;
  cfs->shm_size = MAX_READ;
#if !WINDOWS
  shm_result = make_shm_posix ((void **) &cfs->shm_ptr, &cfs->shm, cfs->shm_name, MAX_SHM_NAME, cfs->shm_size);
#else
  shm_result = make_shm_w32 ((void **) &cfs->shm_ptr, &cfs->shm, cfs->shm_name, MAX_SHM_NAME, cfs->shm_size);
#endif
  if (shm_result != 0)
  {
    cfs_delete (cfs);
    return NULL;
  }
  return cfs;
}

/**
 * Data is read from the source and shoved into decompressor
 * in chunks this big.
 */
#define COM_CHUNK_SIZE (10*1024)

int
cfs_read_zlib (struct CompressedFileSource *cfs, int64_t preserve)
{
  int ret;
  int64_t rc = preserve;
  int64_t total = cfs->strm.total_out;
  if (preserve > 0)
    memmove (cfs->shm_ptr, &((unsigned char *)cfs->shm_ptr)[0], preserve);

  while (rc < cfs->shm_size && ret != Z_STREAM_END)
  {
    if (cfs->strm.avail_in == 0)
    {
      int64_t count = bfds_read (cfs->bfds, &cfs->strm.next_in, COM_CHUNK_SIZE);
      if (count <= 0)
        return 0;
      cfs->strm.avail_in = (uInt) count;
    }
    cfs->strm.next_out = &((unsigned char *)cfs->shm_ptr)[rc];
    cfs->strm.avail_out = cfs->shm_size - rc;
    ret = inflate (&cfs->strm, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
      return 0;
    rc = cfs->strm.total_out - total;
  }
  if (ret == Z_STREAM_END)
    cfs->uncompressed_size = cfs->strm.total_out;
  cfs->shm_pos = preserve;
  cfs->shm_buf_size = rc + preserve;
  return 1;
}

int
cfs_read_bz2 (struct CompressedFileSource *cfs, int64_t preserve)
{
  return -1;
}

/**
 * Re-fills shm with new uncompressed data, preserving the last
 * @preserve bytes of existing data as the first @preserve bytes
 * of the new data.
 * Does the actual decompression. Will set uncompressed_size on
 * the end of compressed stream.
 *
 * @param cfds cfs to read from
 * @param preserve number of bytes to preserve (0 to discard all old data)
 * @return number of bytes in shm. 0 if no more data can be uncompressed.
 */
int64_t
cfs_read (struct CompressedFileSource *cfs, int64_t preserve)
{
  switch (cfs->compression_type)
  {
  case COMP_TYPE_ZLIB:
    return cfs_read_zlib (cfs, preserve);
  case COMP_TYPE_BZ2:
    return cfs_read_bz2 (cfs, preserve);
  default:
    return -1;
  }
}

int64_t
cfs_seek_zlib (struct CompressedFileSource *cfs, int64_t position)
{
  int64_t ret;
  if (position > cfs->strm.total_out - cfs->shm_buf_size && position < cfs->strm.total_out)
  {
    ret = cfs_read (cfs, cfs->strm.total_out - position);
    if (ret < 0)
      return ret;
    return position;
  }
  while (position >= cfs->strm.total_out)
  {
    if (0 > (ret = cfs_read (cfs, 0)))
      return ret;
    if (ret == 0)
      return position;
  }
  if (position < cfs->strm.total_out && position > cfs->strm.total_out - cfs->shm_buf_size)
    return cfs->strm.total_out - cfs->shm_buf_size;
  return -1;
}

int64_t
cfs_seek_bz2 (struct CompressedFileSource *cfs, int64_t position)
{
  return -1;
}

/**
 * Moves the buffer to @position in uncompressed steam. If position
 * requires seeking backwards beyond the boundaries of the buffer, resets the
 * stream and repeats decompression from the beginning to @position.
 *
 * @param cfds cfs to seek on
 * @param position new starting point for the buffer
 * @return new absolute buffer position, -1 on error or EOS
 */
int64_t
cfs_seek (struct CompressedFileSource *cfs, int64_t position)
{
  switch (cfs->compression_type)
  {
  case COMP_TYPE_ZLIB:
    return cfs_seek_zlib (cfs, position);
  case COMP_TYPE_BZ2:
    return cfs_seek_bz2 (cfs, position);
  default:
    return -1;
  }
}

/**
 * Detect if we have compressed data on our hands.
 *
 * @param data pointer to a data buffer or NULL (in case fd is not -1)
 * @param fd a file to read data from, or -1 (if data is not NULL)
 * @param fsize size of data (if data is not NULL) or of file (if fd is not -1)
 * @return -1 to indicate an error, 0 to indicate uncompressed data, or a type (> 0) of compression
 */
static enum ExtractorCompressionType
get_compression_type (const unsigned char *data, int fd, int64_t fsize)
{
  void *read_data = NULL;
  size_t read_data_size = 0;
  ssize_t read_result;
  enum ExtractorCompressionType result = COMP_TYPE_INVALID;

  if ((MIN_COMPRESSED_HEADER < 0) || (fsize < MIN_COMPRESSED_HEADER))
  {
    return COMP_TYPE_INVALID;
  }
  if (data == NULL)
  {
    int64_t position;
    read_data_size = COMPRESSED_DATA_PROBE_SIZE;
    read_data = malloc (read_data_size);
    if (read_data == NULL)
      return -1;
#if WINDOWS
    position = _lseeki64 (fd, 0, SEEK_CUR);
#elif HAVE_LSEEK64
    position = lseek64 (fd, 0, SEEK_CUR);
#else
    position = (int64_t) lseek (fd, 0, SEEK_CUR);
#endif
    read_result = READ (fd, read_data, read_data_size);
#if WINDOWS
    position = _lseeki64 (fd, position, SEEK_SET);
#elif HAVE_LSEEK64
    position = lseek64 (fd, position, SEEK_SET);
#else
    position = lseek (fd, (off_t) position, SEEK_SET);
#endif
    if (read_result != read_data_size)
    {
      free (read_data);
      return COMP_TYPE_UNDEFINED;
    }
    data = (const void *) read_data;
  }
#if HAVE_ZLIB
  if ((fsize >= MIN_ZLIB_HEADER) && (data[0] == 0x1f) && (data[1] == 0x8b) && (data[2] == 0x08))
    result = COMP_TYPE_ZLIB;
#endif
#if HAVE_LIBBZ2
  if ((fsize >= MIN_BZ2_HEADER) && (data[0] == 'B') && (data[1] == 'Z') && (data[2] == 'h')) 
    result = COMP_TYPE_BZ2;
#endif
  if (read_data != NULL)
    free (read_data);
  return result;
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
init_plugin_state (struct EXTRACTOR_PluginList *plugin, uint8_t operation_mode, const char *shm_name, int64_t fsize)
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
 * Forces plugin to move the buffer window to @pos.
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
pl_pick_next_buffer_at (struct EXTRACTOR_PluginList *plugin, int64_t pos, uint8_t want_start)
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
int64_t
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

int64_t
pl_get_fsize (struct EXTRACTOR_PluginList *plugin)
{
  return plugin->fsize;
}

int64_t
pl_get_pos (struct EXTRACTOR_PluginList *plugin)
{
  return plugin->fpos + plugin->shm_pos;
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
int64_t
pl_read (struct EXTRACTOR_PluginList *plugin, unsigned char **data, size_t count)
{
  if (count > MAX_READ)
    return -1;
  if (count > plugin->map_size - plugin->shm_pos)
  {
    int64_t actual_count;
    if (plugin->fpos + plugin->shm_pos != pl_seek (plugin, plugin->fpos + plugin->shm_pos, SEEK_SET))
      return -1;
    *data = &plugin->shm_ptr[plugin->shm_pos];
    actual_count = (count < plugin->map_size - plugin->shm_pos ? count : plugin->map_size - plugin->shm_pos);
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
give_shm_to_plugin (struct EXTRACTOR_PluginList *plugin, int64_t position, size_t map_size, int64_t fsize, uint8_t operation_mode)
{
  int write_result;
  int updated_shm_size = 1 + sizeof (int64_t) + sizeof (size_t) + sizeof (int64_t);
  unsigned char updated_shm[updated_shm_size];
  int t = 0;
  updated_shm[t] = MESSAGE_UPDATED_SHM;
  t += 1;
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
ask_in_process_plugin (struct EXTRACTOR_PluginList *plugin, void *shm_ptr, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
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
      if (extract_reply == 1)
        plugin->seek_request = -1;
    }
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }
}

#if !WINDOWS
/**
 * Receive @size bytes from plugin, store them in @buf
 *
 * @param plugin plugin context
 * @param buf buffer to fill
 * @param size number of bytes to read
 * @return number of bytes read, 0 on EOS, < 0 on error
 */
int
plugin_read (struct EXTRACTOR_PluginList *plugin, unsigned char *buf, size_t size)
{
  ssize_t read_result;
  size_t read_count = 0;
  while (read_count < size)
  {
    read_result = read (plugin->cpipe_out, &buf[read_count], size - read_count);
    if (read_result <= 0)
      return read_result;
    read_count += read_result;
  }
  return read_count;
}
#else
/**
 * Receive @size bytes from plugin, store them in @buf
 *
 * @param plugin plugin context
 * @param buf buffer to fill
 * @param size number of bytes to read
 * @return number of bytes read, 0 on EOS, < 0 on error
 */
int 
plugin_read (struct EXTRACTOR_PluginList *plugin, unsigned char *buf, size_t size)
{
  DWORD bytes_read;
  BOOL bresult;
  size_t read_count = 0;
  while (read_count < size)
  {
    bresult = ReadFile (plugin->cpipe_out, &buf[read_count], size - read_count, &bytes_read, NULL);
    if (!bresult)
      return -1;
    read_count += bytes_read;
  }
  return read_count;
}
#endif

/**
 * Receive a reply from plugin (seek request, metadata and done message)
 *
 * @param plugin plugin context
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return 0 on success, -1 on error
 */
static int
receive_reply (struct EXTRACTOR_PluginList *plugin, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int read_result;
  unsigned char code;
  int must_read = 1;

  int64_t seek_position;
  struct IpcHeader hdr;
  char *mime_type;
  char *data;

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
      read_result = plugin_read (plugin, (unsigned char *) &seek_position, sizeof (int64_t));
      if (read_result < sizeof (int64_t))
        return -1;
      plugin->seek_request = seek_position;
      must_read = 0;
      break;
    case MESSAGE_META: /* Meta */
      read_result = plugin_read (plugin, (unsigned char *) &hdr, sizeof (hdr));
      if (read_result < sizeof (hdr)) /* FIXME: check hdr for sanity */
        return -1;
      mime_type = malloc (hdr.mime_len + 1);
      if (mime_type == NULL)
        return -1;
      read_result = plugin_read (plugin, (unsigned char *) mime_type, hdr.mime_len);
      if (read_result < hdr.mime_len)
        return -1;
      mime_type[hdr.mime_len] = '\0';
      data = malloc (hdr.data_len);
      if (data == NULL)
      {
        free (mime_type);
        return -1;
      }
      read_result = plugin_read (plugin, (unsigned char *) data, hdr.data_len);
      if (read_result < hdr.data_len)
      {
        free (mime_type);
        free (data);
        return -1;
      }
      read_result = proc (proc_cls, plugin->short_libname, hdr.meta_type, hdr.meta_format, mime_type, data, hdr.data_len);
      free (mime_type);
      free (data);
      if (read_result != 0)
        return 1;
      break;
    default:
      return -1;
    }
  }
  return 0;
}

#if !WINDOWS
/**
 * Wait for one of the plugins to reply.
 * Selects on plugin output pipes, runs receive_reply()
 * on each activated pipe until it gets a seek request
 * or a done message. Called repeatedly by the user until all pipes are dry or
 * broken.
 *
 * @param plugins to select upon
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return number of dry/broken pipes since last call, -1 on error or if no
 *         plugins reply in 10 seconds.
 */
static int
wait_for_reply (struct EXTRACTOR_PluginList *plugins, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int ready;
  int result;
  struct timeval tv;
  fd_set to_check;
  int highest = 0;
  int read_result;
  struct EXTRACTOR_PluginList *ppos;

  FD_ZERO (&to_check);

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
  {
    switch (ppos->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
      if (ppos->seek_request == -1)
        continue;
      FD_SET (ppos->cpipe_out, &to_check);
      if (highest < ppos->cpipe_out)
        highest = ppos->cpipe_out;
      break;
    case EXTRACTOR_OPTION_IN_PROCESS:
      break;
    case EXTRACTOR_OPTION_DISABLED:
      break;
    }
  }

  tv.tv_sec = 10;
  tv.tv_usec = 0;
  ready = select (highest + 1, &to_check, NULL, NULL, &tv);
  if (ready <= 0)
    /* an error or timeout -> something's wrong or all plugins hung up */
    return -1;

  result = 0;
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
  {
    switch (ppos->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
      if (ppos->seek_request == -1)
        continue;
      if (FD_ISSET (ppos->cpipe_out, &to_check))
      {
        read_result = receive_reply (ppos, proc, proc_cls);
        if (read_result < 0)
        {
          stop_process (ppos);
        }
        result += 1;
      }
      break;
    case EXTRACTOR_OPTION_IN_PROCESS:
      break;
    case EXTRACTOR_OPTION_DISABLED:
      break;
    }
  }
  return result;
}
#else
/**
 * Wait for one of the plugins to reply.
 * Selects on plugin output pipes, runs receive_reply()
 * on each activated pipe until it gets a seek request
 * or a done message. Called repeatedly by the user until all pipes are dry or
 * broken.
 * This W32 version of wait_for_reply() can't select on more than 64 plugins
 * at once (returns -1 if there are more than 64 plugins).
 *
 * @param plugins to select upon
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return number of dry/broken pipes since last call, -1 on error or if no
 *         plugins reply in 10 seconds.
 */
static int
wait_for_reply (struct EXTRACTOR_PluginList *plugins, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int result;
  DWORD ms;
  DWORD first_ready;
  DWORD dwresult;
  DWORD bytes_read;
  BOOL bresult;
  int i;
  HANDLE events[MAXIMUM_WAIT_OBJECTS];
  

  struct EXTRACTOR_PluginList *ppos;

  i = 0;
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
  {
    if (i == MAXIMUM_WAIT_OBJECTS)
      return -1;
    if (ppos->seek_request == -1)
      continue;
    switch (ppos->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
      if (WaitForSingleObject (ppos->ov_read.hEvent, 0) == WAIT_OBJECT_0)
      {
        ResetEvent (ppos->ov_read.hEvent);
        bresult = ReadFile (ppos->cpipe_out, &i, 0, &bytes_read, &ppos->ov_read);
        if (bresult == TRUE)
        {
          SetEvent (ppos->ov_read.hEvent);
        }
        else
        {
          DWORD err = GetLastError ();
          if (err != ERROR_IO_PENDING)
            SetEvent (ppos->ov_read.hEvent);
        }
      }
      events[i] = ppos->ov_read.hEvent;
      i++;
      break;
    case EXTRACTOR_OPTION_IN_PROCESS:
      break;
    case EXTRACTOR_OPTION_DISABLED:
      break;
    }
  }

  ms = 10000;
  first_ready = WaitForMultipleObjects (i, events, FALSE, ms);
  if (first_ready == WAIT_TIMEOUT || first_ready == WAIT_FAILED)
    /* an error or timeout -> something's wrong or all plugins hung up */
    return -1;

  i = 0;
  result = 0;
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
  {
    int read_result;
    switch (ppos->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
      if (ppos->seek_request == -1)
        continue;
      if (i < first_ready)
      {
        i += 1;
        continue;
      }
      dwresult = WaitForSingleObject (ppos->ov_read.hEvent, 0);
      read_result = 0;
      if (dwresult == WAIT_OBJECT_0)
      {
        read_result = receive_reply (ppos, proc, proc_cls);
        result += 1;
      }
      if (dwresult == WAIT_FAILED || read_result < 0)
      {
        stop_process (ppos);
        if (dwresult == WAIT_FAILED)
          result += 1;
      }
      i++;
      break;
    case EXTRACTOR_OPTION_IN_PROCESS:
      break;
    case EXTRACTOR_OPTION_DISABLED:
      break;
    }
  }
  return result;
}

#endif

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
seek_to_new_position (struct EXTRACTOR_PluginList *plugins, struct CompressedFileSource *cfs, int64_t current_position, int64_t map_size)
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
    plugin_load (plugin);
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
do_extract (struct EXTRACTOR_PluginList *plugins, const char *data, int fd, const char *filename, struct CompressedFileSource *cfs, int64_t fsize, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
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
  int fd = -1;
  struct stat64 fstatbuf;
  int64_t fsize = 0;
  enum ExtractorCompressionType compression_type = -1;
  void *buffer = NULL;
  size_t buffer_size;
  int decompression_result;
  struct CompressedFileSource *cfs = NULL;

  /* If data is not given, then we need to read it from the file. Try opening it */
  if ((data == NULL) &&
      (filename != NULL) &&
      (0 == STAT64(filename, &fstatbuf)) &&
      (!S_ISDIR(fstatbuf.st_mode)) &&
      (-1 != (fd = file_open (filename,
             O_RDONLY | O_LARGEFILE))))
  {
    /* Empty files are of no interest */
    fsize = fstatbuf.st_size;
    if (fsize == 0) 
    {
       close(fd);
       return;
    }
  }

  /* Data is not given, and we've failed to open the file with data -> exit */
  if ((fsize == 0) && (data == NULL))
    return;
  /* fsize is now size of the data OR size of the file */
  if (data != NULL)
    fsize = size;

  errno = 0;
  /* Peek at first few bytes of the file (or of the data), and see if it's compressed. */
  compression_type = get_compression_type (data, fd, fsize);
  if (compression_type < 0)
  {
    /* errno is set by get_compression_type () */
    if (fd != -1)
      close (fd);
    return;
  }

  struct BufferedFileDataSource *bfds;
  bfds = bfds_new (data, fd, fsize);
  if (bfds == NULL)
    return;

  if (compression_type > 0)
  {
    int icr = 0;
    /* Set up a decompressor.
     * Will also report compression-related metadata to the caller.
     */
    cfs = cfs_new (bfds, fsize, compression_type, proc, proc_cls);
    if (cfs == NULL)
    {
      if (fd != -1)
        close (fd);
      errno = EILSEQ;
      return;
    }
    icr = cfs_init_decompressor (cfs, proc, proc_cls);
    if (icr < 0)
    {
      if (fd != -1)
        close (fd);
      errno = EILSEQ;
      return;
    }
    else if (icr == 0)
    {
      if (fd != -1)
        close (fd);
      errno = 0;
      return;
    }
  }

  /* do_extract () might set errno itself, but from our point of view everything is OK */
  errno = 0;

  do_extract (plugins, data, fd, filename, cfs, fsize, proc, proc_cls);
  if (cfs != NULL)
  {
    cfs_deinit_decompressor (cfs);
    cfs_delete (cfs);
  }
  bfds_delete (bfds);
  if (-1 != fd)
    close(fd);
}


#if WINDOWS
void CALLBACK 
RundllEntryPoint (HWND hwnd, 
		  HINSTANCE hinst, 
		  LPSTR lpszCmdLine, 
		  int nCmdShow)
{
  intptr_t in_h;
  intptr_t out_h;
  int in, out;

  sscanf(lpszCmdLine, "%lu %lu", &in_h, &out_h);
  in = _open_osfhandle (in_h, _O_RDONLY);
  out = _open_osfhandle (out_h, 0);
  setmode (in, _O_BINARY);
  setmode (out, _O_BINARY);
  plugin_main (read_plugin_data (in),
		    in, out);
}

void CALLBACK 
RundllEntryPointA (HWND hwnd, 
		  HINSTANCE hinst, 
		  LPSTR lpszCmdLine, 
		  int nCmdShow)
{
  return RundllEntryPoint(hwnd, hinst, lpszCmdLine, nCmdShow);
}
#endif

/**
 * Initialize gettext and libltdl (and W32 if needed).
 */
void __attribute__ ((constructor)) EXTRACTOR_ltdl_init() {
  int err;

#if ENABLE_NLS
  BINDTEXTDOMAIN(PACKAGE, LOCALEDIR);
  BINDTEXTDOMAIN("iso-639", ISOLOCALEDIR); /* used by wordextractor */
#endif
  err = lt_dlinit ();
  if (err > 0) {
#if DEBUG
    fprintf(stderr,
      _("Initialization of plugin mechanism failed: %s!\n"),
      lt_dlerror());
#endif
    return;
  }
#if WINDOWS
  plibc_init("GNU", PACKAGE);
#endif
}


/**
 * Deinit.
 */
void __attribute__ ((destructor)) EXTRACTOR_ltdl_fini() {
#if WINDOWS
  plibc_shutdown();
#endif
  lt_dlexit ();
}

/* end of extractor.c */
