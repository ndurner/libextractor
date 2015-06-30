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
 * @file main/test_gzip.c
 * @brief testcase for gzip decompression in the extractor
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"

/**
 * Return value from main, set to 0 for test to succeed.
 */
static int ret = 2;

#define HLO "Hello world!"
#define GOB "Goodbye!"

/**
 * Function that libextractor calls for each
 * meta data item found.  Should be called once
 * with 'Hello World!" and once with "Goodbye!".
 *
 * @param cls closure should be "main-cls"
 * @param plugin_name should be "test"
 * @param type should be "COMMENT"
 * @param format should be "UTF8"
 * @param data_mime_type should be "<no mime>"
 * @param data hello world or good bye
 * @param data_len number of bytes in data
 * @return 0 on hello world, 1 on goodbye
 */ 
static int
process_replies (void *cls,
		 const char *plugin_name,
		 enum EXTRACTOR_MetaType type,
		 enum EXTRACTOR_MetaFormat format,
		 const char *data_mime_type,
		 const char *data,
		 size_t data_len)
{
  if (0 != strcmp (cls,
		   "main-cls"))
    {
      fprintf (stderr, "closure invalid\n");
      ret = 3;
      return 1;
    }
  if (0 == strcmp (plugin_name,
		   "<zlib>"))
    return 0; /* skip this one */
  if (0 != strcmp (plugin_name,
		   "test"))
    {
      fprintf (stderr, "plugin name invalid: `%s'\n",
	       plugin_name);
      ret = 4;
      return 1;
    }
  if (EXTRACTOR_METATYPE_COMMENT != type)
    {
      fprintf (stderr, "type invalid\n");
      ret = 5;
      return 1;
    }
  if (EXTRACTOR_METAFORMAT_UTF8 != format)
    {
      fprintf (stderr, "format invalid\n");
      ret = 6;
      return 1;
    }
  if ( (NULL == data_mime_type) ||
       (0 != strcmp ("<no mime>",
		     data_mime_type) ) )
    {
      fprintf (stderr, "bad mime type\n");
      ret = 7;
      return 1;
    }
  if ( (2 == ret) &&
       (data_len == strlen (HLO) + 1) &&
       (0 == strncmp (data,
		      HLO,
		      strlen (HLO))) )
    {
#if 0
      fprintf (stderr, "Received '%s'\n", HLO);
#endif
      ret = 1;
      return 0;
    }
  if ( (1 == ret) &&
       (data_len == strlen (GOB) + 1) &&
       (0 == strncmp (data,
		      GOB,
		      strlen (GOB))) )
    {
#if 0
      fprintf (stderr, "Received '%s'\n", GOB);
#endif
      ret = 0;
      return 1;
    }
  fprintf (stderr, "Invalid meta data\n");
  ret = 8;
  return 1;
}


/**
 * Main function for the gzip testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct EXTRACTOR_PluginList *pl;

  /* change environment to find 'extractor_test' plugin which is 
     not installed but should be in the current directory (or .libs)
     on 'make check' */
  if (0 != putenv ("LIBEXTRACTOR_PREFIX=." PATH_SEPARATOR_STR ".libs/"))
    fprintf (stderr, 
	     "Failed to update my environment, plugin loading may fail: %s\n",
	     strerror (errno));    
  pl = EXTRACTOR_plugin_add_config (NULL, "test(test)",
				    EXTRACTOR_OPTION_DEFAULT_POLICY);
  if (NULL == pl)
    {
      fprintf (stderr, "failed to load test plugin\n");
      return 1;
    }
  EXTRACTOR_extract (pl, "test_file.dat.gz", NULL, 0, &process_replies, "main-cls");
  EXTRACTOR_plugin_remove_all (pl);
  return ret;
}

/* end of test_gzip.c */
