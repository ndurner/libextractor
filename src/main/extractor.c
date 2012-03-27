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
 * How many bytes do we actually try to decompress? (from the beginning
 * of the file).  Limit to 16 MB.
 */
#define MAX_DECOMPRESS 16 * 1024 * 1024

/**
 * Maximum length of a Mime-Type string.
 */
#define MAX_MIME_LEN 256

#define MAX_SHM_NAME 255

/**
 * Set to 1 to get failure info,
 * 2 for actual debug info.
 */ 
#define DEBUG 1

#define MESSAGE_INIT_STATE 0x01
#define MESSAGE_UPDATED_SHM 0x02
#define MESSAGE_DONE 0x03
#define MESSAGE_SEEK 0x04
#define MESSAGE_META 0x05
#define MESSAGE_DISCARD_STATE 0x06

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
int
plugin_open_shm (struct EXTRACTOR_PluginList *plugin, char *shm_name)
{
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = shm_open (shm_name, O_RDONLY, 0);
  return plugin->shm_id;
}
#else
HANDLE
plugin_open_shm (struct EXTRACTOR_PluginList *plugin, char *shm_name)
{
  if (plugin->map_handle != 0)
    CloseHandle (plugin->map_handle);
  plugin->map_handle = OpenFileMapping (FILE_MAP_READ, FALSE, shm_name);
  return plugin->map_handle;
}
#endif

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
 * 'main' function of the child process.  Reads shm-filenames from
 * 'in' (line-by-line) and writes meta data blocks to 'out'.  The meta
 * data stream is terminated by an empty entry.
 *
 * @param plugin extractor plugin to use
 * @param in stream to read from
 * @param out stream to write to
 */
static void
process_requests (struct EXTRACTOR_PluginList *plugin, int in, int out)
{
  int read_result1, read_result2, read_result3;
  unsigned char code;
  int64_t fsize = -1;
  int64_t position = 0;
  void *shm_ptr = NULL;
  size_t shm_size = 0;
  char *shm_name = NULL;
  size_t shm_name_len;

  int extract_reply;

  struct IpcHeader hdr;
  int do_break;
#ifdef WINDOWS
  HANDLE map;
  MEMORY_BASIC_INFORMATION mi;
#endif

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
      read_result2 = read (in, &fsize, sizeof (int64_t));
      read_result3 = read (in, &shm_name_len, sizeof (size_t));
      if ((read_result2 < sizeof (int64_t)) || (read_result3 < sizeof (size_t)) ||
          shm_name_len > MAX_SHM_NAME || fsize <= 0)
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
#if !WINDOWS
      if (shm_ptr != NULL)
        munmap (shm_ptr, shm_size);
      if (-1 == plugin_open_shm (plugin, shm_name))
      {
        do_break = 1;
        break;
      }
#else
      if (shm_ptr != NULL)
        UnmapViewOfFile (shm_ptr);
      if (INVALID_HANDLE_VALUE == plugin_open_shm (plugin, shm_name))
      {
        do_break = 1;
        break;
      }
#endif
      plugin->fsize = fsize;
      plugin->init_state_method (plugin);
      break;
    case MESSAGE_DISCARD_STATE:
      plugin->discard_state_method (plugin);
#if !WINDOWS
      if (shm_ptr != NULL && shm_size > 0)
        munmap (shm_ptr, shm_size);
      if (plugin->shm_id != -1)
        close (plugin->shm_id);
      plugin->shm_id = -1;
      shm_size = 0;
#else
      if (shm_ptr != NULL)
        UnmapViewOfFile (shm_ptr);
      if (plugin->map_handle != 0)
        CloseHandle (plugin->map_handle);
      plugin->map_handle = 0;
#endif
      shm_ptr = NULL;
      break;
    case MESSAGE_UPDATED_SHM:
      read_result2 = read (in, &position, sizeof (int64_t));
      read_result3 = read (in, &shm_size, sizeof (size_t));
      if ((read_result2 < sizeof (int64_t)) || (read_result3 < sizeof (size_t)) ||
          position < 0 || fsize <= 0 || position >= fsize)
      {
        do_break = 1;
        break;
      }
      /* FIXME: also check mapped region size (lseek for *nix, VirtualQuery for W32) */
#if !WINDOWS
      if ((-1 == plugin->shm_id) ||
          (NULL == (shm_ptr = mmap (NULL, shm_size, PROT_READ, MAP_SHARED, plugin->shm_id, 0))) ||
          (shm_ptr == (void *) -1))
      {
        do_break = 1;
        break;
      }
#else
      if ((plugin->map_handle == 0) ||
         (NULL == (shm_ptr = MapViewOfFile (plugin->map_handle, FILE_MAP_READ, 0, 0, 0))))
      {
        do_break = 1;
        break;
      }
#endif
      plugin->position = position;
      plugin->shm_ptr = shm_ptr;
      plugin->map_size = shm_size;
      /* Now, ideally a plugin would do reads and seeks on a virtual "plugin" object
       * completely transparently, and the underlying code would return bytes from
       * the memory map, or would block and wait for a seek to happen.
       * That, however, requires somewhat different architecture, and even more wrapping
       * and hand-helding. It's easier to make plugins aware of the fact that they work
       * with discrete in-memory buffers with expensive seeking, not continuous files.
       */
      extract_reply = plugin->extract_method (plugin, transmit_reply, &out);
#if !WINDOWS
      if ((shm_ptr != NULL) &&
          (shm_ptr != (void*) -1) )
        munmap (shm_ptr, shm_size);
#else
      if (shm_ptr != NULL)
        UnmapViewOfFile (shm_ptr);
#endif
      if (extract_reply == 1)
      {
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
      break;
    }
  }
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
    process_requests (plugin, p1[0], p2[1]);
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
  /* only does anything on Windows */
  return 0;
}

