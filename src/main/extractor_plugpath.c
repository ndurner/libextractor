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
 * @file main/extractor_plugpath.c
 * @brief determine path where plugins are installed
 * @author Christian Grothoff
 */

#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>
#include <ltdl.h>

#include "extractor_plugpath.h"
#include "extractor_logging.h"

/**
 * Function to call on paths.
 * 
 * @param cls closure
 * @param path a directory path
 */
typedef void (*EXTRACTOR_PathProcessor) (void *cls,
					 const char *path);


/**
 * Remove a trailing '/bin/' from 'in' (if present).
 *
 * @param in input string, modified
 * @return NULL if 'in' is NULL, otherwise 'in' with '/bin/' removed
 */
static char * 
cut_bin (char * in) 
{
  size_t p;

  if (NULL == in)
    return NULL;
  p = strlen (in);
  if (p < 4)
    return in;
  if ( ('/' == in[p-1]) ||
       ('\\' == in[p-1]) )
    in[--p] = '\0';
  if (0 == strcmp (&in[p-4],
		   "/bin")) 
    {
      in[p-4] = '\0';
      p -= 4;
    }
  else if (0 == strcmp (&in[p-4],
			"\\bin"))
    {
      in[p-4] = '\0';
      p -= 4;
    }
  return in;
}


#if GNU_LINUX
/**
 * Try to determine path by reading /proc/PID/exe or
 * /proc/PID/maps.
 *
 * Note that this may fail if LE is installed in one directory
 * and the binary linking against it sits elsewhere.
 */
static char *
get_path_from_proc_exe () 
{
  char fn[64];
  char line[1024];
  char dir[1024];
  char *lnk;
  char *ret;
  char *lestr;
  ssize_t size;
  FILE *f;

  snprintf (fn,
	    sizeof (fn),
	    "/proc/%u/maps",
	    getpid ());
  if (NULL != (f = FOPEN (fn, "r")))
    {
      while (NULL != fgets (line, 1024, f)) 
	{
	  if ( (1 == sscanf (line,
			     "%*x-%*x %*c%*c%*c%*c %*x %*2x:%*2x %*u%*[ ]%s",
			     dir)) &&
	       (NULL != (lestr = strstr (dir,
					 "libextractor")) ) ) 
	    {
	      lestr[0] = '\0';
	      fclose (f);
	      return strdup (dir);
	    }
	}
      fclose (f);
    }
  snprintf (fn,
	    sizeof (fn),
	    "/proc/%u/exe",
	    getpid ());
  if (NULL == (lnk = malloc (1029))) /* 1024 + 6 for "/lib/" catenation */
    return NULL;
  size = readlink (fn, lnk, 1023);
  if ( (size <= 0) || (size >= 1024) )
    {
      free (lnk);
      return NULL;
    }
  lnk[size] = '\0';
  while ( ('/' != lnk[size]) &&
	  (size > 0) )
    size--;
  if ( (size < 4) ||
       ('/' != lnk[size-4]) )
    {
      /* not installed in "/bin/" -- binary path probably useless */
      free (lnk);
      return NULL;
    }
  lnk[size] = '\0';
  lnk = cut_bin (lnk);
  if (NULL == (ret = realloc (lnk, strlen(lnk) + 6)))
    {
      LOG_STRERROR ("realloc");
      free (lnk);
      return NULL;
    }
  strcat (ret, "/lib/"); /* guess "lib/" as the library dir */
  return ret;
}
#endif


#if WINDOWS
/**
 * Try to determine path with win32-specific function
 */
static char * 
get_path_from_module_filename () 
{
  char *path;
  char *ret;
  char *idx;

  if (NULL == (path = malloc (4103))) /* 4096+nil+6 for "/lib/" catenation */
    return NULL;
  GetModuleFileName (NULL, path, 4096);
  idx = path + strlen (path);
  while ( (idx > path) &&
	  ('\\' != *idx) &&
	  ('/' != *idx) )
    idx--;
  *idx = '\0';
  path = cut_bin (path);
  if (NULL == (ret = realloc (path, strlen(path) + 6)))
    {
      LOG_STRERROR ("realloc");
      free (path);
      return NULL;
    }
  strcat (ret, "/lib/"); /* guess "lib/" as the library dir */
  return ret;
}
#endif


#if DARWIN
#include <dlfcn.h>
#include <mach-o/dyld.h>

/**
 * Signature of the '_NSGetExecutablePath" function.
 *
 * @param buf where to write the path
 * @param number of bytes available in 'buf'
 * @return 0 on success, otherwise desired number of bytes is stored in 'bufsize'
 */
typedef int (*MyNSGetExecutablePathProto) (char *buf, 
					   size_t *bufsize);


/**
 * Try to obtain the path of our executable using '_NSGetExecutablePath'.
 *
 * @return NULL on error
 */
