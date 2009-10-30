/*
     This file is part of libextractor.
     (C) 2002, 2003, 2008 Vidyut Samanta and Christian Grothoff

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

#include "platform.h"
#include "extractor.h"
#include <stdint.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

/* ******************** pipe feeder ************************ */

struct PipeArgs {			       
  const char * data;
  size_t pos;
  size_t size;
  int pi[2];
  int shutdown;
};

static void *
pipe_feeder(void * args)
{
  ssize_t ret;
  struct PipeArgs * p = args;

  while ( (p->shutdown == 0) &&
	  (0 < (ret = WRITE(p->pi[1],
			    &p->data[p->pos],
			    p->size - p->pos))) )
	  p->pos += ret;
  CLOSE(p->pi[1]);
  return NULL;			    
}

static void
sigalrmHandler (int sig)
{
  /* do nothing */
}


/* *************** real libextractor stuff ***************** */

static struct EXTRACTOR_Keywords *
addKeyword (EXTRACTOR_KeywordType type,
            const char *keyword, struct EXTRACTOR_Keywords *next)
{
  EXTRACTOR_KeywordList *result;

  if (keyword == NULL)
    return next;
  result = malloc (sizeof (EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = strdup (keyword);
  result->keywordType = type;
  return result;
}

typedef struct
{
  int32_t rtype;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tests[] = {
  {RPMTAG_NAME, EXTRACTOR_TITLE},
  {RPMTAG_VERSION, EXTRACTOR_VERSIONNUMBER},
  {RPMTAG_RELEASE, EXTRACTOR_RELEASE},
  {RPMTAG_GROUP, EXTRACTOR_GROUP},
  {RPMTAG_SIZE, EXTRACTOR_SIZE},
  {RPMTAG_URL, EXTRACTOR_RESOURCE_IDENTIFIER},
  {RPMTAG_SUMMARY, EXTRACTOR_SUMMARY},
  {RPMTAG_PACKAGER, EXTRACTOR_PACKAGER},
  {RPMTAG_BUILDTIME, EXTRACTOR_CREATION_DATE},
#ifdef RPMTAG_COPYRIGHT
  {RPMTAG_COPYRIGHT, EXTRACTOR_COPYRIGHT},
#endif
  {RPMTAG_LICENSE, EXTRACTOR_LICENSE},
  {RPMTAG_DISTRIBUTION, EXTRACTOR_DISTRIBUTION},
  {RPMTAG_BUILDHOST, EXTRACTOR_BUILDHOST},
  {RPMTAG_VENDOR, EXTRACTOR_VENDOR},
  {RPMTAG_OS, EXTRACTOR_OS},
  {RPMTAG_DESCRIPTION, EXTRACTOR_DESCRIPTION},
  {0, 0},
};

static int discardCB(rpmlogRec rec, void *ctx) {
  /* do nothing! */
  return 0;
}

/* mimetype = application/x-rpm */
struct EXTRACTOR_Keywords *
libextractor_rpm_extract (const char *filename,
                          const char *data,
                          size_t size, struct EXTRACTOR_Keywords *prev)
{
  struct PipeArgs parg;
  pthread_t pthr;
  void * unused;
  const char *str;
  Header hdr;
  HeaderIterator hi;
  rpmtd p;
  int i;
  FD_t fdi;
  rpmRC rc;
  rpmts ts;
  struct sigaction sig;
  struct sigaction old;

  if (0 != pipe(parg.pi))
    return prev;
  fdi = NULL;
  parg.data = data;
  parg.pos = 0;
  parg.size = size;
  parg.shutdown = 0;
  if (0 != pthread_create(&pthr,
			  NULL,
			  &pipe_feeder,
			  &parg))
    {
      CLOSE(parg.pi[0]);
      CLOSE(parg.pi[1]);
      return prev;
    }
  rpmlogSetCallback(&discardCB, NULL);
  fdi = fdDup(parg.pi[0]);
  ts = rpmtsCreate();
  rc = rpmReadPackageFile (ts, fdi, "GNU libextractor", &hdr);
  switch (rc)
    {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
      break;
    case RPMRC_NOTFOUND:
    case RPMRC_FAIL:
    default:
      goto END;
    }
  prev = addKeyword (EXTRACTOR_MIMETYPE,
                     "application/x-rpm", prev);
  hi = headerInitIterator (hdr);
  p = rpmtdNew ();
  while (1 == headerNext (hi, p))
    {
      i = 0;
      while (tests[i].rtype != 0)
        {
          if (tests[i].rtype == p->tag)
            {
              switch (p->type)
                {
                case RPM_STRING_ARRAY_TYPE:
                case RPM_I18NSTRING_TYPE:
                case RPM_STRING_TYPE:
                    while (NULL != (str = rpmtdNextString (p))) 
		      prev = addKeyword (tests[i].type, str, prev);
                    break;
                case RPM_INT32_TYPE:
                  {
                    if (p->tag == RPMTAG_BUILDTIME)
                      {
                        char tmp[30];

                        ctime_r ((time_t *) p, tmp);
                        tmp[strlen (tmp) - 1] = '\0';   /* eat linefeed */
                        prev = addKeyword (tests[i].type, tmp, prev);
                      }
                    else
                      {
                        char tmp[14];

                        sprintf (tmp, "%d", *(int *) p);
                        prev = addKeyword (tests[i].type, tmp, prev);
                      }
                    break;
                  }
		default:
			break;
                }
            }
          i++;
        }
    }
  rpmtdFree (p);
  headerFreeIterator (hi);
  headerFree (hdr);
  rpmtsFree(ts);
 END:						
  /* make sure SIGALRM does not kill us */
  memset (&sig, 0, sizeof (struct sigaction));
  memset (&old, 0, sizeof (struct sigaction));
  sig.sa_flags = SA_NODEFER;
  sig.sa_handler = &sigalrmHandler;
  sigaction (SIGALRM, &sig, &old);
  parg.shutdown = 1;
  pthread_kill(pthr, SIGALRM);
  pthread_join(pthr, &unused);
  sigaction (SIGALRM, &old, &sig);
  Fclose(fdi);
  CLOSE(parg.pi[0]);
  return prev;
}

/* end of rpmextractor.c */
