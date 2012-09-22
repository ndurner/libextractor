/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006, 2009 Vidyut Samanta and Christian Grothoff

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
/**
 * @file main/extractor_print.c
 * @brief convenience functions for printing meta data
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include "extractor_logging.h"
#if HAVE_ICONV
#include "iconv.c"
#endif

/**
 * Simple EXTRACTOR_MetaDataProcessor implementation that simply
 * prints the extracted meta data to the given file.  Only prints
 * those keywords that are in UTF-8 format.
 * 
 * @param handle the file to write to (stdout, stderr), must NOT be NULL,
 *               must be of type "FILE *".
 * @param plugin_name name of the plugin that produced this value
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data 
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return non-zero if printing failed, otherwise 0.
 */
int 
EXTRACTOR_meta_data_print (void *handle,
			   const char *plugin_name,
			   enum EXTRACTOR_MetaType type,
			   enum EXTRACTOR_MetaFormat format,
			   const char *data_mime_type,
			   const char *data,
			   size_t data_len)
{
#if HAVE_ICONV
  iconv_t cd;
#endif
  char * buf;
  int ret;
  const char *mt;

  if (EXTRACTOR_METAFORMAT_UTF8 != format)
    return 0;
#if HAVE_ICONV
  cd = iconv_open (nl_langinfo(CODESET),
		   "UTF-8");
  if (((iconv_t) -1) == cd)
    {
      LOG_STRERROR ("iconv_open");
      return 1;
    }
  buf = iconv_helper (cd, data, data_len);
  if (NULL == buf)
    {
      LOG_STRERROR ("iconv_helper");
      ret = -1;
    }
  else
    {
      mt = EXTRACTOR_metatype_to_string (type);
      ret = fprintf (handle,
		     "%s - %s\n",
		     (NULL == mt) 
		     ? dgettext ("libextractor", gettext_noop ("unknown"))
		     : dgettext ("libextractor", mt),
		     buf);
      free(buf);
    }
  iconv_close(cd);
#else
  ret = fprintf (handle,
		 "%s - %.*s\n",
		 (NULL == mt) 
		 ? dgettext ("libextractor", gettext_noop ("unknown"))
		 : dgettext ("libextractor", mt),
		 (int) data_len,
		 data);
#endif
  return (ret < 0) ? 1 : 0;
}

/* end of extractor_print.c */
