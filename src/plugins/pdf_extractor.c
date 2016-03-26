/*
     This file is part of libextractor.
     Copyright (C) 2016 Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */
/**
 * @file plugins/pdf_extractor.c
 * @brief plugin to support PDF files
 * @author Christian Grothoff
 *
 * PDF libraries today are a nightmare (TM).  So instead of doing the
 * fast thing and calling some library functions to parse the PDF,
 * we execute 'pdfinfo' and parse the output. Because that's 21st
 * century plumbing: nobody writes reasonable code anymore.
 */
#include "platform.h"
#include <extractor.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

/**
 * Entry in the mapping from control data to LE types.
 */
struct Matches
{
  /**
   * Key in the Pdfian control file.
   */
  const char *text;

  /**
   * Corresponding type in LE.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * Map from pdf-control entries to LE types.
 *
 * See output of 'pdfinfo'.
 */
static struct Matches tmap[] = {
  {"Title",        EXTRACTOR_METATYPE_TITLE},
  {"Subject",      EXTRACTOR_METATYPE_SUBJECT},
  {"Keywords",     EXTRACTOR_METATYPE_KEYWORDS},
  {"Author",       EXTRACTOR_METATYPE_AUTHOR_NAME},
  {"Creator",      EXTRACTOR_METATYPE_CREATOR},
  {"Producer",     EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE},
  {"CreationDate", EXTRACTOR_METATYPE_CREATION_DATE},
  {"ModDate",      EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"PDF version",  EXTRACTOR_METATYPE_ENCODER_VERSION},
  {"Pages",        EXTRACTOR_METATYPE_PAGE_COUNT},
  {NULL, 0}
};


/**
 * Process the "stdout" file from pdfinfo.
 *
 * @param fout stdout of pdfinfo
 * @param proc function to call with meta data
 * @param proc_cls closure for @e proc
 */
static void
process_stdout (FILE *fout,
		EXTRACTOR_MetaDataProcessor proc,
		void *proc_cls)
{
  unsigned int i;
  char line[1025];
  const char *psuffix;
  const char *colon;

  while (! feof (fout))
    {
      if (NULL == fgets (line, sizeof (line) - 1, fout))
        break;
      if (0 == strlen (line))
        continue;
      if ('\n' == line[strlen(line)-1])
        line[strlen(line)-1] = '\0';
      colon = strchr (line, (int) ':');
      if (NULL == colon)
        break;
      psuffix = colon + 1;
      while (isblank ((int) psuffix[0]))
        psuffix++;
      if (0 == strlen (psuffix))
        continue;
      for (i = 0; NULL != tmap[i].text; i++)
        {
          if (0 != strncasecmp (line,
                                tmap[i].text,
                                colon - line))
	    continue;
	  if (0 != proc (proc_cls,
			 "pdf",
			 tmap[i].type,
			 EXTRACTOR_METAFORMAT_UTF8,
			 "text/plain",
			 psuffix,
			 strlen(psuffix) + 1))
            return;
	  break;
	}
    }
}


/**
 * Main entry method for the PDF extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_pdf_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  uint64_t fsize;
  void *data;
  pid_t pid;
  int in[2];
  int out[2];
  FILE *fout;
  uint64_t pos;

  fsize = ec->get_size (ec->cls);
  if (fsize < 128)
    return;
  if (4 !=
      ec->read (ec->cls, &data, 4))
    return;
  if (0 != strncmp ("%PDF", data, 4))
    return;
  if (0 !=
      ec->seek (ec->cls, 0, SEEK_SET))
    return;
  if (0 != pipe (in))
    return;
  if (0 != pipe (out))
    {
      close (in[0]);
      close (in[1]);
      return;
    }
  pid = fork ();
  if (-1 == pid)
    {
      close (in[0]);
      close (in[1]);
      close (out[0]);
      close (out[1]);
      return;
    }
  if (0 == pid)
    {
      char *const args[] = {
        "pdfinfo",
        "-",
        NULL
      };
      /* am child, exec 'pdfinfo' */
      close (0);
      close (1);
      dup2 (in[0], 0);
      dup2 (out[1], 1);
      close (in[0]);
      close (in[1]);
      close (out[0]);
      close (out[1]);
      execvp ("pdfinfo", args);
      exit (1);
    }
  /* am parent, send file */
  close (in[0]);
  close (out[1]);
  fout = fdopen (out[0], "r");

  pos = 0;
  while (pos < fsize)
    {
      ssize_t got;
      size_t wpos;

      data = NULL;
      got = ec->read (ec->cls,
                      &data,
                      fsize - pos);
      if ( (-1 == got) ||
           (NULL == data) )
        break;
      wpos = 0;
      while (wpos < got)
        {
          ssize_t out;

          out = write (in[1], data + wpos, got - wpos);
          if (out <= 0)
            break;
          wpos += out;
        }
      if (wpos < got)
        break;
      pos += got;
    }
  close (in[1]);
  process_stdout (fout, ec->proc, ec->cls);
  fclose (fout);
  kill (pid, SIGKILL);
  waitpid (pid, NULL, 0);
}

/* end of pdf_extractor.c */
