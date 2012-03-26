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
#ifndef WINDOWS
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

/**
 * Set to 1 to get failure info,
 * 2 for actual debug info.
 */ 
#define DEBUG 1


/**
 * Stop the child process of this plugin.
 */
static void
stop_process (struct EXTRACTOR_PluginList *plugin)
{
  int status;
#ifdef WINDOWS
  HANDLE process;
#endif

#if DEBUG
#ifndef WINDOWS
  if (plugin->cpid == -1)
#else
  if (plugin->hProcess == INVALID_HANDLE_VALUE)
#endif
    fprintf (stderr,
	     "Plugin `%s' choked on this input\n",
	     plugin->short_libname);
#endif
#ifndef WINDOWS
  if ( (plugin->cpid == -1) ||
       (plugin->cpid == 0) )
    return;
  kill (plugin->cpid, SIGKILL);
  waitpid (plugin->cpid, &status, 0);
  plugin->cpid = -1;
  close (plugin->cpipe_out);
  fclose (plugin->cpipe_in);
#else
  if (plugin->hProcess == INVALID_HANDLE_VALUE ||
      plugin->hProcess == NULL)
  return;
  TerminateProcess (plugin->hProcess, 0);
  CloseHandle (plugin->hProcess);
  plugin->hProcess = INVALID_HANDLE_VALUE;
  close (plugin->cpipe_out);
  fclose (plugin->cpipe_in);
#endif
  plugin->cpipe_out = -1;
  plugin->cpipe_in = NULL;
}


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
  return 0;
}


static int
read_all (int fd,
	  void *buf,
	  size_t size)
{
  char *data = buf;
  size_t off = 0;
  ssize_t ret;
  
  while (off < size)
    {
      ret = read (fd, &data[off], size - off);
      if (ret <= 0)
	return -1;
      off += ret;
    }
  return 0;
}


/**
 * Header used for our IPC replies.  A header
 * with all fields being zero is used to indicate
 * the end of the stream.
 */
