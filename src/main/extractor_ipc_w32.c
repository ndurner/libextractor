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
#include "extractor_datasource.h"
#include "extractor_plugin_main.h"
#include "extractor_plugins.h"
#include "extractor_ipc.h"
#include "extractor_logging.h"

/**
 */
struct EXTRACTOR_SharedMemory
{

  /**
   * W32 handle of the shm into which data is uncompressed
   */ 
  HANDLE map;

  /**
   * Name of the shm
   */ 
  char shm_name[MAX_SHM_NAME + 1];

  /**
   * Pointer to the mapped region of the shm (covers the whole shm)
   */ 
  void *ptr;

  /**
   * Position within shm
   */ 
  int64_t pos;

  /**
   * Allocated size of the shm
   */ 
  int64_t shm_size;

  /**
   * Number of bytes in shm (<= shm_size)
   */ 
  size_t shm_buf_size;

  size_t shm_map_size;

  /**
   * Reference counter describing how many references share this SHM.
   */
  unsigned int rc;
};


/**
 * Definition of an IPC communication channel with
 * some plugin.
 */
struct EXTRACTOR_Channel
{

  /**
   * Process ID of the child process for this plugin. 0 for none.
   */
  HANDLE hProcess;

  /**
   * Pipe used to communicate information to the plugin child process.
   * NULL if not initialized.
   */
  HANDLE cpipe_in;

  /**
   * Handle of the shm object
   */
  HANDLE map_handle;

  /**
   * Pipe used to read information about extracted meta data from
   * the plugin child process.  -1 if not initialized.
   */
  HANDLE cpipe_out;

  /**
   * A structure for overlapped reads on W32.
   */
  OVERLAPPED ov_read;

  /**
   * A structure for overlapped writes on W32.
   */
  OVERLAPPED ov_write;

  /**
   * A write buffer for overlapped writes on W32
   */
  unsigned char *ov_write_buffer;

  /**
   * The plugin this channel is to communicate with.
   */
  struct EXTRACTOR_PluginList *plugin;

  /**
   * Memory segment shared with this process.
   */
  struct EXTRACTOR_SharedMemory *shm;

  void *old_buf;

  /**
   * Buffer for reading data from the plugin.
   */
  char *mdata;

  /**
   * Size of the 'mdata' buffer.
   */
  size_t mdata_size;

  /**
   * Number of valid bytes in the channel's buffer.
   */
  size_t size;
};


/**
 * Create a shared memory area.
 *
 * @param size size of the shared area
 * @return NULL on error
 */
struct EXTRACTOR_SharedMemory *
EXTRACTOR_IPC_shared_memory_create_ (size_t size)
{
  struct EXTRACTOR_SharedMemory *shm;
  const char *tpath = "Local\\";

  if (NULL == (shm = malloc (sizeof (struct EXTRACTOR_SharedMemory))))
    return NULL;

  snprintf (shm->shm_name, MAX_SHM_NAME, 
	    "%slibextractor-shm-%u-%u", 
	    tpath, getpid(),
	    (unsigned int) RANDOM());
  shm->map = CreateFileMapping (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, shm->shm_name);
  shm->ptr = MapViewOfFile (shm->map, FILE_MAP_WRITE, 0, 0, size);
  if (shm->ptr == NULL)
  {
    CloseHandle (shm->map);
    free (shm);
    return NULL;
  }
  shm->shm_size = size;
  shm->rc = 0;
  return shm;
}

/**
 * Change the reference counter for this shm instance.
 *
 * @param shm instance to update
 * @param delta value to change RC by
 * @return new RC
 */
unsigned int
EXTRACTOR_IPC_shared_memory_change_rc_ (struct EXTRACTOR_SharedMemory *shm,
					int delta)
{
  shm->rc += delta;
  return shm->rc;
}


/**
 * Destroy shared memory area.
 *
 * @param shm memory area to destroy
 * @return NULL on error
 */
