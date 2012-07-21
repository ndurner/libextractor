#ifndef EXTRACTOR_PLUGPATH_H
#define EXTRACTOR_PLUGPATH_H

/**
 * Function to call on paths.
 * 
 * @param cls closure
 * @param path a directory path
 */
typedef void (*EXTRACTOR_PathProcessor)(void *cls,
					const char *path);


/**
 * Iterate over all paths where we expect to find GNU libextractor
 * plugins.
 *
 * @param pp function to call for each path
 * @param pp_cls cls argument for pp.
 */
void
EXTRACTOR_get_installation_paths_ (EXTRACTOR_PathProcessor pp,
				   void *pp_cls);


/**
 * Given a short name of a library (i.e. "mime"), find
 * the full path of the respective plugin.
 */
char * 
EXTRACTOR_find_plugin_ (const char *short_name);


#endif 
/* EXTRACTOR_PLUGPATH_H */
