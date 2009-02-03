/*
     This file is part of libextractor.
     (C) 2007, 2008, 2009 Vidyut Samanta and Christian Grothoff

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

#define NUM_TASKS 10

int
main (int argc, char *argv[])
{
  pthread_t task_list[NUM_TASKS];
  struct TaskData td[NUM_TASKS];
  int ret = 0;
  int i;
  int max = NUM_TASKS;
  void * unused;

  printf("testing for %d seconds\n", TEST_SECS);
  for (i = 0; i < NUM_TASKS; i++)
    {
      td[i].id = i;
      ret = pthread_create (&task_list[i], NULL, &test_plugins, &td[i]);
      if (ret != 0)
        {
          printf ("ERROR: pthread_create failed for thread %d\n", i);
          max = i;
          done = 1;
          break;
        }
    }
  printf("Threads running!\n");
  if (!done)
    sleep (TEST_SECS);
  printf("Shutting down...\n");
  done = 1;
  for (i = 0; i < max; i++)
    {
      if (pthread_join (task_list[i], &unused) != 0)
        printf ("WARNING: pthread_join failed for thread %d\n", i);
    }

  return failed;
}
