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
 * @file plugins/test_wav.c
 * @brief testcase for ogg plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"



/**
 * Main function for the WAV testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData wav_noise_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/x-wav",
	strlen ("audio/x-wav") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_RESOURCE_TYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1000 ms, 48000 Hz, mono",
	strlen ("1000 ms, 48000 Hz, mono") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct SolutionData wav_alert_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/x-wav",
	strlen ("audio/x-wav") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_RESOURCE_TYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"525 ms, 22050 Hz, mono",
	strlen ("525 ms, 22050 Hz, mono") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/wav_noise.wav",
	wav_noise_sol },
      { "testdata/wav_alert.wav",
	wav_alert_sol },
      { NULL, NULL }
    };
  return ET_main ("wav", ps);
}

/* end of test_wav.c */