static char *
get_path_from_NSGetExecutablePath ()
{
  static char zero;
  char *path;
  char *ret;
  size_t len;
  MyNSGetExecutablePathProto func;

  path = NULL;
  if (NULL == (func =
	       (MyNSGetExecutablePathProto) dlsym (RTLD_DEFAULT,
						   "_NSGetExecutablePath")))
    return NULL;
  path = &zero;
  len = 0;
  /* get the path len, including the trailing \0 */
  (void) func (path, &len);
  if (0 == len)
    return NULL;
  if (NULL == (path = malloc (len)))
    {
      LOG_STRERROR ("malloc");
      return NULL;
    }
  if (0 != func (path, &len))
  {
    free (path);
    return NULL;
  }
  len = strlen (path);
  while ((path[len] != '/') && (len > 0))
    len--;
  path[len] = '\0';
  if (NULL != strstr (path, "/lib"))
    return path;
  path = cut_bin (path);
  if (NULL == (ret = realloc (path, strlen (path) + 5)))
  {
     LOG_STRERROR ("realloc");
     free (path);
     return NULL;
  }
  strcat (ret, "/lib/");
  return ret;
}


/**
 * Try to obtain the path of our executable using '_dyld_image' API.
 *
 * @return NULL on error
 */
static char * 
get_path_from_dyld_image () 
{
  const char *path;
  char *s;
  char *p;
  unsigned int i;
  int c;

  c = _dyld_image_count ();
  for (i = 0; i < c; i++) 
    {
      if (((void *) _dyld_get_image_header (i)) != (void *) &_mh_dylib_header)
	continue;
      path = _dyld_get_image_name (i);
      if ( (NULL == path) || (0 == strlen (path)) )
	continue;
      if (NULL == (p = strdup (path)))
	{
	  LOG_STRERROR ("strdup");
	  return NULL;
	}
      s = p + strlen (p);
      while ( (s > p) && ('/' != *s) )
	s--;
      s++;
      *s = '\0';
      return p;
    }
  return NULL;
}
#endif


/**
 * Return the actual path to a file found in the current
 * PATH environment variable.
 *
 * @return path to binary, NULL if not found
 */
static char *
get_path_from_PATH() 
{
  struct stat sbuf;
  char *path;
  char *pos;
  char *end;
  char *buf;
  char *ret;
  const char *p;

  if (NULL == (p = getenv ("PATH")))
    return NULL;
  if (NULL == (path = strdup (p))) /* because we write on it */
    {
      LOG_STRERROR ("strdup");
      return NULL;
    }
  if (NULL == (buf = malloc (strlen (path) + 20)))
    {
      LOG_STRERROR ("malloc");
      free (path);
      return NULL;
    }
  pos = path;
  while (NULL != (end = strchr(pos, ':'))) 
    {
      *end = '\0';
      sprintf (buf, "%s/%s", pos, "extract");
      if (0 == stat(buf, &sbuf)) 
	{
	  free (buf);
	  if (NULL == (pos = strdup (pos)))
	    {
	      LOG_STRERROR ("strdup");
	      free (path);
	      return NULL;
	    }
	  free (path);
	  pos = cut_bin (pos);
	  if (NULL == (ret = realloc (pos, strlen (pos) + 5)))
	    {
	      LOG_STRERROR ("realloc");
	      free (pos);
	      return NULL;
	    }
	  strcat (ret, "lib/");
	  return ret;
	}
      pos = end + 1;
    }
  sprintf (buf, "%s/%s", pos, "extract");
  if (0 == stat (buf, &sbuf)) 
    {
      pos = strdup (pos);
      free (buf);
      free (path);
      if (NULL == pos)
	return NULL;
      pos = cut_bin (pos);
      if (NULL == (ret = realloc (pos, strlen (pos) + 5)))
	{
	  LOG_STRERROR ("realloc");
	  free (pos);
	  return NULL;
	}
      strcat (ret, "lib/");
      return ret;
    }
  free (buf);
  free (path);
  return NULL;
}


/**
 * Create a filename by appending 'fname' to 'path'.
 *
 * @param path the base path 
 * @param fname the filename to append
 * @return '$path/$fname', NULL on error
 */
