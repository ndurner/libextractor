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
 * @file main/extractor_logging.h
 * @brief logging API for GNU libextractor
 * @author Christian Grothoff
 */
#ifndef EXTRACTOR_LOGGING_H
#define EXTRACTOR_LOGGING_H

#define DEBUG 0

#if DEBUG

/**
 * Log function.
 *
 * @param file name of file with the error
 * @param line line number with the error
 * @param ... log message and arguments
 */
void
EXTRACTOR_log_ (const char *file, int line, const char *format, ...);

/**
 * Log a message.
 *
 * @param ... format string and arguments for fmt (printf-style)
 */
#define LOG(...) EXTRACTOR_log_(__FILE__, __LINE__, __VA_ARGS__)

#else

/**
 * Log a message.
 *
 * @param ... format string and arguments for fmt (printf-style)
 */
#define LOG(...)

#endif


/**
 * Log an error message about a failed system/libc call
 * using an error message based on 'errno'.
 *
 * @param syscall name of the syscall that failed
 */
#define LOG_STRERROR(syscall) LOG("System call `%s' failed: %s\n", syscall, STRERROR (errno))


/**
 * Log an error message about a failed system/libc call
 * involving a file using an error message based on 'errno'.
 *
 * @param syscall name of the syscall that failed
 * @param filename name of the file that was involved
 */
#define LOG_STRERROR_FILE(syscall,filename) LOG("System call `%s' failed for file `%s': %s\n", syscall, filename, STRERROR (errno))


/**
 * Abort the program reporting an assertion failure
 *
 * @param file filename with the failure
 * @param line line number with the failure
 */
void
EXTRACTOR_abort_ (const char *file,
		  int line);


/**
 * Abort program if assertion fails.
 *
 * @param cond assertion that must hold.
 */
#define ASSERT(cond) do { if (! (cond)) EXTRACTOR_abort_ (__FILE__, __LINE__); } while (0)


#endif
