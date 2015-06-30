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
 * @file plugins/test_lib.h
 * @brief helper library for writing testcases
 * @author Christian Grothoff
 */
#ifndef TEST_LIB_H
#define TEST_LIB_H

#include "extractor.h"

/**
 * Expected outcome from the plugin.
 */
struct SolutionData
{
  /**
   * Expected type.
   */
  enum EXTRACTOR_MetaType type;

  /**
   * Expected format.
   */
  enum EXTRACTOR_MetaFormat format;

  /**
   * Expected data mime type.
   */
  const char *data_mime_type;

  /**
   * Expected meta data.
   */
  const char *data;

  /**
   * Expected number of bytes in meta data.
   */
  size_t data_len;

  /**
   * Internally used flag to say if this solution was
   * provided by the plugin; 0 for no, 1 for yes; -1 to
   * terminate the list.
   */
  int solved;
};


/**
 * Set of problems 
 */
struct ProblemSet
{
  /**
   * File to run the extractor on, NULL
   * to terminate the array.
   */
  const char *filename;

  /**
   * Expected meta data.  Terminate array with -1 in 'solved'.
   */
  struct SolutionData *solution;

};


/**
 * Main function to be called to test a plugin.
 *
 * @param plugin_name name of the plugin to load
 * @param ps array of problems the plugin should solve;
 *        NULL in filename terminates the array. 
 * @return 0 on success, 1 on failure
 */
int 
ET_main (const char *plugin_name,
	 struct ProblemSet *ps);

#endif
