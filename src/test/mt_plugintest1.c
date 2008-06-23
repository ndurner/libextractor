/**
 * @file test/mt-plugintest1.c
 * @brief test extractor plugin load/unload from multiple threads
 * simultaneously
 * @author Heikki Lindholm
 */
#include "platform.h"
#include "extractor.h"
#include <pthread.h>

struct TaskData
{
  int id;
};

static volatile int done = 0;
static volatile int failed = 0;

static void *
test_plugins (void *arg)
{
  struct TaskData *td = (struct TaskData *) arg;
  EXTRACTOR_ExtractorList *el;
  int i;

  while (!done)
    {
      /* do some loading and unloading */
      for (i = 0; i < 10; i++)
        {
          el = EXTRACTOR_loadDefaultLibraries ();
          EXTRACTOR_removeAll (el);
        }

      /* do some load/unload tests */
      el = EXTRACTOR_addLibrary (NULL, "libextractor_split");
      el = EXTRACTOR_addLibrary (el, "libextractor_mime");
      el = EXTRACTOR_addLibrary (el, "libextractor_filename");
      el = EXTRACTOR_removeLibrary (el, "libextractor_mime");
      el = EXTRACTOR_removeLibrary (el, "libextractor_split");
      el = EXTRACTOR_removeLibrary (el, "libextractor_filename");
      if (el != NULL)
        {
          printf ("add-remove test (1) failed in thread %d!\n", td->id);
          failed = 1;
        }

      el = EXTRACTOR_addLibrary (NULL, "libextractor_split");
      el = EXTRACTOR_addLibrary (el, "libextractor_mime");
      el = EXTRACTOR_addLibrary (el, "libextractor_filename");
      el = EXTRACTOR_removeLibrary (el, "libextractor_mime");
      el = EXTRACTOR_removeLibrary (el, "libextractor_filename");
      el = EXTRACTOR_removeLibrary (el, "libextractor_split");
      if (el != NULL)
        {
          printf ("add-remove test (2) failed in thread %d!\n", td->id);
          failed = 1;
        }

      el = EXTRACTOR_loadConfigLibraries (NULL, "libextractor_filename");
      el = EXTRACTOR_loadConfigLibraries (el, "-libextractor_split");
      EXTRACTOR_removeAll (el);
    }
  return 0;
}

#define TEST_SECS 10

int
main (int argc, char *argv[])
{
  int num_tasks = 10;
  pthread_t task_list[num_tasks];
  struct TaskData td[num_tasks];
  int ret = 0;
  int i;

  printf("testing for %d seconds\n", TEST_SECS);
  for (i = 0; i < num_tasks; i++)
    {
      td[i].id = i;
      ret = pthread_create (&task_list[i], NULL, test_plugins, &td[i]);
      if (ret != 0)
        {
          printf ("ERROR: pthread_create failed for thread %d\n", i);
          num_tasks = i;
          done = 1;
          break;
        }
    }
  if (!done)
    sleep (TEST_SECS);
  done = 1;
  for (i = 0; i < num_tasks; i++)
    {
      if (pthread_join (task_list[i], NULL) != 0)
        printf ("WARNING: pthread_join failed for thread %d\n", i);
    }

  return failed;
}
