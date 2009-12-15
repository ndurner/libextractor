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
#include "extractor.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>


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

#define DEBUG 0


#if HAVE_LTDL_H
#include <ltdl.h>
#else
#include <../../libltdl/ltdl.h>
#endif

#if HAVE_LIBBZ2
#include <bzlib.h>
#endif

#if HAVE_ZLIB
#include <zlib.h>
#endif


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
   * Name of the library (i.e., 'libextractor_foo.so')
   */
  char *short_libname;
  
  /**
   * Pointer to the function used for meta data extraction.
   */
  EXTRACTOR_ExtractMethod extractMethod;

  /**
   * Options for the plugin.
   */
  char * plugin_options;

  /**
   * Flags to control how the plugin is executed.
   */
  enum EXTRACTOR_Options flags;

  /**
   * Process ID of the child process for this plugin. 0 for 
   * none.
   */
  pid_t cpid;

  /**
   * Pipe used to send information about shared memory segments to
   * the child process.  NULL if not initialized.
   */
  FILE *cpipe_in;

  /**
   * Pipe used to read information about extracted meta data from
   * the child process.  -1 if not initialized.
   */
  int cpipe_out;

};


/**
 * Remove a trailing '/bin' from in (if present).
 */
static char * 
cut_bin(char * in) {
  size_t p;

  if (in == NULL)
    return NULL;
  p = strlen(in);
  if (p > 4) {
    if ( (in[p-1] == '/') ||
	 (in[p-1] == '\\') )
      in[--p] = '\0';
    if (0 == strcmp(&in[p-3],
		    "bin")) {
      in[p-3] = '\0';
      p -= 3;
    }
  }
  return in;
}

#if LINUX
/**
 * Try to determine path by reading /proc/PID/exe or
 * /proc/PID/maps.
 *
 * Note that this may fail if LE is installed in one directory
 * and the binary linking against it sits elsewhere.
 */
static char *
get_path_from_proc_exe() {
  char fn[64];
  char line[1024];
  char dir[1024];
  char * lnk;
  size_t size;
  FILE * f;

  snprintf(fn,
	   64,
	   "/proc/%u/maps",
	   getpid());
  f = fopen(fn, "r");
  if (f != NULL) {
    while (NULL != fgets(line, 1024, f)) {
      if ( (1 == sscanf(line,
			"%*x-%*x %*c%*c%*c%*c %*x %*2u:%*2u %*u%*[ ]%s",
			dir)) &&
	   (NULL != strstr(dir,
			   "libextractor")) ) {
	strstr(dir, "libextractor")[0] = '\0';
	fclose(f);
	return strdup(dir);
      }
    }
    fclose(f);
  }
  snprintf(fn,
	   64,
	   "/proc/%u/exe",
	   getpid());
  lnk = malloc(1029); /* 1024 + 5 for "lib/" catenation */
  size = readlink(fn, lnk, 1023);
  if ( (size == 0) || (size >= 1024) ) {
    free(lnk);
    return NULL;
  }
  lnk[size] = '\0';
  while ( (lnk[size] != '/') &&
	  (size > 0) )
    size--;
  if ( (size < 4) ||
       (lnk[size-4] != '/') ) {
    /* not installed in "/bin/" -- binary path probably useless */
    free(lnk);
    return NULL;
  }
  lnk[size] = '\0';
  lnk = cut_bin(lnk);
  lnk = realloc(lnk, strlen(lnk) + 5);
  strcat(lnk, "lib/"); /* guess "lib/" as the library dir */
  return lnk;
}
#endif

#if WINDOWS
/**
 * Try to determine path with win32-specific function
 */
static char * get_path_from_module_filename() {
  char * path;
  char * idx;

  path = malloc(4103); /* 4096+nil+6 for "/lib/" catenation */
  GetModuleFileName(NULL, path, 4096);
  idx = path + strlen(path);
  while ( (idx > path) &&
	  (*idx != '\\') &&
	  (*idx != '/') )
    idx--;
  *idx = '\0';
  path = cut_bin(path);
  path = realloc(path, strlen(path) + 6);
  strcat(path, "/lib/"); /* guess "lib/" as the library dir */
  return path;
}
#endif

