/**
 * @file main/test_plugin_load_multi.c
 * @brief testcase for libextractor plugin loading that loads the same
 *    plugins multiple times!
 * @author Christian Grothoff
 */

#include "platform.h"
#include "extractor.h"

static int
testLoadPlugins ()
{
  struct EXTRACTOR_PluginList *el1;
  struct EXTRACTOR_PluginList *el2;

  el1 = EXTRACTOR_plugin_add_defaults (EXTRACTOR_OPTION_DEFAULT_POLICY);
  el2 = EXTRACTOR_plugin_add_defaults (EXTRACTOR_OPTION_DEFAULT_POLICY);
  if ((el1 == NULL) || (el2 == NULL))
    {
      fprintf (stderr,
	       "Failed to load default plugins!\n");
      if (el1 != NULL)
	EXTRACTOR_plugin_remove_all (el1);
      if (el2 != NULL)
	EXTRACTOR_plugin_remove_all (el2);
      return 1;
    }
  EXTRACTOR_plugin_remove_all (el1);
  EXTRACTOR_plugin_remove_all (el2);
  return 0;
}

int
main (int argc, char *argv[])
{
  int ret = 0;

  ret += testLoadPlugins ();
  ret += testLoadPlugins ();
  return ret;
}