#define plugin_print(plug, fmt, ...) fprintf (plug->cpipe_in, fmt, ...)
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

static int
print_to_pipe (HANDLE h, OVERLAPPED *ov, unsigned char **buf, const char *fmt, ...)
{
  va_list va;
  va_list vacp;
  size_t size;
  char *print_buf;
  int result;

  va_start (va, fmt);
  va_copy (vacp, va);
  size = VSNPRINTF (NULL, 0, fmt, vacp) + 1;
  va_end (vacp);
  if (size <= 0)
  {
    va_end (va);
    return size;
  }

  print_buf = malloc (size);
  if (print_buf == NULL)
    return -1;
  VSNPRINTF (print_buf, size, fmt, va);
  va_end (va);
  
  result = write_to_pipe (h, ov, print_buf, size, buf);
  free (buf);
  return result;
}

#define plugin_print(plug, fmt, ...) print_to_pipe (plug->cpipe_in, &plug->ov_write, &plug->ov_write_buffer, fmt, ...)
#define plugin_write(plug, buf, size) write_to_pipe (plug->cpipe_in, &plug->ov_write, buf, size, &plug->ov_write_buffer)

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

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

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

/**
 * Try to decompress compressed data
 *
 * @param data data to decompress, or NULL (if fd is not -1)
 * @param fd file to read data from, or -1 (if data is not NULL)
 * @param fsize size of data (if data is not NULL) or size of fd file (if fd is not -1)
 * @param compression_type type of compression, as returned by get_compression_type ()
 * @param buffer a pointer to a buffer pointer, buffer pointer is NEVER a NULL and already has some data (usually - COMPRESSED_DATA_PROBE_SIZE bytes) in it.
 * @param buffer_size a pointer to buffer size
 * @param proc callback for metadata
 * @param proc_cls cls for proc
 * @return 0 on success, anything else on error
 */
