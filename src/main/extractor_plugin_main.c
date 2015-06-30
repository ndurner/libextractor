/*
     This file is part of libextractor.
     Copyright (C) 2012 Vidyut Samanta and Christian Grothoff

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
/**
 * @file main/extractor_plugin_main.c
 * @brief main loop for an out-of-process plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#include "extractor_common.h"
#include "extractor_datasource.h"
#include "extractor_plugins.h"
#include "extractor_ipc.h"
#include "extractor_logging.h"
#include "extractor_plugin_main.h"
#include <dirent.h>
#include <sys/types.h>
#if GNU_LINUX
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>
#endif

#if WINDOWS
#define SHM_ID HANDLE
#define INVALID_SHM_ID NULL
#else
#define SHM_ID int
#define INVALID_SHM_ID -1
#endif

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
  SHM_ID shm_id;
  
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
 * Moves current absolute buffer position to 'pos' in 'whence' mode.
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
  struct SeekRequestMessage srm;
  struct UpdateMessage um;
  uint64_t npos;
  unsigned char reply;
  uint16_t wval;

  switch (whence)
    {
    case SEEK_CUR:
      if ( (pos < 0) && (pc->read_position < - pos) )
	{
	  LOG ("Invalid seek operation\n");
	  return -1;
	}
      if ((pos > 0) && ((pc->read_position + pos < pc->read_position) ||
          (pc->read_position + pos > pc->file_size)))
	{
	  LOG ("Invalid seek operation\n");
	  return -1;
	}
      npos = (uint64_t) (pc->read_position + pos);
      wval = 0;
      break;
    case SEEK_END:
      if (pos > 0)
	{
	  LOG ("Invalid seek operation\n");
	  return -1;
	}
      if (UINT64_MAX == pc->file_size)
	{
	  wval = 2;
	  npos = (uint64_t) - pos;
	  break;
	}
      pos = (int64_t) (pc->file_size + pos);
      /* fall-through! */
    case SEEK_SET:
      if ( (pos < 0) || (pc->file_size < pos) )
	{
	  LOG ("Invalid seek operation\n");
	  return -1;
	}
      npos = (uint64_t) pos;
      wval = 0;
      break;
    default:
      LOG ("Invalid seek operation\n");
      return -1;
    }
  if ( (pc->shm_off <= npos) &&
       (pc->shm_off + pc->shm_ready_bytes > npos) &&
       (0 == wval) )
    {
      pc->read_position = npos;
      return (int64_t) npos;
    }
  /* need to seek */
  srm.opcode = MESSAGE_SEEK;
  srm.reserved = 0;
  srm.whence = wval;
  srm.requested_bytes = pc->shm_map_size;
  if (0 == wval)
    {
      if (srm.requested_bytes > pc->file_size - npos)
	srm.requested_bytes = pc->file_size - npos;
    }
  else
    {
      srm.requested_bytes = npos;
    }
  srm.file_offset = npos;
  if (-1 == EXTRACTOR_write_all_ (pc->out, &srm, sizeof (srm)))
    {
      LOG ("Failed to send MESSAGE_SEEK\n");
      return -1;
    }
  if (-1 ==
      EXTRACTOR_read_all_ (pc->in,
			   &reply, sizeof (reply)))
    {
      LOG ("Plugin `%s' failed to read response to MESSAGE_SEEK\n",
	   pc->plugin->short_libname);
      return -1;
    }
  if (MESSAGE_UPDATED_SHM != reply)    
    {
      LOG ("Unexpected reply %d to seek\n", reply);
      return -1; /* was likely a MESSAGE_DISCARD_STATE */
    }
  if (-1 == EXTRACTOR_read_all_ (pc->in, &um.reserved, sizeof (um) - 1))
    {
      LOG ("Failed to read MESSAGE_UPDATED_SHM\n");
      return -1;
    }
  pc->shm_off = um.shm_off;
  pc->shm_ready_bytes = um.shm_ready_bytes;
  pc->file_size = um.file_size;
  if (2 == wval)
    {
      /* convert offset to be absolute from beginning of the file */
      npos = pc->file_size - npos;
    }
   if ( (pc->shm_off <= npos) &&
       ((pc->shm_off + pc->shm_ready_bytes > npos) ||
       (pc->file_size == pc->shm_off)) )
    {
      pc->read_position = npos;
      return (int64_t) npos;
    }
  /* oops, serious missunderstanding, we asked to seek
     and then were notified about a different position!? */
  LOG ("Plugin `%s' got invalid MESSAGE_UPDATED_SHM in response to my %d-seek (%llu not in %llu-%llu)\n",
       pc->plugin->short_libname,
       (int) wval,
       (unsigned long long) npos,
       (unsigned long long) pc->shm_off,
       (unsigned long long) pc->shm_off + pc->shm_ready_bytes);
  return -1;
}


/**
 * Fills 'data' with a pointer to the data buffer.
 *
 * @param plugin plugin context
 * @param data location to store data pointer
 * @param count number of bytes to read
 * @return number of bytes (<= count) avalable in 'data', -1 on error
 */
