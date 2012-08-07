/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/template_extractor.c
 * @brief example code for writing your own plugin
 * @author add your own name here
 */
#include "platform.h"
#include "extractor.h"


/**
 * This will be the main method of your plugin.
 * Describe a bit what it does here.
 *
 * @param ec extraction context, here you get the API
 *   for accessing the file data and for returning
 *   meta data
 */
void
EXTRACTOR_template_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  int64_t offset;
  void *data;

  /* temporary variables are declared here */

  if (plugin == NULL)
    return 1;

  /* initialize state here */
  
  /* Call seek (plugin, POSITION, WHENCE) to seek (if you know where
   * data starts):
   */
  // ec->seek (ec->cls, POSITION, SEEK_SET);

  /* Call read (plugin, &data, COUNT) to read COUNT bytes 
   */


  /* Once you find something, call proc(). If it returns non-0 - you're done.
   */
  // if (0 != ec->proc (ec->cls, ...)) return;

  /* Don't forget to free anything you've allocated before returning! */
  return;
}

/* end of template_extractor.c */
