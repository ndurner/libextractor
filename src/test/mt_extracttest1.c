/**
 * @file test/mt_extracttest1.c
 * @brief test keyword extraction from multiple threads simultaneously
 * @author Heikki Lindholm
 */
#include "platform.h"
#include "extractor.h"
#include <pthread.h>

struct TaskData
{
  int id;
  const char *filename;
};

static volatile int done = 0;
static volatile int failed = 0;

pthread_mutex_t reference_lock = PTHREAD_MUTEX_INITIALIZER;
static EXTRACTOR_KeywordList *reference_list;

static int
compare_keywords_to_ref (EXTRACTOR_KeywordList * list)
{
  EXTRACTOR_KeywordList *ptr1, *ptr2;
  unsigned int cnt;
  int *match;
  int i;

  cnt = EXTRACTOR_countKeywords (list);

  pthread_mutex_lock (&reference_lock);

  if (cnt != EXTRACTOR_countKeywords (reference_list))
    {
      pthread_mutex_unlock (&reference_lock);
      return -1;
    }

  match = alloca (cnt * sizeof (int));
  memset (match, 0x00, cnt * sizeof (int));
  ptr1 = list;
  while (ptr1 != NULL)
    {
      int found;
      found = 0;
      ptr2 = reference_list;
      i = 0;
      while (ptr2 != NULL)
        {
          if (ptr2->keywordType == ptr1->keywordType &&
              strcmp (ptr2->keyword, ptr1->keyword) == 0 && match[i] == 0)
            {
              found = 1;
              match[i] = 1;
              break;
            }
          i++;
          ptr2 = ptr2->next;
        }
      if (found == 0)
        break;
      ptr1 = ptr1->next;
    }

  pthread_mutex_unlock (&reference_lock);
  for (i = 0; i < cnt; i++)
    if (match[i] == 0)
      return -1;

  return 0;
}

static EXTRACTOR_KeywordList *
get_keywords_for_file (const char *filename)
{
  EXTRACTOR_ExtractorList *el;
  EXTRACTOR_KeywordList *list;

  el = EXTRACTOR_loadDefaultLibraries ();
  if (el == NULL)
    {
      printf ("ERROR: failed to load plugins!\n");
      return NULL;
    }
  errno = 0;
  list = EXTRACTOR_getKeywords (el, filename);
  if (errno != 0) {
    printf("ERROR: EXTRACTOR_getKeywords: %s\n", strerror(errno));
  }
  /*EXTRACTOR_printKeywords (stderr, list); */
  EXTRACTOR_removeAll (el);

  return list;
}

static void *
test_plugins (void *arg)
{
  struct TaskData *td = (struct TaskData *)arg;
  while (!done)
    {
      EXTRACTOR_KeywordList *list;

      list = get_keywords_for_file (td->filename);

      if ((list == NULL) || (compare_keywords_to_ref (list) != 0))
        {
          printf ("ERROR: thread id %d failed keyword comparison!\n", td->id);
          failed = 1;
        }
      if (list != NULL)
        EXTRACTOR_freeKeywords (list);
    }
  return 0;
}

static const char *filename = TESTDATADIR "/text2.sxw";

#define TEST_SECS 10

int
main (int argc, char *argv[])
{
  int num_tasks = 10;
  pthread_t task_list[num_tasks];
  struct TaskData td[num_tasks];
  int ret = 0;
  int i;

  printf("testing with '%s' for %d seconds\n", filename, TEST_SECS);
  reference_list = get_keywords_for_file (filename);

    for (i = 0; i < num_tasks; i++)
      {
        td[i].id = i;
        td[i].filename = filename;
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

    if (reference_list != NULL)
      EXTRACTOR_freeKeywords (reference_list);

  return failed;
}
