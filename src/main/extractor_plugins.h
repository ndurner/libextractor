/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006, 2009, 2012 Vidyut Samanta and Christian Grothoff

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

#ifndef EXTRACTOR_PLUGINS_H
#define EXTRACTOR_PLUGINS_H

#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#ifndef WINDOWS
#include <sys/wait.h>
#include <sys/shm.h>
#endif
#include <signal.h>
#include <ltdl.h>

/**
 * Linked list of extractor plugins.  An application builds this list
 * by telling libextractor to load various keyword-extraction
 * plugins. Libraries can also be unloaded (removed from this list,
 * see EXTRACTOR_plugin_remove).
 */
struct EXTRACTOR_PluginList
{
  /**
   * This is a linked list.
   */
  struct EXTRACTOR_PluginList *next;

  /**
   * Pointer to the plugin (as returned by lt_dlopen).
   */
  void * libraryHandle;

  /**
   * Name of the library (i.e., 'libextractor_foo.so')
   */
  char *libname;

  /**
   * Short name of the plugin (i.e., 'foo')
   */
  char *short_libname;
  
  /**
   * Pointer to the function used for meta data extraction.
   */
  EXTRACTOR_extract_method extract_method;

  /**
   * Options for the plugin.
   */
  char *plugin_options;

  /**
   * Special options for the plugin
   * (as returned by the plugin's "options" method;
   * typically NULL).
   */
  const char *specials;

  /**
   * Flags to control how the plugin is executed.
   */
  enum EXTRACTOR_Options flags;

  /**
   * Process ID of the child process for this plugin. 0 for none.
   */
#if !WINDOWS
  int cpid;
#else
  HANDLE hProcess;
#endif

  /**
   * Pipe used to communicate information to the plugin child process.
   * NULL if not initialized.
   */
#if !WINDOWS
  FILE *cpipe_in;
#else
  HANDLE cpipe_in;
#endif

  /**
   * Pipe used by plugin to read from its parent.
   */
  int pipe_in;

  /**
   * A position this plugin wants us to seek to. -1 if it's finished.
   * Starts at 0;
   */
  int64_t seek_request;

#if !WINDOWS
  /**
   * ID of the shm object
   */
  int shm_id;
#else
  /**
   * Handle of the shm object
   */
  HANDLE map_handle;
#endif

  /**
   * Used to pass cfs pointer to in-process plugin in OPMODE_DECOMPRESS
   */
  void *pass_cfs;

  /**
   * Uncompressed stream size. Initially -1, until file is fully decompressed
   * (for sources that are not compressed it is set from the start).
   */
  int64_t fsize;

  /**
   * Absolute position within the stream
   */
  int64_t fpos;

  /**
   * Pointer to the shared memory segment
   */
  unsigned char *shm_ptr;

  /**
   * Number of bytes in the segment
   */
  int64_t map_size;

  /**
   * Position within the segment
   */
  int64_t shm_pos;

#if !WINDOWS
  /**
   * Pipe used to read information about extracted meta data from
   * the plugin child process.  -1 if not initialized.
   */
  int cpipe_out;
#else
  /**
   * Pipe used to read information about extracted meta data from
   * the plugin child process.  -1 if not initialized.
   */
  HANDLE cpipe_out;
#endif

#if !WINDOWS
  /**
   * Page size. Mmap offset is a multiple of this number.
   */
  long allocation_granularity;
#else
  /**
   * Page size. Mmap offset is a multiple of this number.
   */
  DWORD allocation_granularity;
#endif

#if WINDOWS
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
#endif

  /**
   * Mode of operation. One of the OPMODE_* constants
   */
  uint8_t operation_mode;

  /**
   * 1 if plugin is currently in a recursive process_requests() call,
   * 0 otherwise
   */
  int waiting_for_update;
};


/**
 * Load a plugin.
 *
 * @param plugin plugin to load
 * @return 0 on success, -1 on error
 */
int
EXTRACTOR_plugin_load_ (struct EXTRACTOR_PluginList *plugin);

#endif /* EXTRACTOR_PLUGINS_H */