#if DARWIN
static char * get_path_from_dyld_image() {
  const char * path;
  char * p, * s;
  int i;
  int c;

  p = NULL;
  c = _dyld_image_count();
  for (i = 0; i < c; i++) {
    if (_dyld_get_image_header(i) == &_mh_dylib_header) {
      path = _dyld_get_image_name(i);
      if (path != NULL && strlen(path) > 0) {
        p = strdup(path);
        s = p + strlen(p);
        while ( (s > p) && (*s != '/') )
          s--;
        s++;
        *s = '\0';
      }
      break;
    }
  }
  return p;
}
#endif

/**
 * This may also fail -- for example, if extract
 * is not also installed.
 */
static char *
get_path_from_PATH() {
  struct stat sbuf;
  char * path;
  char * pos;
  char * end;
  char * buf;
  const char * p;
  size_t size;

  p = getenv("PATH");
  if (p == NULL)
    return NULL;
  path = strdup(p); /* because we write on it */
  buf = malloc(strlen(path) + 20);
  size = strlen(path);
  pos = path;

  while (NULL != (end = strchr(pos, ':'))) {
    *end = '\0';
    sprintf(buf, "%s/%s", pos, "extract");
    if (0 == stat(buf, &sbuf)) {
      pos = strdup(pos);
      free(buf);
      free(path);
      pos = cut_bin(pos);
      pos = realloc(pos, strlen(pos) + 5);
      strcat(pos, "lib/");
      return pos;
    }
    pos = end + 1;
  }
  sprintf(buf, "%s/%s", pos, "extract");
  if (0 == stat(buf, &sbuf)) {
    pos = strdup(pos);
    free(buf);
    free(path);
    pos = cut_bin(pos);
    pos = realloc(pos, strlen(pos) + 5);
    strcat(pos, "lib/");
    return pos;
  }
  free(buf);
  free(path);
  return NULL;
}


/**
 * Function to call on paths.
 * 
 * @param cls closure
 * @param path a directory path
 */
typedef void (*PathProcessor)(void *cls,
			      const char *path);


/**
 * Create a filename by appending 'fname' to 'path'.
 *
 * @param path the base path 
 * @param fname the filename to append
 * @return '$path/$fname'
 */
static char *
append_to_dir (const char *path,
	       const char *fname)
{
  char *ret;

  ret = malloc (strlen (path) + strlen(fname) + 2);
  sprintf (ret,
#ifdef MINGW
	   "%s\%s",
#else
	   "%s/%s",
#endif
	   path, 
	   fname);
  return ret;
}


/**
 * Iterate over all paths where we expect to find GNU libextractor
 * plugins.
 *
 * @param pp function to call for each path
 * @param pp_cls cls argument for pp.
 */
static void
get_installation_paths (PathProcessor pp,
			void *pp_cls)
{
  const char *p;
  char * path;
  char * prefix;
  char * d;

  prefix = NULL;
  p = getenv("LIBEXTRACTOR_PREFIX");
  if (p != NULL)
    {
      d = strdup (p);
      prefix = strtok (d, ":");
      while (NULL != prefix)
	{
	  pp (pp_cls, prefix);
	  prefix = strtok (NULL, ":");
	}
      free (d);
      return;
    }
#if LINUX
  if (prefix == NULL)
    prefix = get_path_from_proc_exe();
#endif
#if WINDOWS
  if (prefix == NULL)
    prefix = get_path_from_module_filename();
#endif
#if DARWIN
  if (prefix == NULL)
    prefix = get_path_from_dyld_image();
#endif
  if (prefix == NULL)
    prefix = get_path_from_PATH();
  if (prefix == NULL)
    return;
  if (prefix != NULL)
    {
      path = append_to_dir (prefix, PLUGINDIR);
      pp (pp_cls, path);
      free (path);
      free (prefix);
      return;
    }
}


struct SearchContext
{
  const char *short_name;
  char *path;
};


