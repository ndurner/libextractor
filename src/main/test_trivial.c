/**
 * @file main/test_trivial.c
 * @brief trivial testcase for libextractor plugin loading
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"

static int
testLoadPlugins (enum EXTRACTOR_Options policy)
{
  struct EXTRACTOR_PluginList *pl;
  
  pl = EXTRACTOR_plugin_add_defaults (policy);
  if (pl == NULL)
    {
      fprintf (stderr,
	       "Failed to load default plugins!\n");
      return 1;
    }
  EXTRACTOR_plugin_remove_all (pl);
  return 0;
}

int
main (int argc, char *argv[])
{
  int ret = 0;

  ret += testLoadPlugins (EXTRACTOR_OPTION_DEFAULT_POLICY);
  ret += testLoadPlugins (EXTRACTOR_OPTION_DEFAULT_POLICY);
  ret += testLoadPlugins (EXTRACTOR_OPTION_DEFAULT_POLICY);
  ret += testLoadPlugins (EXTRACTOR_OPTION_OUT_OF_PROCESS_NO_RESTART);
  return ret;
}
