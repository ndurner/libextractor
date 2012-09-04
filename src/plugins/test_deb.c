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
 * @file plugins/test_deb.c
 * @brief testcase for deb plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the DEB testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData deb_bzip2_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/x-debian-package",
	strlen ("application/x-debian-package") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_NAME,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"bzip2",
	strlen ("bzip2") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_VERSION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1.0.6-4",
	strlen ("1.0.6-4") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TARGET_ARCHITECTURE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"i386",
	strlen ("i386") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_MAINTAINER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Anibal Monsalve Salazar <anibal@debian.org>",
	strlen ("Anibal Monsalve Salazar <anibal@debian.org>") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_INSTALLED_SIZE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"113", /* FIXME: should this be 'kb'? */
	strlen ("113") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"libbz2-1.0 (= 1.0.6-4), libc6 (>= 2.4)",
	strlen ("libbz2-1.0 (= 1.0.6-4), libc6 (>= 2.4)") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_SUGGESTS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"bzip2-doc",
	strlen ("bzip2-doc") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_REPLACES,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"libbz2 (<< 0.9.5d-3)",
	strlen ("libbz2 (<< 0.9.5d-3)") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SECTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"utils",
	strlen ("utils") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_UPLOAD_PRIORITY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"standard",
	strlen ("standard") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_DESCRIPTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"high-quality block-sorting file compressor - utilities\n"
" bzip2 is a freely available, patent free, high-quality data compressor.\n"
" It typically compresses files to within 10% to 15% of the best available\n"
" techniques, whilst being around twice as fast at compression and six\n"
" times faster at decompression.\n"
" .\n"
" bzip2 compresses files using the Burrows-Wheeler block-sorting text\n"
" compression algorithm, and Huffman coding.  Compression is generally\n"
" considerably better than that achieved by more conventional\n"
" LZ77/LZ78-based compressors, and approaches the performance of the PPM\n"
" family of statistical compressors.\n"
" .\n"
" The archive file format of bzip2 (.bz2) is incompatible with that of its\n"
" predecessor, bzip (.bz).",
	strlen ("high-quality block-sorting file compressor - utilities\n"
" bzip2 is a freely available, patent free, high-quality data compressor.\n"
" It typically compresses files to within 10% to 15% of the best available\n"
" techniques, whilst being around twice as fast at compression and six\n"
" times faster at decompression.\n"
" .\n"
" bzip2 compresses files using the Burrows-Wheeler block-sorting text\n"
" compression algorithm, and Huffman coding.  Compression is generally\n"
" considerably better than that achieved by more conventional\n"
" LZ77/LZ78-based compressors, and approaches the performance of the PPM\n"
" family of statistical compressors.\n"
" .\n"
" The archive file format of bzip2 (.bz2) is incompatible with that of its\n"
" predecessor, bzip (.bz).") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/deb_bzip2.deb",
	deb_bzip2_sol },
      { NULL, NULL }
    };
  return ET_main ("deb", ps);
}

/* end of test_deb.c */
