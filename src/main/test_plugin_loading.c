/**
 * @file main/test_plugin_loading.c
 * @brief testcase for dynamic loading and unloading of plugins
 */
#include "platform.h"
#include "extractor.h"

int
main (int argc, char *argv[])
{
  struct EXTRACTOR_PluginList *arg;

  /* do some load/unload tests */
  arg = EXTRACTOR_plugin_add (NULL, "mime", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  arg = EXTRACTOR_plugin_add (arg, "png", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  arg = EXTRACTOR_plugin_add (arg, "zip", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  arg = EXTRACTOR_plugin_remove (arg, "mime");
  arg = EXTRACTOR_plugin_remove (arg, "zip");
  arg = EXTRACTOR_plugin_remove (arg, "png");
  if (arg != NULL)
    {
      fprintf (stderr,
	       "add-remove test failed!\n");
      return -1;
    }

  arg = EXTRACTOR_plugin_add (NULL, "mime", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  arg = EXTRACTOR_plugin_add (arg, "png", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  arg = EXTRACTOR_plugin_add (arg, "zip", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  arg = EXTRACTOR_plugin_remove (arg, "zip");
  arg = EXTRACTOR_plugin_remove (arg, "mime");
  arg = EXTRACTOR_plugin_remove (arg, "png");
  if (arg != NULL)
    {
      fprintf (stderr,
	       "add-remove test failed!\n");
      return -1;
    }
  return 0;
}
