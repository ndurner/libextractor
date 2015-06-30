/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2005, 2006, 2009 Vidyut Samanta and Christian Grothoff

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
 * @file main/test_plugin_load_multi.c
 * @brief testcase for libextractor plugin loading that loads the same
 *    plugins multiple times!
 * @author Christian Grothoff
 */

#include "platform.h"
#include "extractor.h"


static int
testLoadPlugins ()
{
  struct EXTRACTOR_PluginList *el1;
  struct EXTRACTOR_PluginList *el2;

  el1 = EXTRACTOR_plugin_add_defaults (EXTRACTOR_OPTION_DEFAULT_POLICY);
  el2 = EXTRACTOR_plugin_add_defaults (EXTRACTOR_OPTION_DEFAULT_POLICY);
  if ((NULL == el1) || (NULL == el2))
    {
      fprintf (stderr,
	       "Failed to load default plugins!\n");
      if (NULL != el1)
	EXTRACTOR_plugin_remove_all (el1);
      if (NULL != el2)
	EXTRACTOR_plugin_remove_all (el2);
      return 1;
    }
  EXTRACTOR_plugin_remove_all (el1);
  EXTRACTOR_plugin_remove_all (el2);
  return 0;
}


int
main (int argc, char *argv[])
{
  int ret = 0;

  /* change environment to find 'extractor_test' plugin which is 
     not installed but should be in the current directory (or .libs)
     on 'make check' */
  if (0 != putenv ("LIBEXTRACTOR_PREFIX=." PATH_SEPARATOR_STR ".libs/"))
    fprintf (stderr, 
	     "Failed to update my environment, plugin loading may fail: %s\n",
	     strerror (errno));
  ret += testLoadPlugins ();
  ret += testLoadPlugins ();
  return ret;
}

/* end of test_plugin_load_multi.c */
