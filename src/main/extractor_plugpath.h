#if !defined(EXTRACTOR_PLUGPATH_H)
#define EXTRACTOR_PLUGPATH_H

struct DefaultLoaderContext
{
  struct EXTRACTOR_PluginList *res;
  enum EXTRACTOR_Options flags;
};

/**
 * Function to call on paths.
 * 
 * @param cls closure
 * @param path a directory path
 */
typedef void (*PathProcessor)(void *cls,
			      const char *path);

/**
 * Iterate over all paths where we expect to find GNU libextractor
 * plugins.
 *
 * @param pp function to call for each path
 * @param pp_cls cls argument for pp.
 */
void
get_installation_paths (PathProcessor pp,
			void *pp_cls);

/**
 * Given a short name of a library (i.e. "mime"), find
 * the full path of the respective plugin.
 */
char *
find_plugin (const char *short_name);

/**
 * Load all plugins from the given directory.
 * 
 * @param cls pointer to the "struct EXTRACTOR_PluginList*" to extend
 * @param path path to a directory with plugins
 */
void
load_plugins_from_dir (void *cls,
		       const char *path);

#endif /* EXTRACTOR_PLUGPATH_H */
