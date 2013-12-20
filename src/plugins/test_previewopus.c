/*
     This file is part of libextractor.
     (C) 2013 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/test_previewopus.c
 * @brief stub testcase for previewopus plugin
 * @author Bruno Cabral
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the previewopus testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData previewopus_audio_sol[] =
    {
		//TODO. Can't test as it depends on the encoder.
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/mpeg_alien.mpg",
	previewopus_audio_sol },
      { NULL, NULL }
    };
  return ET_main ("previewopus", ps);
}

/* end of test_thumbnailffmpeg.c */
