/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2009 Vidyut Samanta and Christian Grothoff

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

#include "platform.h"
#include "extractor.h"

int 
EXTRACTOR_template_extract (const unsigned char *data,
			    size_t size,
			    EXTRACTOR_MetaDataProcessor proc,
			    void *proc_cls,
			    const char *options)
{
  if (0 != proc (proc_cls,
		 "template",
		 EXTRACTOR_METATYPE_RESERVED,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "foo",
		 strlen ("foo")+1))
    return 1;
  /* insert more here */
  return 0;
}
