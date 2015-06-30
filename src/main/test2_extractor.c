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
 * @file main/test2_extractor.c
 * @brief plugin for testing GNU libextractor
 * Data file (or buffer) for this test must be 150 * 1024 bytes long,
 * first 4 bytes must be "test", all other bytes should be equal to
 * <FILE_OFFSET> % 256. The test client must return 0 after seeing
 * "Hello World!" metadata, and return 1 after seeing "Goodbye!"
 * metadata.
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

  

/**
 * Signature of the extract method that each plugin
 * must provide.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_test2_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *dp;

  if ((NULL == ec->config) || (0 != strcmp (ec->config, "test2")))
    return; /* only run in test mode */
  if (4 != ec->read (ec->cls, &dp, 4))
    {
      fprintf (stderr, "Reading at offset 0 failed\n");
      ABORT (); 
    }
  if (0 != strncmp ("test", dp, 4))
    {
      fprintf (stderr, "Unexpected data at offset 0\n");
      ABORT (); 
    }
  if ( (1024 * 150 != ec->get_size (ec->cls)) &&
       (UINT64_MAX != ec->get_size (ec->cls)) )
    {
      fprintf (stderr, "Unexpected file size returned (expected 150k)\n");
      ABORT (); 
    }		    
  if (1024 * 100 + 4 != ec->seek (ec->cls, 1024 * 100 + 4, SEEK_SET))
    {
      fprintf (stderr, "Failure to seek (SEEK_SET)\n");
      ABORT ();
    }
  if (1 != ec->read (ec->cls, &dp, 1))
    {
      fprintf (stderr, "Failure to read at 100k + 4\n");
      ABORT ();
    }
  if ((1024 * 100 + 4) % 256 != * (unsigned char *) dp)
    {
      fprintf (stderr, "Unexpected data at offset 100k + 4\n");
      ABORT ();
    }
  if (((1024 * 100 + 4) + 1 - (1024 * 50 + 7)) !=
      ec->seek (ec->cls, - (1024 * 50 + 7), SEEK_CUR))
    {
      fprintf (stderr, "Failure to seek (SEEK_SET)\n");
      ABORT ();
    }
  if (1 != ec->read (ec->cls, &dp, 1))
    {
      fprintf (stderr, "Failure to read at 50k - 3\n");
      ABORT ();
    }
  if (((1024 * 100 + 4) + 1 - (1024 * 50 + 7)) % 256 != * (unsigned char *) dp)
    {
      fprintf (stderr, "Unexpected data at offset 100k - 3\n");
      ABORT ();
    }
  if (1024 * 150 != ec->seek (ec->cls, 0, SEEK_END))
    {
      fprintf (stderr, "Failure to seek (SEEK_END)\n");
      ABORT ();
    }
  if (0 != ec->read (ec->cls, &dp, 1))
    {
      fprintf (stderr, "Failed to receive EOF at 150k\n");
      ABORT ();
    }
  if (1024 * 150 - 2 != ec->seek (ec->cls, -2, SEEK_END))
    {
      fprintf (stderr, "Failure to seek (SEEK_END - 2)\n");
      ABORT ();
    }
  if (1 != ec->read (ec->cls, &dp, 1))
    {
      fprintf (stderr, "Failure to read at 150k - 3\n");
      ABORT ();
    }
  if ((1024 * 150 - 2) % 256 != * (unsigned char *) dp)
    {
      fprintf (stderr, "Unexpected data at offset 150k - 3\n");
      ABORT ();
    }
}

/* end of test2_extractor.c */
