/**
 * @file test/mt-plugintest1.c
 * @brief test extractor plugin load/unload from multiple threads
 * simultaneously - thumbnailer plugins test
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
      el = EXTRACTOR_addLibrary (NULL, "libextractor_thumbnailgtk");
      el = EXTRACTOR_removeLibrary (el, "libextractor_thumbnailgtk");
      if (el != NULL)
        {
          printf ("add-remove test failed (gtk) in thread %d!\n", td->id);
          failed = 1;
        }
      el = EXTRACTOR_addLibrary (NULL, "libextractor_thumbnailqt");
      el = EXTRACTOR_removeLibrary (el, "libextractor_thumbnailqt");
      if (el != NULL)
        {
          printf ("add-remove test failed (qt) in thread %d!\n", td->id);
          failed = 1;
        }
      el = EXTRACTOR_addLibrary (NULL, "libextractor_thumbnailffmpeg");
      el = EXTRACTOR_removeLibrary (el, "libextractor_thumbnailffmpeg");
      if (el != NULL)
        {
          printf ("add-remove test failed (ffmpeg) in thread %d!\n", td->id);
          failed = 1;
        }
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