struct IpcHeader
{
  enum EXTRACTOR_MetaType type;
  enum EXTRACTOR_MetaFormat format;
  size_t data_len;
  size_t mime_len;
};


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

  if (data_mime_type == NULL)
    mime_len = 0;
  else
    mime_len = strlen (data_mime_type) + 1;
  if (mime_len > MAX_MIME_LEN)
    mime_len = MAX_MIME_LEN;
  hdr.type = type;
  hdr.format = format;
  hdr.data_len = data_len;
  hdr.mime_len = mime_len;
  if ( (hdr.type == 0) &&
       (hdr.format == 0) &&
       (hdr.data_len == 0) &&
       (hdr.mime_len == 0) )
    return 0; /* better skip this one, would signal termination... */    
  if ( (0 != write_all (*cpipe_out, &hdr, sizeof(hdr))) ||
       (0 != write_all (*cpipe_out, data_mime_type, mime_len)) ||
       (0 != write_all (*cpipe_out, data, data_len)) )
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
process_requests (struct EXTRACTOR_PluginList *plugin,
		  int in,
		  int out)
{
  char hfn[256];
  char tfn[256];
  char sze[256];
  size_t hfn_len;
  size_t tfn_len;
  size_t sze_len;
  char *fn;
  FILE *fin;
  void *ptr;
  int shmid;
  struct IpcHeader hdr;
  size_t size;
  int want_tail;
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
      fprintf (stderr,
	       "Plugin `%s' failed to load!\n",
	       plugin->short_libname);
#endif
      return;
    }  
  want_tail = 0;
  if ( (plugin->specials != NULL) &&
       (NULL != strstr (plugin->specials,
			"want-tail")) )
    {
      want_tail = 1;
    }
  if ( (plugin->specials != NULL) &&
       (NULL != strstr (plugin->specials,
			"close-stderr")) )
    {
      close (2);
    }
  if ( (plugin->specials != NULL) &&
       (NULL != strstr (plugin->specials,
			"close-stdout")) )
    {
      close (1);
    }

  memset (&hdr, 0, sizeof (hdr));
  fin = fdopen (in, "r");
  if (fin == NULL)
    {
      close (in);
      close (out);
      return;
    }
  while (NULL != fgets (hfn, sizeof(hfn), fin))
    {
      hfn_len = strlen (hfn);
      if (hfn_len <= 1)
	break;
      ptr = NULL;
      hfn[--hfn_len] = '\0'; /* kill newline */
      if (NULL == fgets (tfn, sizeof(tfn), fin))
	break;
      if ('!' != tfn[0])
	break;
      tfn_len = strlen (tfn);
      tfn[--tfn_len] = '\0'; /* kill newline */
      if ( (want_tail) &&
	   (tfn_len > 1) )
	{
	  fn = &tfn[1];
	}
      else
	{
	  fn = hfn;	
	}
      if (NULL == fgets (sze, sizeof(sze), fin))
	break;
      if ('s' != sze[0])
	break;
      sze_len = strlen (sze);
      sze[--sze_len] = '\0'; /* kill newline */
      size = strtol (&sze[1], NULL, 10);
      if (size == LONG_MIN || size == LONG_MAX || size == 0)
        break;
      do_break = 0;
#ifndef WINDOWS
      if ( (-1 != (shmid = shm_open (fn, O_RDONLY, 0))) &&
	   (SIZE_MAX != (size = lseek (shmid, 0, SEEK_END))) &&
	   (NULL != (ptr = mmap (NULL, size, PROT_READ, MAP_SHARED, shmid, 0))) &&
	   (ptr != (void*) -1) )
#else
      /* Despite the obvious, this must be READWRITE, not READONLY */
      map = OpenFileMapping (PAGE_READWRITE, FALSE, fn);
      ptr = MapViewOfFile (map, FILE_MAP_READ, 0, 0, 0);
      if (ptr != NULL)
      {
        if (0 == VirtualQuery (ptr, &mi, sizeof (mi)) || mi.RegionSize < size)
        {
          UnmapViewOfFile (ptr);
          ptr = NULL;
        }
      }
      if (ptr != NULL)
#endif
	{
	  if ( ( (plugin->extractMethod != NULL) &&
		 (0 != plugin->extractMethod (ptr,
					      size,
					      &transmit_reply,
					      &out,
					      plugin->plugin_options)) ) ||
	       (0 != write_all (out, &hdr, sizeof(hdr))) )
	    do_break = 1;
	}
#ifndef WINDOWS
      if ( (ptr != NULL) &&
	   (ptr != (void*) -1) )
	munmap (ptr, size);
      if (-1 != shmid)
	close (shmid);
#else
      if (ptr != NULL && ptr != (void*) -1)
        UnmapViewOfFile (ptr);
      if (map != NULL)
        CloseHandle (map);
#endif
      if (do_break)
	break;
      if ( (plugin->specials != NULL) &&
	   (NULL != strstr (plugin->specials,
			    "force-kill")) )
	{
	  /* we're required to die after each file since this
	     plugin only supports a single file at a time */
	  _exit (0);
	}
    }
  fclose (fin);
  close (out);
}


#ifdef WINDOWS
static void
write_plugin_data (int fd, const struct EXTRACTOR_PluginList *plugin)
{
  size_t i;
  DWORD len;
  char *str;

  i = strlen (plugin->libname) + 1;
  write (fd, &i, sizeof (size_t));
  write (fd, plugin->libname, i);
  i = strlen (plugin->short_libname) + 1;
  write (fd, &i, sizeof (size_t));
  write (fd, plugin->short_libname, i);
  if (plugin->plugin_options != NULL)
    {
      i = strlen (plugin->plugin_options) + 1;
      str = plugin->plugin_options;
    }
  else
    {
      i = 0;
    }
  write (fd, &i, sizeof (size_t));
  if (i > 0)
    write (fd, str, i);
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

  read (fd, &i, sizeof (size_t));
  ret->short_libname = malloc (i);
  if (ret->short_libname == NULL)
    {
      free (ret->libname);
      free (ret);
      return NULL;
    }
  read (fd, ret->short_libname, i);

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
    }
  return ret;
}


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
 * Start the process for the given plugin.
 */ 