/**
 * Load all plugins from the given directory.
 * 
 * @param cls pointer to the "struct EXTRACTOR_PluginList*" to extend
 * @param path path to a directory with plugins
 */
static void
find_plugin_in_path (void *cls,
		     const char *path)
{
  struct SearchContext *sc = cls;
  DIR *dir;
  struct dirent *ent;
  const char *la;
  const char *sym_name;
  char *sym;
  char *dot;

  if (sc->path != NULL)
    return;
  dir = opendir (path);
  if (NULL == dir)
    return;
  while (NULL != (ent = readdir (dir)))
    {
      if (ent->d_name[0] == '.')
	continue;
      if ( (NULL != (la = strstr (ent->d_name, ".la"))) &&
	   (la[3] == '\0') )
	continue; /* only load '.so' and '.dll' */
      sym_name = strstr (ent->d_name, "_");
      if (sym_name == NULL)
	continue;
      sym_name++;
      sym = strdup (sym_name);
      dot = strstr (sym, ".");
      if (dot != NULL)
	*dot = '\0';
      if (0 == strcmp (sym, sc->short_name))
	{
	  sc->path = append_to_dir (path, ent->d_name);
	  free (sym);
	  break;
	}
      free (sym);
    }
#if DEBUG
  if (sc->path == NULL)
    fprintf (stderr,
	     "Failed to find plugin `%s' in `%s'\n",
	     sc->short_name,
	     path);
#endif
  closedir (dir);
}



/**
 * Given a short name of a library (i.e. "mime"), find
 * the full path of the respective plugin.
 */
static char *
find_plugin (const char *short_name)
{
  struct SearchContext sc;
  
  sc.path = NULL;
  sc.short_name = short_name;
  get_installation_paths (&find_plugin_in_path,
			  &sc);
  return sc.path;
}



struct DefaultLoaderContext
{
  struct EXTRACTOR_PluginList *res;
  enum EXTRACTOR_Options flags;
};


/**
 * Load all plugins from the given directory.
 * 
 * @param cls pointer to the "struct EXTRACTOR_PluginList*" to extend
 * @param path path to a directory with plugins
 */
static void
load_plugins_from_dir (void *cls,
		       const char *path)
{
  struct DefaultLoaderContext *dlc = cls;
  DIR *dir;
  struct dirent *ent;
  const char *la;
  const char *sym_name;
  char *sym;
  char *dot;

  dir = opendir (path);
  if (NULL == dir)
    return;
  while (NULL != (ent = readdir (dir)))
    {
      if (ent->d_name[0] == '.')
	continue;
      if ( (NULL != (la = strstr (ent->d_name, ".la"))) &&
	   (la[3] == '\0') )
	continue; /* only load '.so' and '.dll' */

      sym_name = strstr (ent->d_name, "_");
      if (sym_name == NULL)
	continue;
      sym_name++;
      sym = strdup (sym_name);
      dot = strstr (sym, ".");
      if (dot != NULL)
	*dot = '\0';
#if DEBUG
      fprintf (stderr,
	       "Adding default plugin `%s'\n",
	       sym);
#endif
      dlc->res = EXTRACTOR_plugin_add (dlc->res,
				       sym,
				       NULL,
				       dlc->flags);
      free (sym);
    }
  closedir (dir);
}


/**
 * Load the default set of plugins. The default can be changed
 * by setting the LIBEXTRACTOR_LIBRARIES environment variable.
 * If it is set to "env", then this function will return
 * EXTRACTOR_plugin_add_config (NULL, env, flags).  Otherwise,
 * it will load all of the installed plugins and return them.
 *
 * @param flags options for all of the plugins loaded
 * @return the default set of plugins, NULL if no plugins were found
 */
struct EXTRACTOR_PluginList * 
EXTRACTOR_plugin_add_defaults(enum EXTRACTOR_Options flags)
{
  struct DefaultLoaderContext dlc;
  char *env;

  env = getenv ("LIBEXTRACTOR_LIBRARIES");
  if (env != NULL)
    return EXTRACTOR_plugin_add_config (NULL, env, flags);
  dlc.res = NULL;
  dlc.flags = flags;
  get_installation_paths (&load_plugins_from_dir,
			  &dlc);
  return dlc.res;
}


