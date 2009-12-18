/*
     This file is part of libextractor.
     (C) 2002, 2003, 2008, 2009 Vidyut Samanta and Christian Grothoff

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

typedef struct
{
  int32_t rtype;
  enum EXTRACTOR_MetaType type;
} Matches;

static Matches tests[] = {
  {RPMTAG_NAME, EXTRACTOR_METATYPE_PACKAGE_NAME},
  {RPMTAG_VERSION, EXTRACTOR_METATYPE_SOFTWARE_VERSION},
  {RPMTAG_GROUP, EXTRACTOR_METATYPE_SECTION},
  {RPMTAG_SIZE, EXTRACTOR_METATYPE_PACKAGE_INSTALLED_SIZE},
  {RPMTAG_SUMMARY, EXTRACTOR_METATYPE_SUMMARY},
  {RPMTAG_PACKAGER, EXTRACTOR_METATYPE_PACKAGE_MAINTAINER},
  {RPMTAG_BUILDTIME, EXTRACTOR_METATYPE_CREATION_DATE},
#ifdef RPMTAG_COPYRIGHT
  {RPMTAG_COPYRIGHT, EXTRACTOR_METATYPE_COPYRIGHT},
#endif
  {RPMTAG_LICENSE, EXTRACTOR_METATYPE_LICENSE},
  {RPMTAG_DISTRIBUTION, EXTRACTOR_METATYPE_PACKAGE_DISTRIBUTION},
  {RPMTAG_BUILDHOST, EXTRACTOR_METATYPE_BUILDHOST},
  {RPMTAG_VENDOR, EXTRACTOR_METATYPE_VENDOR},
  {RPMTAG_OS, EXTRACTOR_METATYPE_TARGET_OS},
  {RPMTAG_DESCRIPTION, EXTRACTOR_METATYPE_DESCRIPTION},
  {RPMTAG_URL, EXTRACTOR_METATYPE_URL},
  {RPMTAG_DISTURL, EXTRACTOR_METATYPE_URL},
  {RPMTAG_RELEASE, EXTRACTOR_METATYPE_PACKAGE_VERSION},
  {RPMTAG_PLATFORM, EXTRACTOR_METATYPE_TARGET_PLATFORM},
  {RPMTAG_ARCH, EXTRACTOR_METATYPE_TARGET_ARCHITECTURE},
  {RPMTAG_CONFLICTNAME, EXTRACTOR_METATYPE_PACKAGE_CONFLICTS},
  {RPMTAG_REQUIRENAME, EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY},
  {RPMTAG_CONFLICTNAME, EXTRACTOR_METATYPE_PACKAGE_CONFLICTS},
  {RPMTAG_PROVIDENAME, EXTRACTOR_METATYPE_PACKAGE_PROVIDES},

#if 0
  {RPMTAG_CHANGELOGTEXT, EXTRACTOR_METATYPE_REVISION_HISTORY},
#endif

#if 0
  /* FIXME: add support for some of these */
    RPMTAG_GIF			= 1012,	/* x */
    RPMTAG_XPM			= 1013,	/* x */
    RPMTAG_SOURCE		= 1018,	/* s[] */
    RPMTAG_PATCH		= 1019,	/* s[] */
    RPMTAG_PREIN		= 1023,	/* s */
    RPMTAG_POSTIN		= 1024,	/* s */
    RPMTAG_PREUN		= 1025,	/* s */
    RPMTAG_POSTUN		= 1026,	/* s */
    RPMTAG_ICON			= 1043, /* x */
    RPMTAG_SOURCERPM		= 1044,	/* s */
    RPMTAG_PROVIDENAME		= 1047,	/* s[] */
    RPMTAG_EXCLUDEARCH		= 1059, /* s[] */
    RPMTAG_EXCLUDEOS		= 1060, /* s[] */
    RPMTAG_EXCLUSIVEARCH	= 1061, /* s[] */
    RPMTAG_EXCLUSIVEOS		= 1062, /* s[] */
    RPMTAG_TRIGGERSCRIPTS	= 1065,	/* s[] */
    RPMTAG_TRIGGERNAME		= 1066,	/* s[] */
    RPMTAG_TRIGGERVERSION	= 1067,	/* s[] */
    RPMTAG_VERIFYSCRIPT		= 1079,	/* s */
    RPMTAG_PREINPROG		= 1085,	/* s */
    RPMTAG_POSTINPROG		= 1086,	/* s */
    RPMTAG_PREUNPROG		= 1087,	/* s */
    RPMTAG_POSTUNPROG		= 1088,	/* s */
    RPMTAG_BUILDARCHS		= 1089, /* s[] */
    RPMTAG_OBSOLETENAME		= 1090,	/* s[] */
    RPMTAG_VERIFYSCRIPTPROG	= 1091,	/* s */
    RPMTAG_TRIGGERSCRIPTPROG	= 1092,	/* s[] */
    RPMTAG_COOKIE		= 1094,	/* s */
    RPMTAG_FILELANGS		= 1097,	/* s[] */
    RPMTAG_PREFIXES		= 1098,	/* s[] */
    RPMTAG_INSTPREFIXES		= 1099,	/* s[] */
    RPMTAG_PROVIDEVERSION	= 1113,	/* s[] */
    RPMTAG_OBSOLETEVERSION	= 1115,	/* s[] */
    RPMTAG_BASENAMES		= 1117,	/* s[] */
    RPMTAG_DIRNAMES		= 1118,	/* s[] */
    RPMTAG_OPTFLAGS		= 1122,	/* s */
    RPMTAG_PAYLOADFORMAT	= 1124,	/* s */
    RPMTAG_PAYLOADCOMPRESSOR	= 1125,	/* s */
    RPMTAG_PAYLOADFLAGS		= 1126,	/* s */
    RPMTAG_CLASSDICT		= 1142,	/* s[] */
    RPMTAG_SOURCEPKGID		= 1146,	/* x */
    RPMTAG_PRETRANS		= 1151,	/* s */
    RPMTAG_POSTTRANS		= 1152,	/* s */
    RPMTAG_PRETRANSPROG		= 1153,	/* s */
    RPMTAG_POSTTRANSPROG	= 1154,	/* s */
    RPMTAG_DISTTAG		= 1155,	/* s */