void
EXTRACTOR_IPC_shared_memory_destroy_ (struct EXTRACTOR_SharedMemory *shm)
{  
  if (shm->ptr != NULL)
    UnmapViewOfFile (shm->ptr);
  if (shm->map != 0)
    CloseHandle (shm->map);
  free (shm);
}

/**
 * Initialize shared memory area from data source.
 *
 * @param shm memory area to initialize
 * @param ds data source to use for initialization
 * @param off offset to use in data source
 * @param size number of bytes to copy
 * @return -1 on error, otherwise number of bytes copied
 */
ssize_t
EXTRACTOR_IPC_shared_memory_set_ (struct EXTRACTOR_SharedMemory *shm,
				  struct EXTRACTOR_Datasource *ds,
				  uint64_t off,
				  size_t size)
{
  if (-1 ==
      EXTRACTOR_datasource_seek_ (ds, off, SEEK_SET))
    {
      LOG ("Failed to set IPC memory due to seek error\n");
      return -1;
    }
  if (size > shm->shm_size)
    size = shm->shm_size;
  return EXTRACTOR_datasource_read_ (ds,
				     shm->ptr,
				     size);
}


/**
 * Query datasource for current position
 *
 * @param ds data source to query
 * @return current position in the datasource or UINT_MAX on error
 */
uint64_t
EXTRACTOR_datasource_get_pos_ (struct EXTRACTOR_Datasource *ds)
{
  int64_t pos = EXTRACTOR_datasource_seek_ (ds, 0, SEEK_CUR);
  if (-1 == pos)
    return UINT_MAX;
  return pos;
}


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
 * Communicates plugin data (library name, options) to the plugin
 * process. This is only necessary on W32, where this information
 * is not inherited by the plugin, because it is not forked.
 *
 * @param plugin plugin context
 *
 * @return 0 on success, -1 on failure
 */ 
static int
write_plugin_data (struct EXTRACTOR_PluginList *plugin,
    struct EXTRACTOR_Channel *channel)
{
  size_t libname_len, shortname_len, opts_len;
  DWORD len;
  char *str;
  size_t total_len = 0;
  unsigned char *buf, *ptr;
  ssize_t write_result;

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
  write_result = EXTRACTOR_IPC_channel_send_ (channel, buf, total_len);
  free (buf);
  return total_len == write_result;
}

/**
 * Create a channel to communicate with a process wrapping
 * the plugin of the given name.  Starts the process as well.
 *
 * @param plugin the plugin
 * @param shm memory to share with the process
 * @return NULL on error, otherwise IPC channel
 */ 
struct EXTRACTOR_Channel *
EXTRACTOR_IPC_channel_create_ (struct EXTRACTOR_PluginList *plugin,
			       struct EXTRACTOR_SharedMemory *shm)
{
  struct EXTRACTOR_Channel *channel;
  HANDLE p1[2];
  HANDLE p2[2];
  struct InitMessage *init;
  size_t slen;

  STARTUPINFOA startup;
  PROCESS_INFORMATION proc;
  char cmd[MAX_PATH + 1];
  char arg1[10], arg2[10];
  HANDLE p10_os_inh = INVALID_HANDLE_VALUE;
  HANDLE p21_os_inh = INVALID_HANDLE_VALUE;
  SECURITY_ATTRIBUTES sa;

  if (NULL == (channel = malloc (sizeof (struct EXTRACTOR_Channel))))
    {
      LOG_STRERROR ("malloc");
      return NULL;
    }
  memset (channel, 0, sizeof (struct EXTRACTOR_Channel));
  channel->mdata_size = 1024;
  if (NULL == (channel->mdata = malloc (channel->mdata_size)))
    {
      LOG_STRERROR ("malloc");
      free (channel);
      return NULL;
    }    channel->shm = shm;
  channel->plugin = plugin;
  channel->size = 0;

