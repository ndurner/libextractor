/*
     This file is part of libextractor.
     Copyright (C) 2012 Vidyut Samanta and Christian Grothoff

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
 * @file main/extractor_logging.c
 * @brief logging API for GNU libextractor
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor_logging.h"

#if DEBUG
/**
 * Log function.
 *
 * @param file name of file with the error
 * @param line line number with the error
 * @param ... log message and arguments
 */
void
EXTRACTOR_log_ (const char *file, int line, const char *format, ...)
{
  va_list va;

  fprintf (stderr,
	   "EXTRACTOR %s:%d ",
	   file, line);
  va_start (va, format); 
  vfprintf (stderr, format, va);
  va_end (va);
  fflush (stderr);
}
#endif


/**
 * Abort the program reporting an assertion failure
 *
 * @param file filename with the failure
 * @param line line number with the failure
 */
void
EXTRACTOR_abort_ (const char *file,
		  int line)
{
#if DEBUG
  EXTRACTOR_log_ (file, line, "Assertion failed.\n");
#endif
  ABORT ();
}

/* end of extractor_logging.c */
