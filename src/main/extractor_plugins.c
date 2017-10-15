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
/**
 * @file main/extractor_plugins.c
 * @brief code to load plugins
 * @author Christian Grothoff
 */
#include "extractor_plugins.h"
#include "extractor_plugpath.h"
#include "extractor_ipc.h"
#include "extractor_logging.h"


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
get_symbol_with_prefix (void *lib_handle,
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

  if (NULL != options)
    *options = NULL;
  if (NULL == (sym_name = strrchr (prefix, '_')))
    return NULL;
  sym_name++;
  if (NULL == (sym = strdup (sym_name)))
    {
      LOG_STRERROR ("strdup");
      return NULL;
    }
  if (NULL != (dot = strchr (sym, '.')))
    *dot = '\0';
  if (NULL == (name = malloc(strlen(sym) + strlen(template) + 1)))
    {
      free (sym);
      return NULL;
    }
  sprintf(name,
	  template,
	  sym);
  /* try without '_' first */
  symbol = lt_dlsym (lib_handle, name + 1);
  if (NULL == symbol)
    {
      /* now try with the '_' */
      char *first_error = strdup (lt_dlerror ());
      symbol = lt_dlsym (lib_handle, name);
      if (NULL == symbol)
	{
	  LOG ("Resolving symbol `%s' failed, "
	       "so I tried `%s', but that failed also.  Errors are: "
	       "`%s' and `%s'.\n",
	       name+1,
	       name,
	       first_error == NULL ? "out of memory" : first_error,
	       lt_dlerror ());
	}
      if (NULL != first_error)
	free (first_error);
    }

  if ( (NULL != symbol) &&
       (NULL != options) )
    {
      /* get special options */
      sprintf (name,
	       "_EXTRACTOR_%s_options",
	       sym);
      /* try without '_' first */
      opt_fun = lt_dlsym (lib_handle, name + 1);
      if (NULL == opt_fun)
	opt_fun = lt_dlsym (lib_handle, name);
      if (NULL != opt_fun)
	*options = opt_fun ();
    }
  free (sym);
  free (name);
  return symbol;
}


/**
 * Load a plugin.
 *
 * @param plugin plugin to load
 * @return 0 on success, -1 on error
 */