/**
 * Try to resolve a plugin function.
 *
 * @param lib_handle library to search for the symbol
 * @param prefix prefix to add
 * @param sym_name base name for the symbol
 * @return NULL on error, otherwise pointer to the symbol
 */
static void *
get_symbol_with_prefix(void *lib_handle,
		       const char *prefix)
{
  char *name;
  void *symbol;
  const char *sym_name;
  char *sym;
  char *dot;

  sym_name = strstr (prefix, "_");
  if (sym_name == NULL)
    return NULL;
  sym_name++;
  sym = strdup (sym_name);
  dot = strstr (sym, ".");
  if (dot != NULL)
    *dot = '\0';
  name = malloc(strlen(sym) + 32);
  sprintf(name,
	  "_EXTRACTOR_%s_extract",
	  sym);
  free (sym);
  /* try without '_' first */
  symbol = lt_dlsym(lib_handle, name + 1);
  if (symbol==NULL) 
    {
      /* now try with the '_' */
#if DEBUG
      char *first_error = strdup (lt_dlerror());
#endif
      symbol = lt_dlsym(lib_handle, name);
#if DEBUG
      if (NULL == symbol)
	{
	  fprintf(stderr,
		  "Resolving symbol `%s' failed, "
		  "so I tried `%s', but that failed also.  Errors are: "
		  "`%s' and `%s'.\n",
		  name+1,
		  name,
		  first_error,
		  lt_dlerror());
	}
      free(first_error);
#endif
    }
  free(name);
  return symbol;
}


/**
 * Load a plugin.
 *
 * @param plugin plugin to load
 * @return 0 on success, -1 on error
 */
static int
plugin_load (struct EXTRACTOR_PluginList *plugin)
{
  lt_dladvise advise;

  plugin->libname = find_plugin (plugin->short_libname);
  if (plugin->libname == NULL)
    {
#if DEBUG
      fprintf (stderr,
	       "Failed to find plugin `%s'\n",
	       plugin->short_libname);
#endif
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
    }
  lt_dladvise_init (&advise);
  lt_dladvise_ext (&advise);
  lt_dladvise_local (&advise);
  plugin->libraryHandle = lt_dlopenadvise (plugin->libname, 
				       advise);
  lt_dladvise_destroy(&advise);
  if (plugin->libraryHandle == NULL)
    {
#if DEBUG
      fprintf (stderr,
	       "Loading `%s' plugin failed: %s\n",
	       plugin->short_libname,
	       lt_dlerror ());
#endif
      free (plugin->libname);
      plugin->libname = NULL;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
    }
  plugin->extractMethod = get_symbol_with_prefix (plugin->libraryHandle,
						  plugin->libname);
  if (plugin->extractMethod == NULL) 
    {
#if DEBUG
      fprintf (stderr,
	       "Resolving `extract' method of plugin `%s' failed: %s\n",
	       plugin->short_libname,
	       lt_dlerror ());
#endif
      lt_dlclose (plugin->libraryHandle);
      free (plugin->libname);
      plugin->libname = NULL;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
    }
  return 0;
}




/**
 * Add a library for keyword extraction.
 *
 * @param prev the previous list of libraries, may be NULL
 * @param library the name of the library
 * @param flags options to use
 * @return the new list of libraries, equal to prev iff an error occured
 */
struct EXTRACTOR_PluginList *
EXTRACTOR_plugin_add (struct EXTRACTOR_PluginList * prev,
		      const char *library,
		      const char *options,
		      enum EXTRACTOR_Options flags)
{
  struct EXTRACTOR_PluginList *result;
  char *libname;

  libname = find_plugin (library);
  if (libname == NULL)
    return prev;
  result = calloc (1, sizeof (struct EXTRACTOR_PluginList));
  result->next = prev;
  result->short_libname = strdup (library);
  result->libname = libname;
  result->flags = flags;
  if (NULL != options)
    result->plugin_options = strdup (options);
  else
    result->plugin_options = NULL;
  return result;
}


