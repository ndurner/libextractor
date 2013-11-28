/*
     This file is part of libextractor.
     (C) 2002, 2003, 2008, 2009, 2012 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
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
 * @file plugins/rpm_extractor.c
 * @brief plugin to support RPM files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <stdint.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#if SOMEBSD
#include <pthread_np.h>
#else
#include <pthread.h>
#endif
#include <sys/types.h>
#include <signal.h>


/**
 * Closure for the 'pipe_feeder'.
 */
struct PipeArgs
{

  /**
   * Context for reading data from.
   */
  struct EXTRACTOR_ExtractContext *ec;

  /**
   * Lock for synchronizing access to 'ec'.
   */
  pthread_mutex_t lock;

  /**
   * Pipe to write to at [1].
   */
  int pi[2];

  /**
   * Set to 1 if we should stop writing to the pipe.
   */
  int shutdown;
};


/**
 * Size of the buffer we use for reading.
 */
#define BUF_SIZE (16 * 1024)


/**
 * Main function of a helper thread that passes the package data
 * to librpm.
 *
 * @param args the 'struct PipeArgs*'
 * @return NULL
 */
static void *
pipe_feeder (void * args)
{
  struct PipeArgs *p = args;
  ssize_t rret;
  ssize_t wret;
  ssize_t done;
  void *ptr;
  char *buf;

  /* buffer is heap-allocated as this is a thread and
     large stack allocations might not be the best idea */
  while (0 == p->shutdown)
    {
      pthread_mutex_lock (&p->lock);
      if (-1 == (rret = p->ec->read (p->ec->cls, &ptr, BUF_SIZE)))
	{
	  pthread_mutex_unlock (&p->lock);
	  break;
	}
      pthread_mutex_unlock (&p->lock);
      if (0 == rret)
	break;
      buf = ptr;
      done = 0;
      while ( (0 == p->shutdown) &&
	      (done < rret) )
	{
	  if (-1 == (wret = WRITE (p->pi[1],
				   &buf[done],
				   rret - done)))
	    {
	      break;
	    }
	  if (0 == wret)
	    break;
	  done += wret;
	}
      if (done != rret)
	break;
    }
  CLOSE (p->pi[1]);
  return NULL;
}


/**
 * LOG callback called by librpm.  Does nothing, we
 * just need this to override the default behavior.
 */
static int
discard_log_callback (rpmlogRec rec,
		      void *ctx)
{
  /* do nothing! */
  return 0;
}


/**
 * Mapping from RPM tags to LE types.
 */
struct Matches
{
  /**
   * RPM tag.
   */
  int32_t rtype;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * List of mappings from RPM tags to LE types.
 */
static struct Matches tests[] = {
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
  {0, 0}
};


/**
 * Main entry method for the 'application/x-rpm' extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_rpm_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  struct PipeArgs parg;
  pthread_t pthr;
  void *unused;
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

  /* FIXME: here it might be worthwhile to do some minimal
     check to see if this is actually an RPM before we go
     and create a pipe and a thread for nothing... */
  parg.ec = ec;
  parg.shutdown = 0;
  if (0 != pipe (parg.pi))
    return;
  if (0 != pthread_mutex_init (&parg.lock, NULL))
    {
      CLOSE (parg.pi[0]);
      CLOSE (parg.pi[1]);
      return;
    }
  if (0 != pthread_create (&pthr,
			   NULL,
			   &pipe_feeder,
			   &parg))
    {
      pthread_mutex_destroy (&parg.lock);
      CLOSE (parg.pi[0]);
      CLOSE (parg.pi[1]);
      return;
    }
  rpmlogSetCallback (&discard_log_callback, NULL);
  fdi = fdDup (parg.pi[0]);
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
  pthread_mutex_lock (&parg.lock);
  if (0 != ec->proc (ec->cls,
		     "rpm",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "application/x-rpm",
		     strlen ("application/x-rpm") +1))
    {
      pthread_mutex_unlock (&parg.lock);
      goto END;
    }
  pthread_mutex_unlock (&parg.lock);
  hi = headerInitIterator (hdr);
  p = rpmtdNew ();
  while (1 == headerNext (hi, p))
    for (i = 0; 0 != tests[i].rtype; i++)
      {
	if (tests[i].rtype != p->tag)
	  continue;
	switch (p->type)
	  {
	  case RPM_STRING_ARRAY_TYPE:
	  case RPM_I18NSTRING_TYPE:
	  case RPM_STRING_TYPE:
	    while (NULL != (str = rpmtdNextString (p)))
	      {
		pthread_mutex_lock (&parg.lock);
		if (0 != ec->proc (ec->cls,
				   "rpm",
				   tests[i].type,
				   EXTRACTOR_METAFORMAT_UTF8,
				   "text/plain",
				   str,
				   strlen (str) + 1))

		  {
		    pthread_mutex_unlock (&parg.lock);
		    goto CLEANUP;
		  }
		pthread_mutex_unlock (&parg.lock);
	      }
	    break;
	  case RPM_INT32_TYPE:
	    {
	      if (p->tag == RPMTAG_BUILDTIME)
		{
		  char tmp[30];
		  uint32_t *v = rpmtdNextUint32 (p);
		  time_t tp = (time_t) *v;

		  ctime_r (&tp, tmp);
		  tmp[strlen (tmp) - 1] = '\0';   /* eat linefeed */
		  pthread_mutex_lock (&parg.lock);
		  if (0 != ec->proc (ec->cls,
				     "rpm",
				     tests[i].type,
				     EXTRACTOR_METAFORMAT_UTF8,
				     "text/plain",
				     tmp,
				     strlen (tmp) + 1))
		    {
		      pthread_mutex_unlock (&parg.lock);
		      goto CLEANUP;
		    }
		  pthread_mutex_unlock (&parg.lock);
		}
	      else
		{
		  char tmp[14];
		  uint32_t *s = rpmtdNextUint32 (p);

		  snprintf (tmp,
			    sizeof (tmp),
			    "%u",
			    (unsigned int) *s);
		  pthread_mutex_lock (&parg.lock);
		  if (0 != ec->proc (ec->cls,
				     "rpm",
				     tests[i].type,
				     EXTRACTOR_METAFORMAT_UTF8,
				     "text/plain",
				     tmp,
				     strlen (tmp) + 1))
		    {
		      pthread_mutex_unlock (&parg.lock);
		      goto CLEANUP;
		    }
		  pthread_mutex_unlock (&parg.lock);
		}
	      break;
	    }
	  default:
	    break;
	  }
      }
 CLEANUP:
  rpmtdFree (p);
  headerFreeIterator (hi);

 END:
  headerFree (hdr);
  rpmtsFree(ts);

  /* make sure SIGALRM does not kill us, then use it to
     kill the thread */
  memset (&sig, 0, sizeof (struct sigaction));
  memset (&old, 0, sizeof (struct sigaction));
  sig.sa_flags = SA_NODEFER;
  sig.sa_handler = SIG_IGN;
  sigaction (SIGALRM, &sig, &old);
  parg.shutdown = 1;
  CLOSE (parg.pi[0]);
  Fclose (fdi);
  pthread_kill (pthr, SIGALRM);
  pthread_join (pthr, &unused);
  pthread_mutex_destroy (&parg.lock);
  sigaction (SIGALRM, &old, &sig);
}

/* end of rpm_extractor.c */
