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
 * @file plugins/test_rpm.c
 * @brief testcase for ogg plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Expected package summary text.
 */
#define SUMMARY "The GNU libtool, which simplifies the use of shared libraries."

/**
 * Expected package description text.
 */
#define DESCRIPTION "The libtool package contains the GNU libtool, a set of shell scripts\n"\
  "which automatically configure UNIX and UNIX-like architectures to\n"	\
  "generically build shared libraries.  Libtool provides a consistent,\n" \
  "portable interface which simplifies the process of using shared\n"	\
  "libraries.\n"							\
  "\n"									\
  "If you are developing programs which will use shared libraries, you\n" \
  "should install libtool."


/**
 * Main function for the RPM testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData rpm_test_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"application/x-rpm",
	strlen ("application/x-rpm") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_NAME,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"libtool",
	strlen ("libtool") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SOFTWARE_VERSION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1.5",
	strlen ("1.5") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_VERSION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"6",
	strlen ("6") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUMMARY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	SUMMARY,
	strlen (SUMMARY) + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_DESCRIPTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	DESCRIPTION,
	strlen (DESCRIPTION) + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Thu Oct  2 11:44:33 2003",
	strlen ("Thu Oct  2 11:44:33 2003") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_BUILDHOST,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"bullwinkle.devel.redhat.com",
	strlen ("bullwinkle.devel.redhat.com") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_INSTALLED_SIZE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2623621", 
	strlen ("2623621") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DISTRIBUTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Red Hat Linux",
	strlen ("Red Hat Linux") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_VENDOR,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Red Hat, Inc.",
	strlen ("Red Hat, Inc.") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LICENSE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"GPL",
	strlen ("GPL") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_MAINTAINER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Red Hat, Inc. <http://bugzilla.redhat.com/bugzilla>",
	strlen ("Red Hat, Inc. <http://bugzilla.redhat.com/bugzilla>") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SECTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Development/Tools",
	strlen ("Development/Tools") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_URL,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"http://www.gnu.org/software/libtool/",
	strlen ("http://www.gnu.org/software/libtool/") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TARGET_OS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"linux",
	strlen ("linux") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TARGET_ARCHITECTURE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"ia64",
	strlen ("ia64") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_PROVIDES,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"libtool",
	strlen ("libtool") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"/bin/sh",
	strlen ("/bin/sh") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"/bin/sh",
	strlen ("/bin/sh") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"/bin/sh",
	strlen ("/bin/sh") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"/sbin/install-info",
	strlen ("/sbin/install-info") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"autoconf",
	strlen ("autoconf") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"automake",
	strlen ("automake") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"libtool-libs",
	strlen ("libtool-libs") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"m4",
	strlen ("m4") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"mktemp",
	strlen ("mktemp") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"perl",
	strlen ("perl") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"rpmlib(CompressedFileNames)",
	strlen ("rpmlib(CompressedFileNames)") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"rpmlib(PayloadFilesHavePrefix)",
	strlen ("rpmlib(PayloadFilesHavePrefix)") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"rpmlib(VersionedDependencies)",
	strlen ("rpmlib(VersionedDependencies)") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_TARGET_PLATFORM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"ia64-redhat-linux-gnu",
	strlen ("ia64-redhat-linux-gnu") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/rpm_test.rpm",
	rpm_test_sol },
      { NULL, NULL }
    };
  return ET_main ("rpm", ps);
}

/* end of test_rpm.c */