/**
 * Add a library for keyword extraction at the END of the list.
 * @param prev the previous list of libraries, may be NULL
 * @param library the name of the library
 * @param options options to give to the library
 * @param flags options to use
 * @return the new list of libraries, always equal to prev
 *         except if prev was NULL and no error occurs
 */
struct EXTRACTOR_PluginList *
EXTRACTOR_plugin_add_last(struct EXTRACTOR_PluginList *prev,
			  const char *library,
			  const char *options,
			  enum EXTRACTOR_Options flags)
{
  struct EXTRACTOR_PluginList *result;
  struct EXTRACTOR_PluginList *pos;
  char *libname;

  libname = find_plugin (library);
  if (libname == NULL)
    return prev;
  result = calloc (1, sizeof (struct EXTRACTOR_PluginList));
  result->next = prev;
  result->short_libname = strdup (library);
  result->libname = libname;
  result->flags = flags;
  if( options )
    result->plugin_options = strdup (options);
  else
    result->plugin_options = NULL;
  if (prev == NULL)
    return result;
  pos = prev;
  while (pos->next != NULL)
    pos = pos->next;
  pos->next = result;
  return prev;
}


/**
 * Load multiple libraries as specified by the user.
 *
 * @param config a string given by the user that defines which
 *        libraries should be loaded. Has the format
 *        "[[-]LIBRARYNAME[(options)][:[-]LIBRARYNAME[(options)]]]*".
 *        For example, 'mp3:ogg.so' loads the
 *        mp3 and the ogg library. The '-' before the LIBRARYNAME
 *        indicates that the library should be added to the end
 *        of the library list (addLibraryLast).
 * @param prev the  previous list of libraries, may be NULL
 * @param flags options to use
 * @return the new list of libraries, equal to prev iff an error occured
 *         or if config was empty (or NULL).
 */
struct EXTRACTOR_PluginList *
EXTRACTOR_plugin_add_config (struct EXTRACTOR_PluginList * prev,
			     const char *config,
			     enum EXTRACTOR_Options flags)
{
  char *cpy;
  size_t pos;
  size_t last;
  ssize_t lastconf;
  size_t len;

  if (config == NULL)
    return prev;
  len = strlen(config);
  cpy = strdup(config);
  pos = 0;
  last = 0;
  lastconf = 0;
  while (pos < len)
    {
      while ((cpy[pos] != ':') && (cpy[pos] != '\0') &&
	     (cpy[pos] != '('))
	pos++;
      if( cpy[pos] == '(' ) {
 	cpy[pos++] = '\0';	/* replace '(' by termination */
	lastconf = pos;         /* start config from here, after (. */
	while ((cpy[pos] != '\0') && (cpy[pos] != ')'))
	  pos++; /* config until ) or EOS. */
	if( cpy[pos] == ')' ) {
	  cpy[pos++] = '\0'; /* write end of config here. */
	  while ((cpy[pos] != ':') && (cpy[pos] != '\0'))
	    pos++; /* forward until real end of string found. */
	  cpy[pos++] = '\0';
	} else {
	  cpy[pos++] = '\0'; /* end of string. */
	}
      } else {
	lastconf = -1;         /* NULL config when no (). */
	cpy[pos++] = '\0';	/* replace ':' by termination */
      }
      if (cpy[last] == '-')
	{
	  last++;
	  prev = EXTRACTOR_plugin_add_last (prev, 
					    &cpy[last],
					    (lastconf != -1) ? &cpy[lastconf] : NULL,
					    flags);
	}
      else
	{
	  prev = EXTRACTOR_plugin_add (prev, 
				       &cpy[last], 
				       (lastconf != -1) ? &cpy[lastconf] : NULL,
				       flags);
	}
      last = pos;
    }
  free (cpy);
  return prev;
}


/**
 * Stop the child process of this plugin.
 */