static int
try_to_decompress (const unsigned char *data, int fd, int64_t fsize, int compression_type, void **buffer, size_t *buffer_size, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  unsigned char *new_buffer;
  ssize_t read_result;

  unsigned char *buf;
  unsigned char *rbuf;
  size_t dsize;
#if HAVE_ZLIB
  z_stream strm;
  int ret;
  size_t pos;
#endif
#if HAVE_LIBBZ2
  bz_stream bstrm;
  int bret;
  size_t bpos;
#endif

  if (fd != -1)
  {
    if (fsize > *buffer_size)
    {
      /* Read the rest of the file. Can't de-compress it partially anyway */
      /* Memory mapping is not useful here, because memory mapping ALSO takes up
       * memory (even more than a buffer, since it might be aligned), and
       * because we need to read every byte anyway (lazy on-demand reads into
       * memory provided by memory mapping won't help).
       */
      new_buffer = realloc (*buffer, fsize);
      if (new_buffer == NULL)
      {
        free (*buffer);
        return -1;
      }
      read_result = READ (fd, &new_buffer[*buffer_size], fsize - *buffer_size);
      if (read_result != fsize - *buffer_size)
      {
        free (*buffer);
        return -1;
      }
      *buffer = new_buffer;
      *buffer_size = fsize;
    }
    data = (const unsigned char *) new_buffer;
  }

#if HAVE_ZLIB
  if (compression_type == 1) 
  {
    /* Process gzip header */
    unsigned int gzip_header_length = 10;

    if (data[3] & 0x4) /* FEXTRA  set */
      gzip_header_length += 2 + (unsigned) (data[10] & 0xff) +
        (((unsigned) (data[11] & 0xff)) * 256);

    if (data[3] & 0x8) /* FNAME set */
    {
      const unsigned char *cptr = data + gzip_header_length;

      /* stored file name is here */
      while ((cptr - data) < fsize)
      {
        if ('\0' == *cptr)
        break;
        cptr++;
      }

      if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_FILENAME,
          EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
          (const char *) (data + gzip_header_length),
          cptr - (data + gzip_header_length)))
        return 0; /* done */

      gzip_header_length = (cptr - data) + 1;
    }

    if (data[3] & 0x16) /* FCOMMENT set */
    {
      const unsigned char * cptr = data + gzip_header_length;

      /* stored comment is here */
      while (cptr < data + fsize)
      {
        if ('\0' == *cptr)
          break;
        cptr ++;
      }  

      if (0 != proc (proc_cls, "<zlib>", EXTRACTOR_METATYPE_COMMENT,
          EXTRACTOR_METAFORMAT_C_STRING, "text/plain",
          (const char *) (data + gzip_header_length),
          cptr - (data + gzip_header_length)))
        return 0; /* done */

      gzip_header_length = (cptr - data) + 1;
    }

    if (data[3] & 0x2) /* FCHRC set */
      gzip_header_length += 2;

    memset (&strm, 0, sizeof (z_stream));

#ifdef ZLIB_VERNUM
    gzip_header_length = 0;
#endif

    if (fsize > gzip_header_length)
    {
      strm.next_in = (Bytef *) data + gzip_header_length;
      strm.avail_in = fsize - gzip_header_length;
    }
    else
    {
      strm.next_in = (Bytef *) data;
      strm.avail_in = 0;
    }
    strm.total_in = 0;
    strm.zalloc = NULL;
    strm.zfree = NULL;
    strm.opaque = NULL;

    /*
     * note: maybe plain inflateInit(&strm) is adequate,
     * it looks more backward-compatible also ;
     *
     * ZLIB_VERNUM isn't defined by zlib version 1.1.4 ;
     * there might be a better check.
     */
    if (Z_OK == inflateInit2 (&strm,
#ifdef ZLIB_VERNUM
        15 + 32
#else
        -MAX_WBITS
#endif
        ))
    {
      pos = 0;
      dsize = 2 * fsize;
      if ( (dsize > MAX_DECOMPRESS) ||
	   (dsize < fsize) )
        dsize = MAX_DECOMPRESS;
      buf = malloc (dsize);

      if (buf != NULL)
      {
        strm.next_out = (Bytef *) buf;
        strm.avail_out = dsize;

        do
        {
          ret = inflate (&strm, Z_SYNC_FLUSH);
          if (ret == Z_OK)
          {
            if (dsize == MAX_DECOMPRESS)
              break;

            pos += strm.total_out;
            strm.total_out = 0;
            dsize *= 2;

            if (dsize > MAX_DECOMPRESS)
              dsize = MAX_DECOMPRESS;

            rbuf = realloc (buf, dsize);
            if (rbuf == NULL)
            {
              free (buf);
              buf = NULL;
              break;
            }

            buf = rbuf;
            strm.next_out = (Bytef *) &buf[pos];
            strm.avail_out = dsize - pos;
          }
          else if (ret != Z_STREAM_END) 
          {
            /* error */
            free (buf);
            buf = NULL;
          }
        } while ((buf != NULL) && (ret != Z_STREAM_END));

        dsize = pos + strm.total_out;
        if ((dsize == 0) && (buf != NULL))
        {
          free (buf);
          buf = NULL;
        }
      }

      inflateEnd (&strm);

      if (fd != -1)
        if (*buffer != NULL)
          free (*buffer);

      if (buf == NULL)
      {
        return -1;
      }
      else
      {
        *buffer = buf;
        *buffer_size = dsize;
        return 0;
      }
    }
  }
