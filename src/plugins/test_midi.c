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
 * @file plugins/test_midi.c
 * @brief testcase for midi plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the MIDI testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData midi_dth_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/midi",
	strlen ("audio/midi") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_COPYRIGHT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"(c) 2012 d-o-o",
	strlen ("(c) 2012 d-o-o"),
	0 
      },
      { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Tage wie diese T2",
	strlen ("Tage wie diese T2"),
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"XFhd:::Rock:8 Beat:1:m1:-:-:-:-:DD",
	strlen ("XFhd:::Rock:8 Beat:1:m1:-:-:-:-:DD"),
	0 
      },
      { 
	EXTRACTOR_METATYPE_COMMENT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"XFln:L1:Tage wie diese:von Holst:von Holst:-:Toten Hosen:DD",
	strlen ("XFln:L1:Tage wie diese:von Holst:von Holst:-:Toten Hosen:DD"),
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/midi_dth.mid",
	midi_dth_sol },
      { NULL, NULL }
    };
  return ET_main ("midi", ps);
}

/* end of test_midi.c */