static void
start_process (struct EXTRACTOR_PluginList *plugin)
{
#if !WINDOWS
  int p1[2];
  int p2[2];
  pid_t pid;
  int status;

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
#else
  int p1[2];
  int p2[2];
  STARTUPINFO startup;
  PROCESS_INFORMATION proc;
  char cmd[MAX_PATH + 1];
  char arg1[10], arg2[10];
  HANDLE p10_os = INVALID_HANDLE_VALUE, p21_os = INVALID_HANDLE_VALUE;
  HANDLE p10_os_inh = INVALID_HANDLE_VALUE, p21_os_inh = INVALID_HANDLE_VALUE;

  plugin->hProcess = NULL;
  if (0 != _pipe (p1, 0, _O_BINARY | _O_NOINHERIT))
    {
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return;
    }
  if (0 != _pipe (p2, 0, _O_BINARY | _O_NOINHERIT))
    {
      close (p1[0]);
      close (p1[1]);
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return;
    }

  memset (&startup, 0, sizeof (STARTUPINFO));

  p10_os = (HANDLE) _get_osfhandle (p1[0]);
  p21_os = (HANDLE) _get_osfhandle (p2[1]);

  if (p10_os == INVALID_HANDLE_VALUE || p21_os == INVALID_HANDLE_VALUE)
  {
    close (p1[0]);
    close (p1[1]);
    close (p2[0]);
    close (p2[1]);
    plugin->flags = EXTRACTOR_OPTION_DISABLED;
    return;
  }

  if (!DuplicateHandle (GetCurrentProcess (), p10_os, GetCurrentProcess (),
      &p10_os_inh, 0, TRUE, DUPLICATE_SAME_ACCESS)
      || !DuplicateHandle (GetCurrentProcess (), p21_os, GetCurrentProcess (),
      &p21_os_inh, 0, TRUE, DUPLICATE_SAME_ACCESS))
  {
    if (p10_os_inh != INVALID_HANDLE_VALUE)
      CloseHandle (p10_os_inh);
    if (p21_os_inh != INVALID_HANDLE_VALUE)
      CloseHandle (p21_os_inh);
    close (p1[0]);
    close (p1[1]);
    close (p2[0]);
    close (p2[1]);
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
      close (p1[0]);
      close (p1[1]);
      close (p2[0]);
      close (p2[1]);
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return;
    }
  close (p1[0]);
  close (p2[1]);
  CloseHandle (p10_os_inh);
  CloseHandle (p21_os_inh);

  write_plugin_data (p1[1], plugin);

  plugin->cpipe_in = fdopen (p1[1], "w");
  if (plugin->cpipe_in == NULL)
    {
      perror ("fdopen");
      TerminateProcess (plugin->hProcess, 0);
      WaitForSingleObject (plugin->hProcess, INFINITE);
      CloseHandle (plugin->hProcess);
      close (p1[1]);
      close (p2[0]);
      plugin->hProcess = INVALID_HANDLE_VALUE;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return;
    }
  plugin->cpipe_out = p2[0];
#endif
}


/**
 * Extract meta data using the given plugin, running the
 * actual code of the plugin out-of-process.
 *
 * @param plugin which plugin to call
 * @param size size of the file mapped by shmfn or tshmfn
 * @param shmfn file name of the shared memory segment
 * @param tshmfn file name of the shared memory segment for the end of the data
 * @param proc function to call on the meta data
 * @param proc_cls cls for proc
 * @return 0 if proc did not return non-zero
 */
