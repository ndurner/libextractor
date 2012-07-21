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


/**
 * Definition of an IPC communication channel with
 * some plugin.
 */
struct EXTRACTOR_Channel
{

  /**
   * W32 handle of the shm into which data is uncompressed
   */ 
  HANDLE shm;

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


};



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
    UnmapViewOfFile (plugin->shm_ptr);
  plugin->shm_ptr = NULL;
  if (INVALID_HANDLE_VALUE == plugin_open_shm (plugin, shm_name))
    return 1;
  plugin->fsize = fsize;
  plugin->shm_pos = 0;
  plugin->fpos = 0;
  return 0;
}

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
static HANDLE
plugin_open_shm (struct EXTRACTOR_PluginList *plugin, 
		 const char *shm_name)
{
  if (plugin->map_handle != 0)
    CloseHandle (plugin->map_handle);
  plugin->map_handle = OpenFileMapping (FILE_MAP_READ, FALSE, shm_name);
  return plugin->map_handle;
}


/**
 * Another name for plugin_open_shm().
 */ 
static HANDLE
plugin_open_file (struct EXTRACTOR_PluginList *plugin, 
		  const char *shm_name)
{
  return plugin_open_shm (plugin, shm_name);
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
    UnmapViewOfFile (plugin->shm_ptr);
  plugin->shm_ptr = NULL;
  if (INVALID_HANDLE_VALUE == plugin_open_shm (plugin, shm_name))
    return 1;
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
  if (plugin->shm_ptr != NULL)
    UnmapViewOfFile (plugin->shm_ptr);
  if (plugin->map_handle != 0)
    CloseHandle (plugin->map_handle);
  plugin->map_handle = 0;
  plugin->map_size = 0;
  plugin->shm_ptr = NULL;
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
write_to_pipe (HANDLE h, 
	       OVERLAPPED *ov,
	       unsigned char *buf, size_t size, 
	       unsigned char **old_buf)
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
  {
    SYSTEM_INFO si;
    GetSystemInfo (&si);
    ret->allocation_granularity = si.dwAllocationGranularity;
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

  /* TODO: write our own plugin-hosting executable? rundll32, for once, has smaller than usual stack size.
   * Also, users might freak out seeing over 9000 rundll32 processes (seeing over 9000 processes named
   * "libextractor_plugin_helper" is probably less confusing).
   */
  snprintf(cmd, MAX_PATH + 1, 
	   "rundll32.exe libextractor-3.dll,RundllEntryPoint@16 %lu %lu", 
	   p10_os_inh, p21_os_inh);
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
make_shm_w32 (void **ptr, 
	      HANDLE *map, 
	      char *fn,
	      size_t fn_size, size_t size)
{
  const char *tpath = "Local\\";

  snprintf (fn, fn_size, 
	    "%slibextractor-shm-%u-%u", 
	    tpath, getpid(),
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



#define plugin_write(plug, buf, size) write_all (fileno (plug->cpipe_in), buf, size)


void CALLBACK 
RundllEntryPoint (HWND hwnd, 
		  HINSTANCE hinst, 
		  LPSTR lpszCmdLine, 
		  int nCmdShow)
{
  intptr_t in_h;
  intptr_t out_h;
  int in;
  int out;

  sscanf (lpszCmdLine, "%lu %lu", &in_h, &out_h);
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
  return RundllEntryPoint (hwnd, hinst, lpszCmdLine, nCmdShow);
}



/**
 * Receive 'size' bytes from plugin, store them in 'buf'
 *
 * @param plugin plugin context
 * @param buf buffer to fill
 * @param size number of bytes to read
 * @return number of bytes read, 0 on EOS, < 0 on error
 */
static int 
plugin_read (struct EXTRACTOR_PluginList *plugin, 
	     void *buf, size_t size)
{
  char *rb = buf;
  DWORD bytes_read;
  size_t read_count = 0;

  while (read_count < size)
  {
    if (! ReadFile (plugin->cpipe_out, 
		    &rb[read_count], size - read_count, 
		    &bytes_read, NULL))
      return -1;
    read_count += bytes_read;
  }
  return read_count;
}


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
wait_for_reply (struct EXTRACTOR_PluginList *plugins, 
		EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int result;
  DWORD ms;
  DWORD first_ready;
  DWORD dwresult;
  DWORD bytes_read;
  BOOL bresult;
  unsigned int i;
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


