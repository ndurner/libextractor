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
 * @file main/test_plugin_loading.c
 * @brief testcase for dynamic loading and unloading of plugins
 */
#include "platform.h"
#include "extractor.h"

int
main (int argc, char *argv[])
{
  struct EXTRACTOR_PluginList *arg;

  /* change environment to find 'extractor_test' plugin which is 
     not installed but should be in the current directory (or .libs)
     on 'make check' */
  if (0 != putenv ("LIBEXTRACTOR_PREFIX=." PATH_SEPARATOR_STR ".libs/"))
    fprintf (stderr, 
	     "Failed to update my environment, plugin loading may fail: %s\n",
	     strerror (errno));

  /* do some load/unload tests */
  arg = EXTRACTOR_plugin_add (NULL, "test", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY);
  if (arg != EXTRACTOR_plugin_add (arg, "test", NULL, EXTRACTOR_OPTION_DEFAULT_POLICY))
    {
      fprintf (stderr,
	       "Could load plugin twice, that should not be allowed\n");
    }
  arg = EXTRACTOR_plugin_remove (arg, "test");
  if (NULL != arg)
    {
      fprintf (stderr,
	       "add-remove test failed!\n");
      return -1;
    }
  return 0;
}

/* end of test_plugin_loading.c */
