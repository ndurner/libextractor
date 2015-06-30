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
 * @file plugins/test_mime.c
 * @brief testcase for ogg plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"
#include <magic.h>


/**
 * Main function for the MIME testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  int result = 0;
  int test_result;
  int test_result_around_19, test_result_around_22;
  struct SolutionData courseclear_file_around_19_sol[] =
    {
      {
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
        /* not sure which is the exact version, but old ones do
           not even define MAGIC_VERSION, so this is approximately
           right. Users where this tests fail should report
           their version number from "magic.h" so we can adjust
           if necessary. */
#ifdef MAGIC_VERSION
	"audio/ogg",
	strlen ("audio/ogg") + 1,
#else
        "application/ogg",
	strlen ("application/ogg") + 1,
#endif
	0
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct SolutionData courseclear_file_around_22_sol[] =
    {
      {
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/ogg",
	strlen ("audio/ogg") + 1,
	0
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct SolutionData gif_image_sol[] =
    {
      {
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"image/gif",
	strlen ("image/gif") + 1,
	0
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps_gif[] =
    {
      { "testdata/gif_image.gif",
	gif_image_sol },
      { NULL, NULL }
    };
  struct ProblemSet ps_ogg_around_19[] =
    {
      { "testdata/ogg_courseclear.ogg",
	courseclear_file_around_19_sol },
      { NULL, NULL }
    };
  struct ProblemSet ps_ogg_around_22[] =
    {
      { "testdata/ogg_courseclear.ogg",
	courseclear_file_around_22_sol },
      { NULL, NULL }
    };
  printf ("Running gif test on libmagic:\n");
  test_result = (0 == ET_main ("mime", ps_gif) ? 0 : 1);
  printf ("gif libmagic test result: %s\n", test_result == 0 ? "OK" : "FAILED");
  result += test_result;

  printf ("Running ogg test on libmagic, assuming version ~5.19:\n");
  test_result_around_19 = (0 == ET_main ("mime", ps_ogg_around_19) ? 0 : 1);
  printf ("ogg libmagic test result: %s\n", test_result_around_19 == 0 ? "OK" : "FAILED");

  printf ("Running ogg test on libmagic, assuming version ~5.22:\n");
  test_result_around_22 = (0 == ET_main ("mime", ps_ogg_around_22) ? 0 : 1);
  printf ("ogg libmagic test result: %s\n", test_result_around_22 == 0 ? "OK" : "FAILED");

  if ((test_result_around_19 != 0) && (test_result_around_22 != 0))
    result++;
  return result;
}

/* end of test_mime.c */
