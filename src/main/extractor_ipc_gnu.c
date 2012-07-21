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
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>


/**
 * Definition of an IPC communication channel with
 * some plugin.
 */
struct EXTRACTOR_Channel
{
  /**
   * POSIX id of the shm into which data is uncompressed
   */ 
  int shm;

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
 * Opens a shared memory object (for later mmapping).
 * This is POSIX variant of the the plugin_open_* function. Shm is always memory-backed.
 * Closes a shm is already opened, closes it before opening a new one.
 *
 * @param plugin plugin context
 * @param shm_name name of the shm.
 * @return shm id (-1 on error). That is, the result of shm_open() syscall.
 */ 
static int
plugin_open_shm (struct EXTRACTOR_PluginList *plugin, 
		 const char *shm_name)
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
plugin_open_file (struct EXTRACTOR_PluginList *plugin, 
		  const char *shm_name)
{
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = open (shm_name, O_RDONLY, 0);
  return plugin->shm_id;
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
  if (plugin->shm_ptr != NULL && plugin->map_size > 0)
    munmap (plugin->shm_ptr, plugin->map_size);
  if (plugin->shm_id != -1)
    close (plugin->shm_id);
  plugin->shm_id = -1;
  plugin->map_size = 0;
  plugin->shm_ptr = NULL;
}



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
make_shm_posix (void **ptr, 
		int *shmid, 
		char *fn, 
		size_t fn_size, size_t size)
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
	     void *buf, 
	     size_t size)
{
  char *rb = buf;
  ssize_t read_result;
  size_t read_count = 0;

  while (read_count < size)
  {
    read_result = read (plugin->cpipe_out, 
			&rb[read_count], size - read_count);
    if (read_result <= 0)
      return read_result;
    read_count += read_result;
  }
  return read_count;
}


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
wait_for_reply (struct EXTRACTOR_PluginList *plugins,
		EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
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