int
EXTRACTOR_plugin_load_ (struct EXTRACTOR_PluginList *plugin)
{
#if WINDOWS
  wchar_t wlibname[4097];
  char llibname[4097];
#endif
  lt_dladvise advise;

  if (EXTRACTOR_OPTION_DISABLED == plugin->flags)
    return -1;
  if (NULL == plugin->libname)
    plugin->libname = EXTRACTOR_find_plugin_ (plugin->short_libname);
  if (NULL == plugin->libname)
    {
      LOG ("Failed to find plugin `%s'\n",
	   plugin->short_libname);
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
    }
  lt_dladvise_init (&advise);
  lt_dladvise_ext (&advise);
  lt_dladvise_local (&advise);
#if WINDOWS
  wlibname[0] = L'\0';
  llibname[0] = '\0';
  if ( (MultiByteToWideChar (CP_UTF8, 0, plugin->libname, -1,
			     wlibname, sizeof (wlibname)) <= 0) ||
       (WideCharToMultiByte (CP_ACP, 0, wlibname, -1,
			     llibname, sizeof (llibname), NULL, NULL) < 0) )
    {
      LOG ("Loading `%s' plugin failed: %s\n",
	   plugin->short_libname,
	   "can't convert plugin name to local encoding");
      free (plugin->libname);
      plugin->libname = NULL;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
  }
  plugin->libraryHandle = lt_dlopenadvise (llibname,
					   advise);
#else
  plugin->libraryHandle = lt_dlopenadvise (plugin->libname,
					   advise);
#endif
  lt_dladvise_destroy (&advise);
  if (NULL == plugin->libraryHandle)
    {
      LOG ("Loading `%s' plugin failed (using name `%s'): %s\n",
	   plugin->short_libname,
	   plugin->libname,
	   lt_dlerror ());
      free (plugin->libname);
      plugin->libname = NULL;
      plugin->flags = EXTRACTOR_OPTION_DISABLED;
      return -1;
    }
  plugin->extract_method = get_symbol_with_prefix (plugin->libraryHandle,
						   "_EXTRACTOR_%s_extract_method",
						   plugin->libname,
						   &plugin->specials);
  if (NULL == plugin->extract_method)
    {
      LOG ("Resolving `extract' method of plugin `%s' failed: %s\n",
	   plugin->short_libname,
	   lt_dlerror ());
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
 * @param options options to pass to the plugin
 * @param flags options to use
 * @return the new list of libraries, equal to prev iff an error occured
 */
struct EXTRACTOR_PluginList *
EXTRACTOR_plugin_add (struct EXTRACTOR_PluginList *prev,
		      const char *library,
		      const char *options,
		      enum EXTRACTOR_Options flags)
{
  struct EXTRACTOR_PluginList *plugin;
  struct EXTRACTOR_PluginList *pos;
  char *libname;

  for (pos = prev; NULL != pos; pos = pos->next)
    if (0 == strcmp (pos->short_libname, library))
      return prev; /* no change, library already loaded */
  if (NULL == (libname = EXTRACTOR_find_plugin_ (library)))
    {
      LOG ("Could not load plugin `%s'\n",
	   library);
      return prev;
    }
  if (NULL == (plugin = malloc (sizeof (struct EXTRACTOR_PluginList))))
    return prev;
  memset (plugin, 0, sizeof (struct EXTRACTOR_PluginList));
  plugin->next = prev;
  if (NULL == (plugin->short_libname = strdup (library)))
    {
      free (plugin);
      return NULL;
    }
  plugin->libname = libname;
  plugin->flags = flags;
  if (NULL != options)
    plugin->plugin_options = strdup (options);
  else
    plugin->plugin_options = NULL;
  plugin->seek_request = -1;
  return plugin;
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
EXTRACTOR_plugin_add_config (struct EXTRACTOR_PluginList *prev,
			     const char *config,
			     enum EXTRACTOR_Options flags)
{
  char *cpy;
  size_t pos;
  size_t last;
  ssize_t lastconf;
  size_t len;

  if (NULL == config)
    return prev;
  if (NULL == (cpy = strdup (config)))
    return prev;
  len = strlen (config);
  pos = 0;
  last = 0;
  lastconf = 0;
  while (pos < len)
    {
      while ( (':' != cpy[pos]) &&
	      ('\0' != cpy[pos]) &&
	      ('(' != cpy[pos]) )
	pos++;
      switch (cpy[pos])
	{
	case '(':
	  cpy[pos++] = '\0';	/* replace '(' by termination */
	  lastconf = pos;         /* start config from here, after (. */
	  while ( ('\0' != cpy[pos]) &&
		  (')' != cpy[pos]))
	    pos++; /* config until ) or EOS. */
	  if (')' == cpy[pos])
	    {
	      cpy[pos++] = '\0'; /* write end of config here. */
	      while ( (':' != cpy[pos]) &&
		      ('\0' != cpy[pos]) )
		pos++; /* forward until real end of string found. */
	      cpy[pos++] = '\0';
	    }
	  else
	    {
	      cpy[pos++] = '\0'; /* end of string. */
	    }
	  break;
	case ':':
	case '\0':
	  lastconf = -1;         /* NULL config when no (). */
	  cpy[pos++] = '\0';	/* replace ':' by termination */
	  break;
	default:
	  ABORT ();
	}
      if ('-' == cpy[last])
	{
	  last++;
	  prev = EXTRACTOR_plugin_remove (prev,
					  &cpy[last]);
	}
      else
	{
	  prev = EXTRACTOR_plugin_add (prev,
				       &cpy[last],
				       (-1 != lastconf) ? &cpy[lastconf] : NULL,
				       flags);
	}
      last = pos;
    }
  free (cpy);
  return prev;
}


/**
 * Remove a plugin from a list.
 *
 * @param prev the current list of plugins
 * @param library the name of the plugin to remove
 * @return the reduced list, unchanged if the plugin was not loaded
 */
struct EXTRACTOR_PluginList *
EXTRACTOR_plugin_remove (struct EXTRACTOR_PluginList *prev,
			 const char *library)
{
  struct EXTRACTOR_PluginList *pos;
  struct EXTRACTOR_PluginList *first;

  pos = prev;
  first = prev;
  while ( (NULL != pos) &&
	  (0 != strcmp (pos->short_libname, library)) )
    {
      prev = pos;
      pos = pos->next;
    }
  if (NULL == pos)
    {
      LOG ("Unloading plugin `%s' failed!\n",
	   library);
      return first;
    }
  /* found, close library */
  if (first == pos)
    first = pos->next;
  else
    prev->next = pos->next;
  if (NULL != pos->channel)
    EXTRACTOR_IPC_channel_destroy_ (pos->channel);
  if ( (NULL != pos->shm) &&
       (0 == EXTRACTOR_IPC_shared_memory_change_rc_ (pos->shm, -1)) )
    EXTRACTOR_IPC_shared_memory_destroy_ (pos->shm);
  if (NULL != pos->libname)
    free (pos->libname);
  free (pos->plugin_options);
  if (NULL != pos->libraryHandle)
	lt_dlclose (pos->libraryHandle);
  free (pos);
  return first;
}


/**
 * Remove all plugins from the given list (destroys the list).
 *
 * @param plugin the list of plugins
 */
void
EXTRACTOR_plugin_remove_all (struct EXTRACTOR_PluginList *plugins)
{
  while (NULL != plugins)
    plugins = EXTRACTOR_plugin_remove (plugins, plugins->short_libname);
}


/* end of extractor_plugins.c */