static void
stop_process (struct EXTRACTOR_PluginList *plugin)
{
  int status;

  if ( (plugin->cpid == -1) ||
       (plugin->cpid == 0) )
    return;
  kill (plugin->cpid, SIGKILL);
  waitpid (plugin->cpid, &status, 0);
  plugin->cpid = -1;
  close (plugin->cpipe_out);
  plugin->cpipe_out = -1;
  fclose (plugin->cpipe_in);
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
 * 'main' function of the child process.
 * Reads shm-filenames from 'in' (line-by-line) and
 * writes meta data blocks to 'out'.  The meta data
 * stream is terminated by an empty entry.
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
  char fn[256];
  FILE *fin;
  void *ptr;
  int shmid;
  struct stat sbuf;
  struct IpcHeader hdr;

  if (0 != plugin_load (plugin))
    {
      close (in);
      close (out);
      return;
    }  
  memset (&hdr, 0, sizeof (hdr));
  fin = fdopen (in, "r");
  while (NULL != fgets (fn, sizeof(fn), fin))
    {
      if ( (-1 != (shmid = shm_open (fn, O_RDONLY, 0))) &&
	   (0 == fstat (shmid, &sbuf)) &&
	   (NULL != (ptr = shmat (shmid, NULL, SHM_RDONLY))) )
	{
	  if (0 != plugin->extractMethod (ptr,
					  sbuf.st_size,
					  &transmit_reply,
					  &out,
					  plugin->plugin_options))
	    break;
	  if (0 != write_all (out, &hdr, sizeof(hdr)))
	    break;
	}
      if (ptr != NULL)
	shmdt (ptr);
      if (-1 != shmid)
	close (shmid);
    }
  fclose (fin);
  close (out);
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
  
  if (0 != pipe (p1))
    {
      plugin->cpid = -1;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return;
    }
  if (0 != pipe (p2))
    {
      close (p1[0]);
      close (p1[1]);
      plugin->cpid = -1;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return;
    }
  pid = fork ();
  if (pid == -1)
    {
      close (p1[0]);
      close (p1[1]);
      close (p2[0]);
      close (p2[1]);
      plugin->cpid = -1;
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
  plugin->cpid = 0;
  close (p1[0]);
  close (p2[1]);
  plugin->cpipe_in = fdopen (p1[1], "w");
  plugin->cpipe_out = p2[0];
}


/**
 * Extract meta data using the given plugin, running the
 * actual code of the plugin out-of-process.
 *
 * @param plugin which plugin to call
 * @param shmfn file name of the shared memory segment
 * @param proc function to call on the meta data
 * @param proc_cls cls for proc
 * @return 0 if proc did not return non-zero
 */
static int
extract_oop (struct EXTRACTOR_PluginList *plugin,
	     const char *shmfn,
	     EXTRACTOR_MetaDataProcessor proc,
	     void *proc_cls)
{
  struct IpcHeader hdr;
  char mimetype[MAX_MIME_LEN + 1];
  char *data;

  if (0 <= fprintf (plugin->cpipe_in, "%s\n", shmfn))
    {
      stop_process (plugin);
      plugin->cpid = -1;
      if (plugin->flags != EXTRACTOR_OPTION_AUTO_RESTART)
	plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return 0;
    }
  while (1)
    {
      if (0 != read_all (plugin->cpipe_out,
			 &hdr,
			 sizeof(hdr)))
	{
	  if (plugin->flags != EXTRACTOR_OPTION_AUTO_RESTART)
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
	  if (plugin->flags != EXTRACTOR_OPTION_AUTO_RESTART)
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
	  free (data);
	  if (plugin->flags != EXTRACTOR_OPTION_AUTO_RESTART)
	    plugin->flags = EXTRACTOR_OPTION_DISABLED;
	  return 0;
	}	   
      mimetype[hdr.mime_len] = '\0';
      if ( (proc != NULL) &&
	   (0 != proc (proc_cls, 
		       plugin->libname,
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
 * Extract keywords from a file using the given set of plugins.
 *
 * @param plugins the list of plugins to use
 * @param filename the name of the file, can be NULL 
 * @param data data to process, never NULL
 * @param size number of bytes in data, ignored if data is NULL
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
extract (struct EXTRACTOR_PluginList *plugins,
	 const char * filename,
	 const char * data,
	 size_t size,
	 EXTRACTOR_MetaDataProcessor proc,
	 void *proc_cls) 
{
  struct EXTRACTOR_PluginList *ppos;
  int shmid;
  enum EXTRACTOR_Options flags;
  void *ptr;
  char fn[255];
  int want_shm;

  want_shm = 0;
  ppos = plugins;
  while (NULL != ppos)
    {      
      switch (ppos->flags)
	{
	case EXTRACTOR_OPTION_NONE:
	  break;
	case EXTRACTOR_OPTION_OUT_OF_PROCESS:
	  if (0 == plugins->cpid)
	    start_process (plugins);
	  want_shm = 1;
	  break;
	case EXTRACTOR_OPTION_AUTO_RESTART:
	  if ( (0 == plugins->cpid) ||
	       (-1 == plugins->cpid) )
	    start_process (plugins);
	  want_shm = 1;
	  break;
	case EXTRACTOR_OPTION_DISABLED:
	  break;
	}      
      ppos = ppos->next;
    }

  if (want_shm)
    {
      sprintf (fn,
	       "/tmp/libextractor-shm-%u-XXXXXX",
	       getpid());	   
      mktemp (fn);
      shmid = shm_open (fn, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      ptr = NULL;
      if (shmid != -1)
	{
	  if ( (0 != ftruncate (shmid, size)) ||
	       (NULL == (ptr = shmat (shmid, NULL, 0))) )
	    {
	      close (shmid);	
	      shmid = -1;
	    }
	  memcpy (ptr, data, size);
	}
    }
  ppos = plugins;
  while (NULL != ppos)
    {
      flags = ppos->flags;
      if (shmid == -1)
	flags = EXTRACTOR_OPTION_NONE;
      switch (flags)
	{
	case EXTRACTOR_OPTION_NONE:	  
	  if (NULL == ppos->extractMethod)  
	    plugin_load (ppos);	    
	  if ( (NULL != ppos->extractMethod) &&
	       (0 != ppos->extractMethod (data, 
					  size, 
					  proc, 
					  proc_cls,
					  ppos->plugin_options)) )
	    return;
	  break;
	case EXTRACTOR_OPTION_OUT_OF_PROCESS:
	case EXTRACTOR_OPTION_AUTO_RESTART:
	  if (0 != extract_oop (ppos, fn, proc, proc_cls))
	    return;
	  break;
	case EXTRACTOR_OPTION_DISABLED:
	  break;
	}      
      ppos = ppos->next;
    }
  if (want_shm)
    {
      if (NULL != ptr)
	shmdt (ptr);
      if (shmid != -1)
	close (shmid);
      shm_unlink (fn);
      unlink (fn);
    }
}


/**
 * If the given data is compressed using gzip or bzip2, decompress
 * it.  Run 'extract' on the decompressed contents (or the original
 * contents if they were not compressed).
 *
 * @param plugins the list of plugins to use
 * @param filename the name of the file, can be NULL 
 * @param data data to process, never NULL
 * @param size number of bytes in data, ignored if data is NULL
 * @param proc function to call for each meta data item found
 * @param proc_cls cls argument to proc
 */
static void
decompress_and_extract (struct EXTRACTOR_PluginList *plugins,
			const char * filename,
			const unsigned char * data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls) {
  unsigned char * buf;
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
		    buf = realloc(buf, dsize);
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
	    if (dsize == 0) {
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
		    buf = realloc(buf, dsize);
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
	      if (dsize == 0) 
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
	   filename,
	   (const char*) data,
	   size,
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
  return open(fn, oflag, mode);
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
  struct stat fstatbuf;
  size_t fsize;
  int eno;

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
  decompress_and_extract (plugins,
			  filename,
			  buffer != NULL ? buffer : data,
			  buffer != NULL ? fsize : size,
			  proc,
			  proc_cls);
  if (buffer != NULL)
    MUNMAP (buffer, size);
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