static int
extract_oop (struct EXTRACTOR_PluginList *plugin,
             size_t size,
	     const char *shmfn,
	     const char *tshmfn,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  struct IpcHeader hdr;
  char mimetype[MAX_MIME_LEN + 1];
  char *data;

#ifndef WINDOWS
  if (plugin->cpid == -1)
#else
  if (plugin->hProcess == INVALID_HANDLE_VALUE)
#endif
    return 0;
  if (0 >= fprintf (plugin->cpipe_in, 
		    "%s\n",
		    shmfn))
    {
      stop_process (plugin);
#ifndef WINDOWS
      plugin->cpid = -1;
#else
      plugin->hProcess = INVALID_HANDLE_VALUE;
#endif
      if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
	plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return 0;
    }
  if (0 >= fprintf (plugin->cpipe_in, 
		    "!%s\n",
		    (tshmfn != NULL) ? tshmfn : ""))
    {
      stop_process (plugin);
#ifndef WINDOWS
      plugin->cpid = -1;
#else
      plugin->hProcess = INVALID_HANDLE_VALUE;
#endif
      if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
	plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return 0;
    }
  if (0 >= fprintf (plugin->cpipe_in, 
		    "s%lu\n",
		    size))
    {
      stop_process (plugin);
#ifndef WINDOWS
      plugin->cpid = -1;
#else
      plugin->hProcess = INVALID_HANDLE_VALUE;
#endif
      if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
	plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return 0;
    }
  fflush (plugin->cpipe_in);
  while (1)
    {
      if (0 != read_all (plugin->cpipe_out,
			 &hdr,
			 sizeof(hdr)))
	{
	  stop_process (plugin);
#ifndef WINDOWS
      plugin->cpid = -1;
#else
      plugin->hProcess = INVALID_HANDLE_VALUE;
#endif
	  if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
	    plugin->flags = EXTRACTOR_OPTION_DISABLED;
	  return 0;
	}
      if  ( (hdr.type == 0) &&
	    (hdr.format == 0) &&
	    (hdr.data_len == 0) &&
	    (hdr.mime_len == 0) )
	break;
      if (hdr.mime_len > MAX_MIME_LEN)
	{
	  stop_process (plugin);	  
#ifndef WINDOWS
      plugin->cpid = -1;
#else
      plugin->hProcess = INVALID_HANDLE_VALUE;
#endif
	  if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
	    plugin->flags = EXTRACTOR_OPTION_DISABLED;
	  return 0;
	}
      data = malloc (hdr.data_len);
      if (data == NULL)
	{
	  stop_process (plugin);
	  return 1;
	}
      if ( (0 != (read_all (plugin->cpipe_out,
			    mimetype,
			    hdr.mime_len))) ||
	   (0 != (read_all (plugin->cpipe_out,
			    data,
			    hdr.data_len))) )
	{
	  stop_process (plugin);
#ifndef WINDOWS
      plugin->cpid = -1;
#else
      plugin->hProcess = INVALID_HANDLE_VALUE;
#endif
	  free (data);
	  if (plugin->flags != EXTRACTOR_OPTION_DEFAULT_POLICY)
	    plugin->flags = EXTRACTOR_OPTION_DISABLED;
	  return 0;
	}	   
      mimetype[hdr.mime_len] = '\0';
      if ( (proc != NULL) &&
	   (0 != proc (proc_cls, 
		       plugin->short_libname,
		       hdr.type,
		       hdr.format,
		       mimetype,
		       data,
		       hdr.data_len)) )
	proc = NULL;	
      free (data);
    }
  if (NULL == proc)
    return 1;
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
make_shm (int is_tail,
	  void **ptr,
#ifndef WINDOWS
	  int *shmid,
#else
	  HANDLE *map,
#endif	  
	  char *fn,
	  size_t fn_size,
	  size_t size)
{
  const char *tpath;
#ifdef WINDOWS
  tpath = "Local\\";
#elif SOMEBSD
  const char *tpath;
  /* this works on FreeBSD, not sure about others... */
  tpath = getenv ("TMPDIR");
  if (tpath == NULL)
    tpath = "/tmp/";
#else
  tpath = "/"; /* Linux */
#endif 
  snprintf (fn,
	    fn_size,
	    "%slibextractor-%sshm-%u-%u",
	    tpath,
	    (is_tail) ? "t" : "",
	    getpid(),
	    (unsigned int) RANDOM());
#ifndef WINDOWS
  *shmid = shm_open (fn, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  *ptr = NULL;
  if (-1 == (*shmid))
    return 1;    
  if ( (0 != ftruncate (*shmid, size)) ||
       (NULL == (*ptr = mmap (NULL, size, PROT_WRITE, MAP_SHARED, *shmid, 0))) ||
       (*ptr == (void*) -1) )
    {
      close (*shmid);
      *shmid = -1;
      shm_unlink (fn);
      return 1;
    }
  return 0;
#else
  *map = CreateFileMapping (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, fn);
  *ptr = MapViewOfFile (*map, FILE_MAP_WRITE, 0, 0, size);
  if (*ptr == NULL)
    {
      CloseHandle (*map);
      return 1;
    }
  return 0;
#endif
}


/**
 * Extract keywords using the given set of plugins.
 *
 * @param plugins the list of plugins to use
 * @param data data to process, never NULL
 * @param size number of bytes in data, ignored if data is NULL
 * @param tdata end of file data, or NULL
 * @param tsize number of bytes in tdata
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
extract (struct EXTRACTOR_PluginList *plugins,
	 const char * data,
	 size_t size,
	 const char * tdata,
	 size_t tsize,
	 EXTRACTOR_MetaDataProcessor proc,
	 void *proc_cls) 
{
  struct EXTRACTOR_PluginList *ppos;
  enum EXTRACTOR_Options flags;
  void *ptr;
  void *tptr;
  char fn[255];
  char tfn[255];
  int want_shm;
  int want_tail;
#ifndef WINDOWS
  int shmid;
  int tshmid;
#else
  HANDLE map;
  HANDLE tmap;
#endif

  want_shm = 0;
  ppos = plugins;
  while (NULL != ppos)
    {      
      switch (ppos->flags)
	{
	case EXTRACTOR_OPTION_DEFAULT_POLICY:
#ifndef WINDOWS
	  if ( (0 == ppos->cpid) ||
	       (-1 == ppos->cpid) )
#else
	  if (ppos->hProcess == NULL || ppos->hProcess == INVALID_HANDLE_VALUE)
#endif
	    start_process (ppos);
	  want_shm = 1;
	  break;
	case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
#ifndef WINDOWS
	  if (0 == ppos->cpid)
#else
	  if (ppos->hProcess == NULL)
#endif
	    start_process (ppos);
	  want_shm = 1;
	  break;
	case EXTRACTOR_OPTION_IN_PROCESS:
	  break;
	case EXTRACTOR_OPTION_DISABLED:
	  break;
	}      
      ppos = ppos->next;
    }
  ptr = NULL;
  tptr = NULL;
  if (want_shm)
    {
      if (size > MAX_READ)
	size = MAX_READ;
      if (0 == make_shm (0, 
			 &ptr,
#ifndef WINDOWS
			 &shmid,
#else
			 &map,
#endif
			 fn, sizeof(fn), size))
	{
	  memcpy (ptr, data, size);      
	  if ( (tdata != NULL) &&
	       (0 == make_shm (1,
			       &tptr,
#ifndef WINDOWS
			       &tshmid,
#else
			       &tmap,
#endif
			       tfn, sizeof(tfn), tsize)) )
	    {
	      memcpy (tptr, tdata, tsize);      
	    }
	  else
	    {
	      tptr = NULL;
	    }
	}
      else
	{
	  want_shm = 0;
	}	    
    }
  ppos = plugins;
  while (NULL != ppos)
    {
      flags = ppos->flags;
      if (! want_shm)
	flags = EXTRACTOR_OPTION_IN_PROCESS;
      switch (flags)
	{
	case EXTRACTOR_OPTION_DEFAULT_POLICY:
	  if (0 != extract_oop (ppos, (tptr != NULL) ? tsize : size, fn, 
				(tptr != NULL) ? tfn : NULL,
				proc, proc_cls))
	    {
	      ppos = NULL;
	      break;
	    }
#ifndef WINDOWS
	  if (ppos->cpid == -1)
#else
      if (ppos->hProcess == INVALID_HANDLE_VALUE)
#endif
	    {
	      start_process (ppos);
	      if (0 != extract_oop (ppos, (tptr != NULL) ? tsize : size, fn, 
				    (tptr != NULL) ? tfn : NULL,
				    proc, proc_cls))
		{
		  ppos = NULL;
		  break;
		}
	    }
	  break;
	case EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART:
	  if (0 != extract_oop (ppos, (tptr != NULL) ? tsize : size, fn, 
				(tptr != NULL) ? tfn : NULL,
				proc, proc_cls))
	    {
	      ppos = NULL;
	      break;
	    }
	  break;
	case EXTRACTOR_OPTION_IN_PROCESS:	  	  
	  want_tail = ( (ppos->specials != NULL) &&
			(NULL != strstr (ppos->specials,
					 "want-tail")));
	  if (NULL == ppos->extractMethod) 
	    plugin_load (ppos);	    
	  if ( ( (ppos->specials == NULL) ||
		 (NULL == strstr (ppos->specials,
				  "oop-only")) ) )
	    {
	      if (want_tail)
		{
		  if ( (NULL != ppos->extractMethod) &&
		       (tdata != NULL) &&
		       (0 != ppos->extractMethod (tdata, 
						  tsize, 
						  proc, 
						  proc_cls,
						  ppos->plugin_options)) )
		    {
		      ppos = NULL;
		      break;
		    }
		}
	      else
		{
		  if ( (NULL != ppos->extractMethod) &&
		       (0 != ppos->extractMethod (data, 
						  size, 
						  proc, 
						  proc_cls,
						  ppos->plugin_options)) )
		    {
		      ppos = NULL;
		      break;
		    }
		}
	    }
	  break;
	case EXTRACTOR_OPTION_DISABLED:
	  break;
	}      
      if (ppos == NULL)
	break;
      ppos = ppos->next;
    }
  if (want_shm)
    {
#ifndef WINDOWS
      if (NULL != ptr)
	munmap (ptr, size);
      if (shmid != -1)
	close (shmid);
      shm_unlink (fn);
      if (NULL != tptr)
	{
	  munmap (tptr, tsize);
	  shm_unlink (tfn);
	  if (tshmid != -1)
	    close (tshmid);
	}
#else
      UnmapViewOfFile (ptr);
      CloseHandle (map);
      if (tptr != NULL)
	{
	  UnmapViewOfFile (tptr);
	  CloseHandle (tmap);
	}
#endif
    }
}


/**
 * If the given data is compressed using gzip or bzip2, decompress
 * it.  Run 'extract' on the decompressed contents (or the original
 * contents if they were not compressed).
 *
 * @param plugins the list of plugins to use
 * @param data data to process, never NULL
 * @param size number of bytes in data
 * @param tdata end of file data, or NULL
 * @param tsize number of bytes in tdata
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
decompress_and_extract (struct EXTRACTOR_PluginList *plugins,
			const unsigned char * data,
			size_t size,
			const char * tdata,
			size_t tsize,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls) {
  unsigned char * buf;
  unsigned char * rbuf;
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

  buf = NULL;
  dsize = 0;
#if HAVE_ZLIB
  /* try gzip decompression first */
  if ( (size >= 12) &&
       (data[0] == 0x1f) &&
       (data[1] == 0x8b) &&
       (data[2] == 0x08) ) 
    {
      /* Process gzip header */
      unsigned int gzip_header_length = 10;
      
      if (data[3] & 0x4) /* FEXTRA  set */
	gzip_header_length += 2 + (unsigned) (data[10] & 0xff)
	  + (((unsigned) (data[11] & 0xff)) * 256);
      
      if (data[3] & 0x8) /* FNAME set */
	{
	  const unsigned char * cptr = data + gzip_header_length;
	  /* stored file name is here */
	  while (cptr < data + size)
	    {
	      if ('\0' == *cptr)
		break;	      
	      cptr++;
	    }
	  if (0 != proc (proc_cls,
			 "<zlib>",
			 EXTRACTOR_METATYPE_FILENAME,
			 EXTRACTOR_METAFORMAT_C_STRING,
			 "text/plain",
			 (const char*) (data + gzip_header_length),
			 cptr - (data + gzip_header_length)))
	    return; /* done */	  
	  gzip_header_length = (cptr - data) + 1;
	}
      if (data[3] & 0x16) /* FCOMMENT set */
	{
	  const unsigned char * cptr = data + gzip_header_length;
	  /* stored comment is here */	  
	  while (cptr < data + size)
	    {
	      if('\0' == *cptr)
		break;
	      cptr ++;
	    }	
	  if (0 != proc (proc_cls,
			 "<zlib>",
			 EXTRACTOR_METATYPE_COMMENT,
			 EXTRACTOR_METAFORMAT_C_STRING,
			 "text/plain",
			 (const char*) (data + gzip_header_length),
			 cptr - (data + gzip_header_length)))
	    return; /* done */
	  gzip_header_length = (cptr - data) + 1;
	}
      if(data[3] & 0x2) /* FCHRC set */
	gzip_header_length += 2;
      memset(&strm,
	     0,
	     sizeof(z_stream));
#ifdef ZLIB_VERNUM
      gzip_header_length = 0;
#endif
      if (size > gzip_header_length) 
	{
	  strm.next_in = (Bytef*) data + gzip_header_length;
	  strm.avail_in = size - gzip_header_length;
	}
      else
	{
	  strm.next_in = (Bytef*) data;
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
      if (Z_OK == inflateInit2(&strm,
#ifdef ZLIB_VERNUM
			       15 + 32
#else
			       -MAX_WBITS
#endif
			       )) {
	dsize = 2 * size;
	if (dsize > MAX_DECOMPRESS)
	  dsize = MAX_DECOMPRESS;
	buf = malloc(dsize);
	pos = 0;
	if (buf == NULL) 
	  {
	    inflateEnd(&strm);
	  } 
	else 
	  {
	    strm.next_out = (Bytef*) buf;
	    strm.avail_out = dsize;
	    do
	      {
		ret = inflate(&strm,
			      Z_SYNC_FLUSH);
		if (ret == Z_OK) 
		  {
		    if (dsize == MAX_DECOMPRESS)
		      break;
		    pos += strm.total_out;
		    strm.total_out = 0;
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
		    strm.next_out = (Bytef*) &buf[pos];
		    strm.avail_out = dsize - pos;
		  }
		else if (ret != Z_STREAM_END) 
		  {
		    /* error */
		    free(buf);
		    buf = NULL;
		  }
	      } while ( (buf != NULL) &&		
			(ret != Z_STREAM_END) );
	    dsize = pos + strm.total_out;
	    inflateEnd(&strm);
	    if ( (dsize == 0) &&
		 (buf != NULL) )
	      {
		free(buf);
		buf = NULL;
	      }
	  }
      }
    }
#endif
  
#if HAVE_LIBBZ2
  if ( (size >= 4) &&
       (data[0] == 'B') &&
       (data[1] == 'Z') &&
       (data[2] == 'h') ) 
    {
      /* now try bz2 decompression */
      memset(&bstrm,
	     0,
	     sizeof(bz_stream));
      bstrm.next_in = (char*) data;
      bstrm.avail_in = size;
      bstrm.total_in_lo32 = 0;
      bstrm.total_in_hi32 = 0;
      bstrm.bzalloc = NULL;
      bstrm.bzfree = NULL;
      bstrm.opaque = NULL;
      if ( (buf == NULL) &&
	   (BZ_OK == BZ2_bzDecompressInit(&bstrm,
					  0,
					  0)) ) 
	{
	  dsize = 2 * size;
	  if (dsize > MAX_DECOMPRESS)
	    dsize = MAX_DECOMPRESS;
	  buf = malloc(dsize);
	  bpos = 0;
	  if (buf == NULL) 
	    {
	      BZ2_bzDecompressEnd(&bstrm);
	    }
	  else 
	    {
	      bstrm.next_out = (char*) buf;
	      bstrm.avail_out = dsize;
	      do {
		bret = BZ2_bzDecompress(&bstrm);
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
		    free(buf);
		    buf = NULL;
		  }
	      } while ( (buf != NULL) &&
			(bret != BZ_STREAM_END) );
	      dsize = bpos + bstrm.total_out_lo32;
	      BZ2_bzDecompressEnd(&bstrm);
	      if ( (dsize == 0) &&
		   (buf != NULL) )
		{
		  free(buf);
		  buf = NULL;
		}
	    }
	}
    }
#endif  
  if (buf != NULL) 
    {
      data = buf;
      size = dsize;
    }
  extract (plugins,
	   (const char*) data,
	   size,
	   tdata, 
	   tsize,
	   proc,
	   proc_cls);
  if (buf != NULL)
    free(buf);
  errno = 0; /* kill transient errors */
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
  int fd;
  void * buffer;
  void * tbuffer;
  struct stat fstatbuf;
  size_t fsize;
  size_t tsize;
  int eno;
  off_t offset;
  long pg;
#ifdef WINDOWS
  SYSTEM_INFO sys;
#endif

  fd = -1;
  buffer = NULL;
  if ( (data == NULL) &&
       (filename != NULL) &&
       (0 == STAT(filename, &fstatbuf)) &&
       (!S_ISDIR(fstatbuf.st_mode)) &&
       (-1 != (fd = file_open (filename,
			       O_RDONLY | O_LARGEFILE))) )
    {      
      fsize = (fstatbuf.st_size > 0xFFFFFFFF) ? 0xFFFFFFFF : fstatbuf.st_size;
      if (fsize == 0) 
	{
	  close(fd);
	  return;
	}
      if (fsize > MAX_READ)
	fsize = MAX_READ;
      buffer = MMAP(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
      if ( (buffer == NULL) || (buffer == (void *) -1) ) 
	{
	  eno = errno;
	  close(fd);
	  errno = eno;
	  return;
	}
    }
  if ( (buffer == NULL) &&
       (data == NULL) )
    return;
  /* for footer extraction */
  tsize = 0;
  tbuffer = NULL;
  if ( (data == NULL) &&
       (fstatbuf.st_size > fsize) &&
       (fstatbuf.st_size > MAX_READ) )
    {
      pg = SYSCONF (_SC_PAGE_SIZE);
      if ( (pg > 0) &&
	   (pg < MAX_READ) )
	{
	  offset = (1 + (fstatbuf.st_size - MAX_READ) / pg) * pg;
	  if (offset < fstatbuf.st_size)
	    {
	      tsize = fstatbuf.st_size - offset;
	      tbuffer = MMAP (NULL, tsize, PROT_READ, MAP_PRIVATE, fd, offset);
	      if ( (tbuffer == NULL) || (tbuffer == (void *) -1) ) 
		{
		  tsize = 0;
		  tbuffer = NULL;
		}
	    }
	}
    }
  decompress_and_extract (plugins,
			  buffer != NULL ? buffer : data,
			  buffer != NULL ? fsize : size,
			  tbuffer,
			  tsize,
			  proc,
			  proc_cls);
  if (buffer != NULL)
    MUNMAP (buffer, fsize);
  if (tbuffer != NULL)
    MUNMAP (tbuffer, tsize);
  if (-1 != fd)
    close(fd);  
}

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
#ifdef MINGW
  plibc_init("GNU", PACKAGE);
#endif
}


/**
 * Deinit.
 */
void __attribute__ ((destructor)) EXTRACTOR_ltdl_fini() {
#ifdef MINGW
  plibc_shutdown();
#endif
  lt_dlexit ();
}



/* end of extractor.c */