#endif
  
#if HAVE_LIBBZ2
  if (compression_type == 2) 
  {
    memset(&bstrm, 0, sizeof (bz_stream));
    bstrm.next_in = (char *) data;
    bstrm.avail_in = fsize;
    bstrm.total_in_lo32 = 0;
    bstrm.total_in_hi32 = 0;
    bstrm.bzalloc = NULL;
    bstrm.bzfree = NULL;
    bstrm.opaque = NULL;
    if (BZ_OK == BZ2_bzDecompressInit(&bstrm, 0,0)) 
    {
      bpos = 0;
      dsize = 2 * fsize;
      if ( (dsize > MAX_DECOMPRESS) || (dsize < fsize) )
        dsize = MAX_DECOMPRESS;
      buf = malloc (dsize);

      if (buf != NULL) 
      {
        bstrm.next_out = (char *) buf;
        bstrm.avail_out = dsize;

        do
        {
          bret = BZ2_bzDecompress (&bstrm);
          if (bret == Z_OK) 
          {
            if (dsize == MAX_DECOMPRESS)
              break;
            bpos += bstrm.total_out_lo32;
            bstrm.total_out_lo32 = 0;

            dsize *= 2;
            if (dsize > MAX_DECOMPRESS)
              dsize = MAX_DECOMPRESS;

            rbuf = realloc(buf, dsize);
            if (rbuf == NULL)
            {
              free (buf);
              buf = NULL;
              break;
            }

            buf = rbuf;
            bstrm.next_out = (char*) &buf[bpos];
            bstrm.avail_out = dsize - bpos;
          } 
          else if (bret != BZ_STREAM_END) 
          {
            /* error */
            free (buf);
            buf = NULL;
          }
        } while ((buf != NULL) && (bret != BZ_STREAM_END));

        dsize = bpos + bstrm.total_out_lo32;
        if ((dsize == 0) && (buf != NULL))
        {
          free (buf);
          buf = NULL;
        }
      }

      BZ2_bzDecompressEnd (&bstrm);

      if (fd != -1)
        if (*buffer != NULL)
          free (*buffer);

      if (buf == NULL)
      {
        return -1;
      }
      else
      {
        *buffer = buf;
	*buffer_size = dsize;
        return 0;
      }
    }
  }
#endif
  return -1;
}

/**
 * Detect if we have compressed data on our hands.
 *
 * @param data pointer to a data buffer or NULL (in case fd is not -1)
 * @param fd a file to read data from, or -1 (if data is not NULL)
 * @param fsize size of data (if data is not NULL) or of file (if fd is not -1)
 * @param buffer will receive a pointer to the data that this function read
 * @param buffer_size will receive size of the buffer
 * @return -1 to indicate an error, 0 to indicate uncompressed data, or a type (> 0) of compression
 */
static int
get_compression_type (const unsigned char *data, int fd, int64_t fsize, void **buffer, size_t *buffer_size)
{
  void *read_data = NULL;
  size_t read_data_size = 0;
  ssize_t read_result;

  if ((MIN_COMPRESSED_HEADER < 0) || (fsize < MIN_COMPRESSED_HEADER))
  {
    *buffer = NULL;
    return 0;
  }
  if (data == NULL)
  {
    read_data_size = COMPRESSED_DATA_PROBE_SIZE;
    read_data = malloc (read_data_size);
    if (read_data == NULL)
      return -1;
    read_result = READ (fd, read_data, read_data_size);
    if (read_result != read_data_size)
    {
      free (read_data);
      return -1;
    }
    *buffer = read_data;
    *buffer_size = read_data_size;
    data = (const void *) read_data;
  }
#if HAVE_ZLIB
  if ((fsize >= MIN_ZLIB_HEADER) && (data[0] == 0x1f) && (data[1] == 0x8b) && (data[2] == 0x08))
    return 1;
#endif
#if HAVE_LIBBZ2
  if ((fsize >= MIN_BZ2_HEADER) && (data[0] == 'B') && (data[1] == 'Z') && (data[2] == 'h')) 
    return 2;
#endif
  return 0;
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

static void
destroy_shm_w32 (void *ptr, HANDLE map)
{
  UnmapViewOfFile (ptr);
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


static void
init_plugin_state (struct EXTRACTOR_PluginList *plugin, char *shm_name, int64_t fsize)
{
  int write_result;
  int init_state_size;
  unsigned char *init_state;
  int t;
  size_t shm_name_len = strlen (shm_name) + 1;
  init_state_size = 1 + sizeof (size_t) + shm_name_len + sizeof (int64_t);
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
    plugin_open_shm (plugin, shm_name);
    plugin->fsize = fsize;
    plugin->init_state_method (plugin);
    plugin->seek_request = 0;
    return;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }
}

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
    plugin->discard_state_method (plugin);
    return;
    break;
  case EXTRACTOR_OPTION_DISABLED:
    return;
    break;
  }
}