#endif
  {0, 0},
};

static int discardCB(rpmlogRec rec, void *ctx) {
  /* do nothing! */
  return 0;
}

/* mimetype = application/x-rpm */
int 
EXTRACTOR_rpm_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
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
    return 0;
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
      return 0;
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

  if (0 != proc (proc_cls, 
		 "rpm",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "application/x-rpm",
		 strlen ("application/x-rpm") +1))
    return 1;
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
		      {
			if (0 != proc (proc_cls, 
				       "rpm",
				       tests[i].type,
				       EXTRACTOR_METAFORMAT_UTF8,
				       "text/plain",
				       str,
				       strlen (str) +1))
			  return 1;
		      }
                    break;
                case RPM_INT32_TYPE:
                  {
                    if (p->tag == RPMTAG_BUILDTIME)
                      {
                        char tmp[30];

                        ctime_r ((time_t *) p, tmp);
                        tmp[strlen (tmp) - 1] = '\0';   /* eat linefeed */

			if (0 != proc (proc_cls, 
				       "rpm",
				       tests[i].type,
				       EXTRACTOR_METAFORMAT_UTF8,
				       "text/plain",
				       tmp,
				       strlen (tmp) +1))
			  return 1;
                      }
                    else
                      {
                        char tmp[14];

                        sprintf (tmp, "%d", *(int *) p);
			if (0 != proc (proc_cls, 
				       "rpm",
				       tests[i].type,
				       EXTRACTOR_METAFORMAT_UTF8,
				       "text/plain",
				       tmp,
				       strlen (tmp) +1))
			  return 1;
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
  return 0;
}

/* end of rpm_extractor.c */