static ssize_t
plugin_env_read (void *cls,
		 void **data, size_t count)
{
  struct ProcessingContext *pc = cls;
  unsigned char *dp;
  
  *data = NULL;
  if ( (count + pc->read_position > pc->file_size) ||
       (count + pc->read_position < pc->read_position) )
    count = pc->file_size - pc->read_position;
  if ((((pc->read_position >= pc->shm_off + pc->shm_ready_bytes) &&
      (pc->read_position < pc->file_size)) ||
      (pc->read_position < pc->shm_off)) &&
      (-1 == plugin_env_seek (pc, pc->read_position, SEEK_SET)))
    {
      LOG ("Failed to seek to satisfy read\n");
      return -1; 
    }
  if (pc->read_position + count > pc->shm_off + pc->shm_ready_bytes)
    count = pc->shm_off + pc->shm_ready_bytes - pc->read_position;
  dp = pc->shm;
  *data = &dp[pc->read_position - pc->shm_off];
  pc->read_position += count;
  return count;
}


/**
 * Provide the overall file size to plugins.
 *
 * @param cls the 'struct ProcessingContext'
 * @return overall file size of the current file
 */
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
  struct MetaMessage mm;
  size_t mime_len;
  unsigned char reply;

  if (data_len > MAX_META_DATA)
    return 0; /* skip, too large */
  if (NULL == data_mime_type)
    mime_len = 0;
  else
    mime_len = strlen (data_mime_type) + 1;
  if (mime_len > UINT16_MAX)
    mime_len = UINT16_MAX;
  mm.opcode = MESSAGE_META;
  mm.reserved = 0;
  mm.meta_type = type;
  mm.meta_format = (uint16_t) format;
  mm.mime_length = (uint16_t) mime_len;
  mm.value_size = (uint32_t) data_len;
  if ( (sizeof (mm) != 
	EXTRACTOR_write_all_ (pc->out,
			      &mm, sizeof (mm))) ||
       (mime_len !=
	EXTRACTOR_write_all_ (pc->out, 
			      data_mime_type, mime_len)) ||
       (data_len !=
	EXTRACTOR_write_all_ (pc->out, 
			      data, data_len)) )
    {
      LOG ("Failed to send meta message\n");
      return 1;
    }
  if (-1 ==
      EXTRACTOR_read_all_ (pc->in,
			   &reply, sizeof (reply)))
    {
      LOG ("Failed to read response to meta message\n");
      return 1;
    }
  if (MESSAGE_DISCARD_STATE == reply)
    return 1;
  if (MESSAGE_CONTINUE_EXTRACTING != reply)
    {
      LOG ("Received unexpected reply to meta data: %d\n", reply);
      return 1;
    }
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
    {
      LOG ("Cannot handle 'init' message, have already been initialized\n");
      return -1;
    }
  if (sizeof (struct InitMessage) - 1
      != EXTRACTOR_read_all_ (pc->in,
		   &init.reserved,
		   sizeof (struct InitMessage) - 1))
    {
      LOG ("Failed to read 'init' message\n");
      return -1;
    }
  if (init.shm_name_length > MAX_SHM_NAME)
    {
      LOG ("Invalid 'init' message\n");
      return -1;
    }
  {
    char shm_name[init.shm_name_length + 1];

    if (init.shm_name_length 
	!= EXTRACTOR_read_all_ (pc->in,
		     shm_name,
		     init.shm_name_length))
      {
	LOG ("Failed to read 'init' message\n");
	return -1;
      }
    shm_name[init.shm_name_length] = '\0';

    pc->shm_map_size = init.shm_map_size;
#if WINDOWS
    /* FIXME: storing pointer in an int */
    pc->shm_id = OpenFileMapping (FILE_MAP_READ, FALSE, shm_name);
    if (NULL == pc->shm_id)
      return -1;
    pc->shm = MapViewOfFile (pc->shm_id, FILE_MAP_READ, 0, 0, 0);
    if (NULL == pc->shm)
    {
      CloseHandle (pc->shm_id);
      return -1;
    }
#else
    pc->shm_id = shm_open (shm_name, O_RDONLY, 0);
    if (-1 == pc->shm_id)
      {
	LOG_STRERROR_FILE ("open", shm_name);
	return -1;
      }
    pc->shm = mmap (NULL,
		    pc->shm_map_size,
		    PROT_READ,
		    MAP_SHARED,
		    pc->shm_id, 0);
    if ( ((void*) -1) == pc->shm)
      {
	LOG_STRERROR_FILE ("mmap", shm_name);
	return -1;
      }
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
  char done;

  if (sizeof (struct StartMessage) - 1
      != EXTRACTOR_read_all_ (pc->in,
		   &start.reserved,
		   sizeof (struct StartMessage) - 1))
    {
      LOG ("Failed to read 'start' message\n");
      return -1;
    }
  pc->shm_ready_bytes = start.shm_ready_bytes;
  pc->file_size = start.file_size;
  pc->read_position = 0;
  pc->shm_off = 0;
  ec.cls = pc;
  ec.config = pc->plugin->plugin_options;
  ec.read = &plugin_env_read;
  ec.seek = &plugin_env_seek;
  ec.get_size = &plugin_env_get_size;
  ec.proc = &plugin_env_send_proc;
  pc->plugin->extract_method (&ec);
  done = MESSAGE_DONE;
  if (-1 == EXTRACTOR_write_all_ (pc->out, &done, sizeof (done)))
    {
      LOG ("Failed to write 'done' message\n");
      return -1;
    }
  if ( (NULL != pc->plugin->specials) &&
       (NULL != strstr (pc->plugin->specials, "force-kill")) )
    {
      /* we're required to die after each file since this
	 plugin only supports a single file at a time */
#if !WINDOWS
      fsync (pc->out);
#else
      _commit (pc->out);
#endif
      _exit (0);
    }
  return 0;
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

      if (1 != EXTRACTOR_read_all_ (pc->in, &code, 1))
	{
	  LOG ("Failed to read next request\n");
	  break;
	}
      switch (code)
	{
	case MESSAGE_INIT_STATE:
	  if (0 != handle_init_message (pc))
	    {
	      LOG ("Failure to handle INIT\n");
	      return;
	    }
	  break;
	case MESSAGE_EXTRACT_START:
	  if (0 != handle_start_message (pc))
	    {
	      LOG ("Failure to handle START\n");
	      return;
	    }
	  break;
	case MESSAGE_UPDATED_SHM:
	  LOG ("Illegal message\n");
	  /* not allowed here, we're not waiting for SHM to move! */
	  return;
	case MESSAGE_DISCARD_STATE:
	  /* odd, we're already in the start state... */
	  continue;
	default:
	  LOG ("Received invalid messag %d\n", (int) code);
	  /* error, unexpected message */
	  return;
	}
    }
}


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