static int
give_shm_to_plugin (struct EXTRACTOR_PluginList *plugin, int64_t position, size_t map_size)
{
  int write_result;
  int updated_shm_size = 1 + sizeof (int64_t) + sizeof (size_t);
  unsigned char updated_shm[updated_shm_size];
  int t = 0;
  updated_shm[t] = MESSAGE_UPDATED_SHM;
  t += 1;
  memcpy (&updated_shm[t], &position, sizeof (int64_t));
  t += sizeof (int64_t);
  memcpy (&updated_shm[t], &map_size, sizeof (size_t));
  t += sizeof (size_t);
  switch (plugin->flags)
  {
  case EXTRACTOR_OPTION_DEFAULT_POLICY:
  case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    if (plugin->seek_request < 0)
      return 0;
    write_result = plugin_write (plugin, updated_shm, updated_shm_size);
    if (write_result < updated_shm_size)
    {
      stop_process (plugin);
      return 0;
    }
    return 1;
  case EXTRACTOR_OPTION_IN_PROCESS:
    plugin->position = position;
    plugin->map_size = map_size;
    return 0;
  case EXTRACTOR_OPTION_DISABLED:
    return 0;
  default:
    return 1;
  }
}

static void
ask_in_process_plugin (struct EXTRACTOR_PluginList *plugin, int64_t position, void *shm_ptr, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
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

static int64_t
seek_to_new_position (struct EXTRACTOR_PluginList *plugins, int fd, int64_t fsize, int64_t current_position)
{
  int64_t min_pos = fsize;
  struct EXTRACTOR_PluginList *ppos;
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
  {
    switch (ppos->flags)
    {
    case EXTRACTOR_OPTION_DEFAULT_POLICY:
    case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
    case EXTRACTOR_OPTION_IN_PROCESS:
    if (ppos->seek_request > 0 && ppos->seek_request >= current_position &&
        ppos->seek_request <= min_pos)
      min_pos = ppos->seek_request;
      break;
    case EXTRACTOR_OPTION_DISABLED:
      break;
    }
  }
  if (min_pos >= fsize)
    return -1;
#if WINDOWS
  _lseeki64 (fd, min_pos, SEEK_SET);
#elif !HAVE_SEEK64
  lseek64 (fd, min_pos, SEEK_SET);
#else
  if (min_pos >= INT_MAX)
    return -1;
  lseek (fd, (ssize_t) min_pos, SEEK_SET);
#endif
  return min_pos;
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
 * @param fsize size of data or size of file
 * @param buffer a buffer with data alteady read from the file (if fd != -1)
 * @param buffer_size size of buffer
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
do_extract (struct EXTRACTOR_PluginList *plugins, const char *data, int fd, int64_t fsize, void *buffer, size_t buffer_size, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
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
  size_t map_size;
  ssize_t read_result;
  int kill_plugins = 0;

  map_size = (fd == -1) ? fsize : MAX_READ;

  /* Make a shared memory object. Even if we're running in-process. Simpler that way */
#if !WINDOWS
  shm_result = make_shm_posix ((void **) &shm_ptr, &shm_id, shm_name, MAX_SHM_NAME,
      map_size);
#else  
  shm_result = make_shm_w32 ((void **) &shm_ptr, &map_handle, shm_name, MAX_SHM_NAME,
      map_size);
#endif
  if (shm_result != 0)
    return;

  /* This three-loops-instead-of-one construction is intended to increase parallelism */
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    start_process (ppos);

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    load_in_process_plugin (ppos);

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    write_plugin_data (ppos);

  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    init_plugin_state (ppos, shm_name, fsize);

  while (1)
  {
    int plugins_not_ready = 0;
    if (fd != -1)
    {
      /* fill the share buffer with data from the file */
      if (buffer_size > 0)
        memcpy (shm_ptr, buffer, buffer_size);
      read_result = READ (fd, &shm_ptr[buffer_size], MAX_READ - buffer_size);
      if (read_result <= 0)
        break;
      else
        map_size = read_result + buffer_size;
      if (buffer_size > 0)
         buffer_size = 0;
    }
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      plugins_not_ready += give_shm_to_plugin (ppos, position, map_size);
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      ask_in_process_plugin (ppos, position, shm_ptr, proc, proc_cls);
    while (plugins_not_ready > 0 && !kill_plugins)
    {
      int ready = wait_for_reply (plugins, proc, proc_cls);
      if (ready <= 0)
        kill_plugins = 1;
      plugins_not_ready -= ready;
    }
    if (kill_plugins)
      break;
    if (fd != -1)
    {
      position += map_size;
      position = seek_to_new_position (plugins, fd, fsize, position);
      if (position < 0)
        break;
    }
    else
      break;
  }

  if (kill_plugins)
    for (ppos = plugins; NULL != ppos; ppos = ppos->next)
      stop_process (ppos);
  for (ppos = plugins; NULL != ppos; ppos = ppos->next)
    discard_plugin_state (ppos);

#if WINDOWS
  destroy_shm_w32 (shm_ptr, map_handle);
#else
  destroy_shm_posix (shm_ptr, shm_id, (fd == -1) ? fsize : MAX_READ, shm_name);
#endif
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
  int memory_only = 1;
  int compression_type = -1;
  void *buffer = NULL;
  size_t buffer_size;
  int decompression_result;

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
    /* File is too big -> can't read it into memory */
    if (fsize > MAX_READ)
      memory_only = 0;
  }

  /* Data is not given, and we've failed to open the file with data -> exit */
  if ((fsize == 0) && (data == NULL))
    return;
  /* fsize is now size of the data OR size of the file */
  if (data != NULL)
    fsize = size;

  errno = 0;
  /* Peek at first few bytes of the file (or of the data), and see if it's compressed.
   * If data is NULL, buffer is allocated by the function and holds the first few bytes
   * of the file, buffer_size is set too.
   */
  compression_type = get_compression_type (data, fd, fsize, &buffer, &buffer_size);
  if (compression_type < 0)
  {
    /* errno is set by get_compression_type () */
    if (fd != -1)
      close (fd);
    return;
  }
  if (compression_type > 0)
  {
    /* Don't assume that MAX_DECOMPRESS < MAX_READ */
    if ((fsize > MAX_DECOMPRESS) || (fsize > MAX_READ))
    {
      /* File or data is to big to be decompressed in-memory (the only kind of decompression we do) */
      errno = EFBIG;
      if (fd != -1)
        close (fd);
      if (buffer != NULL)
        free (buffer);
      return;
    }
    /* Decompress data (or file contents + what we've read so far. Either way it writes a new
     * pointer to buffer, sets buffer_size, and frees the old buffer (if it wasn't NULL).
     * In case of failure it cleans up the buffer after itself.
     * Will also report compression-related metadata to the caller.
     */
    decompression_result = try_to_decompress (data, fd, fsize, compression_type, &buffer, &buffer_size, proc, proc_cls);
    if (decompression_result != 0)
    {
      /* Buffer is taken care of already */
      close (fd);
      errno = EILSEQ;
      return;
    }
    else
    {
      close (fd);
      fd = -1;
    }
  }

  /* Now we either have a non-NULL data of fsize bytes
   * OR a valid fd to read from and a small buffer of buffer_size bytes
   * OR an invalid fd and a big buffer of buffer_size bytes
   * Simplify this situation a bit:
   */
  if ((data == NULL) && (fd == -1) && (buffer_size > 0))
  {
    data = (const void *) buffer;
    fsize = buffer_size;
  }

  /* Now we either have a non-NULL data of fsize bytes
   * OR a valid fd to read from and a small buffer of buffer_size bytes
   * and we might need to free the buffer later in either case
   */

  /* do_extract () might set errno itself, but from our point of view everything is OK */
  errno = 0;

  do_extract (plugins, data, fd, fsize, buffer, buffer_size, proc, proc_cls);

  if (buffer != NULL)
    free (buffer);
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
  process_requests (read_plugin_data (in),
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