static char *
append_to_dir (const char *path,
	       const char *fname)
{
  char *ret;
  size_t slen;

  if (0 == (slen = strlen (path)))
    return NULL;
  if (DIR_SEPARATOR == fname[0])
    fname++;
  ret = malloc (slen + strlen(fname) + 2);
  if (NULL == ret)
    return NULL;
#ifdef MINGW
  if ('\\' == path[slen-1])
    sprintf (ret,
	     "%s%s",
	     path, 
	     fname);
  else
    sprintf (ret,
	     "%s\\%s",
	     path, 
	     fname);
#else
  if ('/' == path[slen-1])
    sprintf (ret,
	     "%s%s",
	     path, 
	     fname);
  else
    sprintf (ret,
	     "%s/%s",
	     path, 
	     fname);
#endif
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
get_installation_paths (EXTRACTOR_PathProcessor pp,
			void *pp_cls)
{
  const char *p;
  char *path;
  char *prefix;
  char *d;
  char *saveptr;

  prefix = NULL;
  if (NULL != (p = getenv ("LIBEXTRACTOR_PREFIX")))
    {
      if (NULL == (d = strdup (p)))
	{
	  LOG_STRERROR ("strdup");
	  return;
	}
      for (prefix = strtok_r (d, PATH_SEPARATOR_STR, &saveptr);
	   NULL != prefix;
	   prefix = strtok_r (NULL, PATH_SEPARATOR_STR, &saveptr))
	pp (pp_cls, prefix);	
      free (d);
      return;
    }
#if GNU_LINUX
  if (NULL == prefix)
    prefix = get_path_from_proc_exe ();
#endif
#if WINDOWS
  if (NULL == prefix)
    prefix = get_path_from_module_filename ();
#endif
#if DARWIN
  if (NULL == prefix)
    prefix = get_path_from_NSGetExecutablePath ();
  if (NULL == prefix)
    prefix = get_path_from_dyld_image ();
#endif
  if (NULL == prefix)
    prefix = get_path_from_PATH ();
  pp (pp_cls, PLUGININSTDIR);
  if (NULL == prefix)
    return;
  path = append_to_dir (prefix, PLUGINDIR);
  if (NULL != path)
    {
      if (0 != strcmp (path,
		       PLUGININSTDIR))
	pp (pp_cls, path);
      free (path);
    }
  free (prefix);
}


/**
 * Closure for 'find_plugin_in_path'.
 */
struct SearchContext
{
  /**
   * Name of the plugin we are looking for.
   */
  const char *short_name;
  
  /**
   * Location for storing the path to the plugin upon success.
   */ 
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
  const char *sym_name;
  char *sym;
  char *dot;
  size_t dlen;

  if (NULL != sc->path)
    return;
  if (NULL == (dir = OPENDIR (path)))
    return;
  while (NULL != (ent = READDIR (dir)))
    {
      if ('.' == ent->d_name[0])
	continue;
      dlen = strlen (ent->d_name);
      if ( (dlen < 4) ||
	   ( (0 != strcmp (&ent->d_name[dlen-3], ".so")) &&
	     (0 != strcasecmp (&ent->d_name[dlen-4], ".dll")) ) )
	continue; /* only load '.so' and '.dll' */
      if (NULL == (sym_name = strrchr (ent->d_name, '_')))
	continue;	
      sym_name++;
      if (NULL == (sym = strdup (sym_name)))
	{
	  LOG_STRERROR ("strdup");
	  CLOSEDIR (dir);
	  return;
	}
      dot = strchr (sym, '.');
      if (NULL != dot)
	*dot = '\0';
      if (0 == strcmp (sym, sc->short_name))
	{
	  sc->path = append_to_dir (path, ent->d_name);
	  free (sym);
	  break;
	}
      free (sym);
    }
  CLOSEDIR (dir);
}


/**
 * Given a short name of a library (i.e. "mime"), find
 * the full path of the respective plugin.
 */
char *
EXTRACTOR_find_plugin_ (const char *short_name)
{
  struct SearchContext sc;
  
  sc.path = NULL;
  sc.short_name = short_name;
  get_installation_paths (&find_plugin_in_path,
			  &sc);
  return sc.path;
}


/** 
 * Closure for 'load_plugins_from_dir'.
 */
struct DefaultLoaderContext
{
  /**
   * Accumulated result list.
   */ 
  struct EXTRACTOR_PluginList *res;

  /**
   * Flags to use for all plugins.
   */
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
  const char *sym_name;
  char *sym;
  char *dot;
  size_t dlen;

  if (NULL == (dir = opendir (path)))
    return;
  while (NULL != (ent = readdir (dir)))
    {
      if (ent->d_name[0] == '.')
	continue;
      dlen = strlen (ent->d_name);
      if ( (dlen < 4) ||
	   ( (0 != strcmp (&ent->d_name[dlen-3], ".so")) &&
	     (0 != strcasecmp (&ent->d_name[dlen-4], ".dll")) ) )
	continue; /* only load '.so' and '.dll' */
      if (NULL == (sym_name = strrchr (ent->d_name, '_')))
	continue;
      sym_name++;
      if (NULL == (sym = strdup (sym_name)))
	{
	  LOG_STRERROR ("strdup");
	  closedir (dir);
	  return;
	}
      if (NULL != (dot = strchr (sym, '.')))
	*dot = '\0';
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
EXTRACTOR_plugin_add_defaults (enum EXTRACTOR_Options flags)
{
  struct DefaultLoaderContext dlc;
  char *env;

  env = getenv ("LIBEXTRACTOR_LIBRARIES");
  if (NULL != env)
    return EXTRACTOR_plugin_add_config (NULL, env, flags);
  dlc.res = NULL;
  dlc.flags = flags;
  get_installation_paths (&load_plugins_from_dir,
			  &dlc);
  return dlc.res;
}


/* end of extractor_plugpath.c */