#ifndef WINDOWS
  fd = open ("/dev/null", flags);
#else
  fd = open ("\\\\?\\NUL", flags);
#endif
  if (-1 == fd)
    {
      LOG_STRERROR_FILE ("open", "/dev/null");
      return; /* good luck */
    }
  if (fd == target_fd)
    return; /* already done */
  if (-1 == dup2 (fd, target_fd))
  {
    LOG_STRERROR ("dup2");
    (void) close (fd);
    return; /* good luck */
  }
  /* close original result from 'open' */
  if (0 != close (fd))
    LOG_STRERROR ("close");
}


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
      if (0 != close (2))
	LOG_STRERROR ("close");
      open_dev_null (2, O_WRONLY);
    }
  if ( (NULL != plugin->specials) &&
       (NULL != strstr (plugin->specials, "close-stdout")))
    {
      if (0 != close (1))
	LOG_STRERROR ("close");
      open_dev_null (1, O_WRONLY);
    }
  pc.plugin = plugin;
  pc.in = in;
  pc.out = out;
  pc.shm_id = INVALID_SHM_ID;
  pc.shm = NULL;
  pc.shm_map_size = 0;
  process_requests (&pc);
  LOG ("IPC error; plugin `%s' terminates!\n",
       plugin->short_libname);
#if WINDOWS
  if (NULL != pc.shm)
    UnmapViewOfFile (pc.shm);
  if (NULL != pc.shm_id)
    CloseHandle (pc.shm_id);
#else
  if ( (NULL != pc.shm) &&
       (((void*) 1) != pc.shm) )
    munmap (pc.shm, pc.shm_map_size);
  if (-1 != pc.shm_id)
    {
      if (0 != close (pc.shm_id))
	LOG_STRERROR ("close");
    }
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

  // FIXME: check for errors from 'EXTRACTOR_read_all_'!
  if (NULL == (ret = malloc (sizeof (struct EXTRACTOR_PluginList))))
    {
      LOG_STRERROR ("malloc");
      return NULL;
    }
  memset (ret, 0, sizeof (struct EXTRACTOR_PluginList));
  /*GetSystemInfo (&si);
  ret->allocation_granularity = si.dwAllocationGranularity;*/
  EXTRACTOR_read_all_ (fd, &i, sizeof (size_t));
  if (NULL == (ret->libname = malloc (i)))
    {
      free (ret);
      return NULL;
    }
  EXTRACTOR_read_all_ (fd, ret->libname, i);
  ret->libname[i - 1] = '\0';
  EXTRACTOR_read_all_ (fd, &i, sizeof (size_t));
  if (NULL == (ret->short_libname = malloc (i)))
    {
      free (ret->libname);
      free (ret);
      return NULL;
    }
  EXTRACTOR_read_all_ (fd, ret->short_libname, i);
  ret->short_libname[i - 1] = '\0';
  EXTRACTOR_read_all_ (fd, &i, sizeof (size_t));
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
  EXTRACTOR_read_all_ (fd, ret->plugin_options, i);
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
  EXTRACTOR_plugin_main_ (plugin, in, out);
  close (in);
  close (out);
  /* libgobject may crash us hard if we LoadLibrary() it directly or
   * indirectly, and then exit normally (causing FreeLibrary() to be
   * called by the OS) or call FreeLibrary() on it directly or
   * indirectly.
   * By terminating here we alleviate that problem.
   */
  TerminateProcess (GetCurrentProcess (), 0);
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
