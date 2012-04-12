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

#include "extractor_plugins.h"
#include "extractor_plugpath.h"


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
 * @param options set to special options requested by the plugin
 * @return NULL on error, otherwise pointer to the symbol
 */
static void *
get_symbol_with_prefix(void *lib_handle,
		       const char *template,
		       const char *prefix,
		       const char **options)
{
  char *name;
  void *symbol;
  const char *sym_name;
  char *sym;
  char *dot;
  const char *(*opt_fun)(void);

  if (NULL != options) *options = NULL;
  sym_name = strstr (prefix, "_");
  if (sym_name == NULL)
    return NULL;
  sym_name++;
  sym = strdup (sym_name);
  if (sym == NULL)
    return NULL;
  dot = strstr (sym, ".");
  if (dot != NULL)
    *dot = '\0';
  name = malloc(strlen(sym) + strlen(template) + 1);
  if (name == NULL)
    {
      free (sym);
      return NULL;
    }
  sprintf(name,
	  template,
	  sym);
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
		  first_error == NULL ? "out of memory" : first_error,
		  lt_dlerror());
	}
      if (first_error != NULL)
	free(first_error);
#endif
    }

  if ( (symbol != NULL) &&
       (NULL != options) )
    {
      /* get special options */
      sprintf(name,
	      "_EXTRACTOR_%s_options",
	      sym);
      /* try without '_' first */
      opt_fun = lt_dlsym(lib_handle, name + 1);
      if (opt_fun == NULL) 
	opt_fun = lt_dlsym(lib_handle, name);
      if (opt_fun != NULL)	
	*options = opt_fun ();
    }
  free (sym);
  free(name);

  return symbol;
}


/**
 * Load a plugin.
 *
 * @param plugin plugin to load
 * @return 0 on success, -1 on error
 */
int
plugin_load (struct EXTRACTOR_PluginList *plugin)
{
#if WINDOWS
  wchar_t wlibname[4097];
  char llibname[4097];
#endif
  lt_dladvise advise;

  if (plugin->libname == NULL)
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
#if WINDOWS
  wlibname[0] = L'\0';
  llibname[0] = '\0';
  if (MultiByteToWideChar (CP_UTF8, 0, plugin->libname, -1, wlibname, 4097) <= 0
      || WideCharToMultiByte (CP_ACP, 0, wlibname, -1, llibname, 4097, NULL, NULL) < 0)
  {
#if DEBUG
      fprintf (stderr,
	       "Loading `%s' plugin failed: %s\n",
	       plugin->short_libname,
	       "can't convert plugin name to local encoding");
      free (plugin->libname);
      plugin->libname = NULL;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
#endif
  }
  plugin->libraryHandle = lt_dlopenadvise (llibname,
				       advise);
#else
  plugin->libraryHandle = lt_dlopenadvise (plugin->libname, 
				       advise);
#endif
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
  plugin->extract_method = get_symbol_with_prefix (plugin->libraryHandle,
						  "_EXTRACTOR_%s_extract_method",
						  plugin->libname,
						  &plugin->specials);
  if (plugin->extract_method == NULL) 
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
  struct EXTRACTOR_PluginList *i;
  char *libname;

  for (i = prev; i != NULL; i = i->next)
  {
    if (strcmp (i->short_libname, library) == 0)
      return prev;
  }

  libname = find_plugin (library);
  if (libname == NULL)
    {
      fprintf (stderr,
	       "Could not load `%s'\n",
	       library);
      return prev;
    }
  result = calloc (1, sizeof (struct EXTRACTOR_PluginList));
  if (result == NULL)
    return prev;
  result->next = prev;
  result->short_libname = strdup (library);
  if (result->short_libname == NULL)
    {
      free (result);
      return NULL;
    }
  result->libname = libname;
  result->flags = flags;
  if (NULL != options)
    result->plugin_options = strdup (options);
  else
    result->plugin_options = NULL;
  /* This is kinda weird, but it allows us to not to call GetSystemInfo()
   * or sysconf() every time we need allocation granularity - just once
   * for each plugin.
   * The only alternative is to keep it in a global variable...
   */
#if WINDOWS
  {
    SYSTEM_INFO si;
    GetSystemInfo (&si);
    result->allocation_granularity = si.dwAllocationGranularity;
  }
#else
  result->allocation_granularity = sysconf (_SC_PAGE_SIZE);
#endif
  return result;
}


/**
 * Load multiple libraries as specified by the user.
 *
 * @param config a string given by the user that defines which
 *        libraries should be loaded. Has the format
 *        "[[-]LIBRARYNAME[(options)][:[-]LIBRARYNAME[(options)]]]*".
 *        For example, 'mp3:ogg.so' loads the
 *        mp3 and the ogg library. The '-' before the LIBRARYNAME
 *        indicates that the library should be removed from
 *        the library list.
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
  if (cpy == NULL)
    return prev;
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
	  prev = EXTRACTOR_plugin_remove (prev, 
					  &cpy[last]);
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
