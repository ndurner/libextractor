/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/test_lib.c
 * @brief helper library for writing testcases
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Function that libextractor calls for each
 * meta data item found.
 *
 * @param cls closure the 'struct SolutionData' we are currently working on
 * @param plugin_name should be "test"
 * @param type should be "COMMENT"
 * @param format should be "UTF8"
 * @param data_mime_type should be "<no mime>"
 * @param data hello world or good bye
 * @param data_len number of bytes in data
 * @return 0 (always)
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
  struct SolutionData *sd = cls;
  unsigned int i;

  for (i=0; -1 != sd[i].solved; i++)
    {
      if ( (0 != sd[i].solved) ||
	   (sd[i].type != type) ||
	   (sd[i].format != format) ||
	   (sd[i].data_len != data_len) ||
	   (0 != memcmp (sd[i].data, data, data_len)) )
	continue;
      if (NULL != sd[i].data_mime_type)
	{
	  if (NULL == data_mime_type)
	    continue;
	  if (0 != strcmp (sd[i].data_mime_type, data_mime_type))
	    continue;
	}
      else
	{
	  if (NULL != data_mime_type)
	    continue;
	}
      sd[i].solved = 1;
      return 0;
    }
  fprintf (stderr, 
	   "Got additional meta data of type %d and format %d with value `%.*s' from plugin `%s'\n",
	   type,
	   format,
	   (int) data_len,
	   data,
	   plugin_name);
  return 0;
}


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
	 struct ProblemSet *ps)
{
  struct EXTRACTOR_PluginList *pl;
  unsigned int i;
  unsigned int j;
  int ret;
  
  /* change environment to find plugins which may not yet be
     not installed but should be in the current directory (or .libs)
     on 'make check' */
  if (0 != putenv ("LIBEXTRACTOR_PREFIX=." PATH_SEPARATOR_STR ".libs/"))
    fprintf (stderr, 
	     "Failed to update my environment, plugin loading may fail: %s\n",
	     strerror (errno));    
  pl = EXTRACTOR_plugin_add_config (NULL, 
				    plugin_name,
				    EXTRACTOR_OPTION_IN_PROCESS);
  for (i=0; NULL != ps[i].filename; i++)
    EXTRACTOR_extract (pl,
		       ps[i].filename,
		       NULL, 0, 
		       &process_replies,
		       ps[i].solution);   
  EXTRACTOR_plugin_remove_all (pl);
  ret = 0;
  for (i=0; NULL != ps[i].filename; i++)    
    for (j=0; -1 != ps[i].solution[j].solved; j++)
      if (0 == ps[i].solution[j].solved)
	ret = 1;
  return ret;
}



/* end of test_lib.c */