  sa.nLength = sizeof (sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = FALSE;

  if (0 != create_selectable_pipe (&p1[0], &p1[1], &sa, 1024, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
  {
    LOG_STRERROR ("pipe");
    free (channel);
    return NULL;
  }
  if (0 != create_selectable_pipe (&p2[0], &p2[1], &sa, 1024, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
  {
    LOG_STRERROR ("pipe");
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    free (channel);
    return NULL;
  }

  if (!DuplicateHandle (GetCurrentProcess (), p1[0], GetCurrentProcess (),
      &p10_os_inh, 0, TRUE, DUPLICATE_SAME_ACCESS)
      || !DuplicateHandle (GetCurrentProcess (), p2[1], GetCurrentProcess (),
      &p21_os_inh, 0, TRUE, DUPLICATE_SAME_ACCESS))
  {
    LOG_STRERROR ("DuplicateHandle");
    if (p10_os_inh != INVALID_HANDLE_VALUE)
      CloseHandle (p10_os_inh);
    if (p21_os_inh != INVALID_HANDLE_VALUE)
      CloseHandle (p21_os_inh);
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    CloseHandle (p2[0]);
    CloseHandle (p2[1]);
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    free (channel);
    return NULL;
  }

  memset (&proc, 0, sizeof (PROCESS_INFORMATION));
  memset (&startup, 0, sizeof (STARTUPINFOA));

  /* TODO: write our own plugin-hosting executable? rundll32, for once, has smaller than usual stack size.
   * Also, users might freak out seeing over 9000 rundll32 processes (seeing over 9000 processes named
   * "libextractor_plugin_helper" is probably less confusing).
   */
  snprintf(cmd, MAX_PATH, 
	   "rundll32.exe libextractor-3.dll,RundllEntryPoint@16 %lu %lu", 
	   p10_os_inh, p21_os_inh);
  cmd[MAX_PATH] = '\0';
  startup.cb = sizeof (STARTUPINFOA);
  if (CreateProcessA (NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL,
      &startup, &proc))
  {
    channel->hProcess = proc.hProcess;
    ResumeThread (proc.hThread);
    CloseHandle (proc.hThread);
  }
  else
  {
    LOG_STRERROR ("CreateProcess");
    CloseHandle (p1[0]);
    CloseHandle (p1[1]);
    CloseHandle (p2[0]);
    CloseHandle (p2[1]);
    free (channel);
    return NULL;
  }
  CloseHandle (p1[0]);
  CloseHandle (p2[1]);
  CloseHandle (p10_os_inh);
  CloseHandle (p21_os_inh);

  channel->cpipe_in = p1[1];
  channel->cpipe_out = p2[0];

  memset (&channel->ov_read, 0, sizeof (OVERLAPPED));
  memset (&channel->ov_write, 0, sizeof (OVERLAPPED));

  channel->ov_write_buffer = NULL;

  channel->ov_write.hEvent = CreateEvent (NULL, TRUE, TRUE, NULL);
  channel->ov_read.hEvent = CreateEvent (NULL, TRUE, TRUE, NULL);

  if (!write_plugin_data (plugin, channel))
  {
    LOG_STRERROR ("write_plugin_data");
    EXTRACTOR_IPC_channel_destroy_ (channel);
    return NULL;
  }

  slen = strlen (shm->shm_name) + 1;
  if (NULL == (init = malloc (sizeof (struct InitMessage) + slen)))
    {
      LOG_STRERROR ("malloc");
      EXTRACTOR_IPC_channel_destroy_ (channel);
      return NULL;
    }
  init->opcode = MESSAGE_INIT_STATE;
  init->reserved = 0;
  init->reserved2 = 0;
  init->shm_name_length = slen;
  init->shm_map_size = shm->shm_size;
  memcpy (&init[1], shm->shm_name, slen);
  if (sizeof (struct InitMessage) + slen !=
      EXTRACTOR_IPC_channel_send_ (channel, init,
      sizeof (struct InitMessage) + slen))
    {
      LOG ("Failed to send INIT_STATE message to plugin\n");
      EXTRACTOR_IPC_channel_destroy_ (channel);
      free (init);
      return NULL;
    }
  free (init);
  return channel;
}

/**
 * Destroy communication channel with a plugin/process.  Also
 * destroys the process.
 *
 * @param channel channel to communicate with the plugin
 */
void
EXTRACTOR_IPC_channel_destroy_ (struct EXTRACTOR_Channel *channel)
{
  int status;

  CloseHandle (channel->cpipe_out);
  CloseHandle (channel->cpipe_in);
  CloseHandle (channel->ov_read.hEvent);
  CloseHandle (channel->ov_write.hEvent);
  if (channel->ov_write_buffer != NULL)
  {
    free (channel->ov_write_buffer);
    channel->ov_write_buffer = NULL;
  }
  if (NULL != channel->plugin)
    channel->plugin->channel = NULL;
  free (channel->mdata);
  WaitForSingleObject (channel->hProcess, 1000);
  TerminateProcess (channel->hProcess, 0);
  CloseHandle (channel->hProcess);
  free (channel);
}

/**
 * Send data via the given IPC channel (blocking).
 *
 * @param channel channel to communicate with the plugin
 * @param buf data to send
 * @param size number of bytes in buf to send
 * @return -1 on error, number of bytes sent on success
 *           (never does partial writes)
 */
ssize_t
EXTRACTOR_IPC_channel_send_ (struct EXTRACTOR_Channel *channel,
			     const void *data,
			     size_t size)
{
  DWORD written;
  DWORD err;
  BOOL bresult;
  const char *cdata = data;

  if (WAIT_OBJECT_0 != WaitForSingleObject (channel->ov_write.hEvent, INFINITE))
    return -1;

  ResetEvent (channel->ov_write.hEvent);

  if (channel->old_buf != NULL)
    free (channel->old_buf);

  channel->old_buf = malloc (size);
  if (channel->old_buf == NULL)
    return -1;

  memcpy (channel->old_buf, data, size);
  written = 0;
  channel->ov_write.Offset = 0;
  channel->ov_write.OffsetHigh = 0;
  channel->ov_write.Pointer = 0;
  channel->ov_write.Internal = 0;
  channel->ov_write.InternalHigh = 0;
  bresult = WriteFile (channel->cpipe_in, channel->old_buf, size, &written, &channel->ov_write);

  if (bresult == TRUE)
  {
    SetEvent (channel->ov_write.hEvent);
    free (channel->old_buf);
    channel->old_buf = NULL;
    return written;
  }

  err = GetLastError ();
  if (err == ERROR_IO_PENDING)
    return size;
  SetEvent (channel->ov_write.hEvent);
  free (channel->old_buf);
  channel->old_buf = NULL;
  SetLastError (err);
  return -1;
}


/**
 * Receive data from any of the given IPC channels (blocking).
 * Wait for one of the plugins to reply.
 * Selects on plugin output pipes, runs 'receive_reply'
 * on each activated pipe until it gets a seek request
 * or a done message. Called repeatedly by the user until all pipes are dry or
 * broken.
 *
 * @param channels array of channels, channels that break may be set to NULL
 * @param num_channels length of the 'channels' array
 * @param proc function to call to process messages (may be called
 *             more than once)
 * @param proc_cls closure for 'proc'
 * @return -1 on error, 1 on success
 */
int
EXTRACTOR_IPC_channel_recv_ (struct EXTRACTOR_Channel **channels,
			     unsigned int num_channels,
			     EXTRACTOR_ChannelMessageProcessor proc,
			     void *proc_cls)
{
  DWORD ms;
  DWORD first_ready;
  DWORD dwresult;
  DWORD bytes_read;
  BOOL bresult;
  unsigned int i;
  unsigned int c;
  char *ndata;
  HANDLE events[MAXIMUM_WAIT_OBJECTS];
  int closed_channel;

  c = 0;
  for (i = 0; i < num_channels; i++)
  {
    if (NULL == channels[i])
      continue;
    if (MAXIMUM_WAIT_OBJECTS == c)
      return -1;
    if (WaitForSingleObject (channels[i]->ov_read.hEvent, 0) == WAIT_OBJECT_0)
    {
      ResetEvent (channels[i]->ov_read.hEvent);
      bresult = ReadFile (channels[i]->cpipe_out, &i, 0, &bytes_read, &channels[i]->ov_read);
      if (bresult == TRUE)
      {
        SetEvent (channels[i]->ov_read.hEvent);
      }
      else
      {
        DWORD err = GetLastError ();
        if (err != ERROR_IO_PENDING)
          SetEvent (channels[i]->ov_read.hEvent);
      }
    }
    events[c] = channels[i]->ov_read.hEvent;
    c++;
  }

  if (c == 0)
    return 1; /* nothing left to do! */

  ms = 500;
  first_ready = WaitForMultipleObjects (c, events, FALSE, ms);
  if (first_ready == WAIT_TIMEOUT || first_ready == WAIT_FAILED)
  {
    /* an error or timeout -> something's wrong or all plugins hung up */
    closed_channel = 0;
    for (i = 0; i < num_channels; i++)
    {
      struct EXTRACTOR_Channel *channel = channels[i];
      if (NULL == channel)
        continue;
      if (-1 == channel->plugin->seek_request)
      {
        /* plugin blocked for too long, kill the channel */
        LOG ("Channel blocked, closing channel to %s\n",
             channel->plugin->libname);
        channel->plugin->channel = NULL;
        channel->plugin->round_finished = 1;
        EXTRACTOR_IPC_channel_destroy_ (channel);
        channels[i] = NULL;
        closed_channel = 1;
      }
    }
    if (1 == closed_channel)
      return 1;
    LOG_STRERROR ("WaitForMultipleObjects");
    return -1;
  }

  i = 0;
  for (i = 0; i < num_channels; i++)
  {
    if (NULL == channels[i])
      continue;
    dwresult = WaitForSingleObject (channels[i]->ov_read.hEvent, 0);
    if (dwresult == WAIT_OBJECT_0)
    {
      int ret;
      if (channels[i]->mdata_size == channels[i]->size)
	{
	  /* not enough space, need to grow allocation (if allowed) */
	  if (MAX_META_DATA == channels[i]->mdata_size)
	    {
	      LOG ("Inbound message from channel too large, aborting\n");
	      EXTRACTOR_IPC_channel_destroy_ (channels[i]);
	      channels[i] = NULL;
	    }
	  channels[i]->mdata_size *= 2;
	  if (channels[i]->mdata_size > MAX_META_DATA)
	    channels[i]->mdata_size = MAX_META_DATA;
	  if (NULL == (ndata = realloc (channels[i]->mdata,
					channels[i]->mdata_size)))
	    {
	      LOG_STRERROR ("realloc");
	      EXTRACTOR_IPC_channel_destroy_ (channels[i]);
	      channels[i] = NULL;
	    }
	  channels[i]->mdata = ndata;
	}
      bresult = ReadFile (channels[i]->cpipe_out,
          &channels[i]->mdata[channels[i]->size],
          channels[i]->mdata_size - channels[i]->size, &bytes_read, NULL);
      if (bresult)
        ret = EXTRACTOR_IPC_process_reply_ (channels[i]->plugin,
            channels[i]->mdata, channels[i]->size + bytes_read, proc, proc_cls);
      if (!bresult || -1 == ret)
      {
        DWORD error = GetLastError ();
        SetErrnoFromWinError (error);
        if (!bresult)
          LOG_STRERROR ("ReadFile");
        EXTRACTOR_IPC_channel_destroy_ (channels[i]);
        channels[i] = NULL;
      }
      else
      {
        memmove (channels[i]->mdata, &channels[i]->mdata[ret],
          channels[i]->size + bytes_read - ret);
        channels[i]->size = channels[i]->size + bytes_read- ret;
      }
    }
  }
  return 1;
}


