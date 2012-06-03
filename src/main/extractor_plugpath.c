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

#include "extractor_plugpath.h"

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
  char * ret;
  char * lestr;
  ssize_t size;
  FILE * f;

  snprintf(fn,
	   sizeof (fn),
	   "/proc/%u/maps",
	   getpid());
  f = FOPEN(fn, "r");
  if (f != NULL) {
    while (NULL != fgets(line, 1024, f)) {
      if ( (1 == sscanf(line,
			"%*x-%*x %*c%*c%*c%*c %*x %*2x:%*2x %*u%*[ ]%s",
			dir)) &&
	   (NULL != (lestr = strstr(dir,
				    "libextractor")) ) ) {
	lestr[0] = '\0';
	fclose(f);
	return strdup(dir);
      }
    }
    fclose(f);
  }
  snprintf(fn,
	   sizeof (fn),
	   "/proc/%u/exe",
	   getpid());
  lnk = malloc(1029); /* 1024 + 5 for "lib/" catenation */
  if (lnk == NULL)         
    return NULL;
  size = readlink(fn, lnk, 1023);
  if ( (size <= 0) || (size >= 1024) ) {
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
  ret = realloc(lnk, strlen(lnk) + 5);
  if (ret == NULL)
    {
      free (lnk);
      return NULL;
    }
  strcat(ret, "lib/"); /* guess "lib/" as the library dir */
  return ret;
}
#endif

#if WINDOWS
/**
 * Try to determine path with win32-specific function
 */
static char * 
get_path_from_module_filename() {
  char * path;
  char * ret;
  char * idx;

  path = malloc(4103); /* 4096+nil+6 for "/lib/" catenation */
  if (path == NULL)
    return NULL;
  GetModuleFileName(NULL, path, 4096);
  idx = path + strlen(path);
  while ( (idx > path) &&
	  (*idx != '\\') &&
	  (*idx != '/') )
    idx--;
  *idx = '\0';
  path = cut_bin(path);
  ret = realloc(path, strlen(path) + 6);
  if (ret == NULL)
    {
      free (path);
      return NULL;
    }
  strcat(ret, "/lib/"); /* guess "lib/" as the library dir */
  return ret;
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
	if (p == NULL)
	  return NULL;
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
  char * ret;
  const char * p;

  p = getenv("PATH");
  if (p == NULL)
    return NULL;
  path = strdup(p); /* because we write on it */
  if (path == NULL)
    return NULL;
  buf = malloc(strlen(path) + 20);
  if (buf == NULL)
    {
      free (path);
      return NULL;
    }
  pos = path;

  while (NULL != (end = strchr(pos, ':'))) {
    *end = '\0';
    sprintf(buf, "%s/%s", pos, "extract");
    if (0 == stat(buf, &sbuf)) {
      pos = strdup(pos);
      free(buf);
      free(path);
      if (pos == NULL)
	return NULL;
      pos = cut_bin(pos);
      ret = realloc(pos, strlen(pos) + 5);
      if (ret == NULL)
	{
	  free (pos);
	  return NULL;
	}
      strcat(ret, "lib/");
      return ret;
    }
    pos = end + 1;
  }
  sprintf(buf, "%s/%s", pos, "extract");
  if (0 == stat(buf, &sbuf)) {
    pos = strdup(pos);
    free(buf);
    free(path);
    if (pos == NULL)
      return NULL;
    pos = cut_bin(pos);
    ret = realloc(pos, strlen(pos) + 5);
    if (ret == NULL)
      {
	free (pos);
	return NULL;
      }
    strcat(ret, "lib/");
    return ret;
  }
  free(buf);
  free(path);
  return NULL;
}

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
  size_t slen;

  slen = strlen (path);
  if (slen == 0)
    return NULL;
  if (fname[0] == DIR_SEPARATOR)
    fname++;
  ret = malloc (slen + strlen(fname) + 2);
  if (ret == NULL)
    return NULL;
#ifdef MINGW
  if (path[slen-1] == '\\')
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
  if (path[slen-1] == '/')
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
void
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
      if (d == NULL)
	return;
      prefix = strtok (d, PATH_SEPARATOR_STR);
      while (NULL != prefix)
	{
	  pp (pp_cls, prefix);
	  prefix = strtok (NULL, PATH_SEPARATOR_STR);
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
  pp (pp_cls, PLUGININSTDIR);
  if (prefix == NULL)
    return;
  path = append_to_dir (prefix, PLUGINDIR);
  if (path != NULL)
    {
      if (0 != strcmp (path,
		       PLUGININSTDIR))
	pp (pp_cls, path);
      free (path);
    }
  free (prefix);
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
  dir = OPENDIR (path);
  if (NULL == dir)
    return;
  while (NULL != (ent = READDIR (dir)))
    {
      if (ent->d_name[0] == '.')
	continue;
      if ( (NULL != (la = strstr (ent->d_name, ".la"))) &&
	   (la[3] == '\0') )
	continue; /* only load '.so' and '.dll' */
      sym_name = strrchr (ent->d_name, '_');
      if (sym_name == NULL)
	continue;	
      sym_name++;
      sym = strdup (sym_name);
      if (sym == NULL)
	{
	  CLOSEDIR (dir);
	  return;
	}
      dot = strchr (sym, '.');
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
  CLOSEDIR (dir);
}


/**
 * Given a short name of a library (i.e. "mime"), find
 * the full path of the respective plugin.
 */
char *
find_plugin (const char *short_name)
{
  struct SearchContext sc;
  
  sc.path = NULL;
  sc.short_name = short_name;
  get_installation_paths (&find_plugin_in_path,
			  &sc);
  return sc.path;
}


/**
 * Load all plugins from the given directory.
 * 
 * @param cls pointer to the "struct EXTRACTOR_PluginList*" to extend
 * @param path path to a directory with plugins
 */
void
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
      if ( ( (NULL != (la = strstr (ent->d_name, ".la"))) &&
	     (la[3] == '\0') ) ||
	   ( (NULL != (la = strstr (ent->d_name, ".a"))) &&
	     (la[2] == '\0')) )
	continue; /* only load '.so' and '.dll' */

      sym_name = strrchr (ent->d_name, '_');
      if (sym_name == NULL)
	continue;
      sym_name++;
      sym = strdup (sym_name);
      if (NULL == sym)
	{
	  closedir (dir);
	  return;
	}
      dot = strchr (sym, '.');
      if (dot != NULL)
	*dot = '\0';
#if DEBUG > 1
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

