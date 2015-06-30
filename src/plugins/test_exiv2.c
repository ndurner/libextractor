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
 * @file plugins/test_exiv2.c
 * @brief testcase for exiv2 plugin
 * @author Christian Grothoff
 */
#include "platform.h"
#include "test_lib.h"


/**
 * Main function for the EXIV2 testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  struct SolutionData exiv2_iptc_sol[] =
    {
      { 
	EXTRACTOR_METATYPE_GPS_LATITUDE_REF,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"North",
	strlen ("North") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_GPS_LATITUDE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"28deg 8' 17.585\" ",
	strlen ("28deg 8' 17.585\" ") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_GPS_LONGITUDE_REF,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"West",
	strlen ("West") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_GPS_LONGITUDE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"14deg 14' 21.713\" ",
	strlen ("14deg 14' 21.713\" ") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CAMERA_MAKE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"PENTAX Corporation",
	strlen ("PENTAX Corporation") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CAMERA_MODEL,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"PENTAX Optio W30",
	strlen ("PENTAX Optio W30") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ORIENTATION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"top, left",
	strlen ("top, left") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATION_DATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2008:06:29 16:06:10",
	strlen ("2008:06:29 16:06:10") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_EXPOSURE_BIAS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"0 EV",
	strlen ("0 EV") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FLASH,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"No, compulsory",
	strlen ("No, compulsory") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FOCAL_LENGTH,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"18.9 mm",
	strlen ("18.9 mm") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_FOCAL_LENGTH_35MM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"114.0 mm",
	strlen ("114.0 mm") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_ISO_SPEED,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"64",
	strlen ("64") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_METERING_MODE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Multi-segment",
	strlen ("Multi-segment") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_APERTURE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"F8",
	strlen ("F8") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_EXPOSURE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"1/320 s",
	strlen ("1/320 s") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LOCATION_CITY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Los Verdes",
	strlen ("Los Verdes") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LOCATION_CITY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Los Verdes",
	strlen ("Los Verdes") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LOCATION_SUBLOCATION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Fuerteventura",
	strlen ("Fuerteventura") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LOCATION_COUNTRY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Spain",
	strlen ("Spain") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LOCATION_COUNTRY,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Spain",
	strlen ("Spain") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Fuerteventura",
	strlen ("Fuerteventura") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Landschaftsbild",
	strlen ("Landschaftsbild") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"ProCenter Rene Egli",
	strlen ("ProCenter Rene Egli") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Sand",
	strlen ("Sand") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Sport",
	strlen ("Sport") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Urlaub",
	strlen ("Urlaub") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Was?",
	strlen ("Was?") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Wind",
	strlen ("Wind") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Windsurfen",
	strlen ("Windsurfen") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_KEYWORDS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Wo?",
	strlen ("Wo?") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_RATING,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"3",
	strlen ("3") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_RATING,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"50",
	strlen ("50") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_LOCATION_COUNTRY_CODE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"ES",
	strlen ("ES") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Optio W30 Ver 1.00",
	strlen ("Optio W30 Ver 1.00") + 1,
	0 
      },
      { 
	EXTRACTOR_METATYPE_SUBJECT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Wo?, Wo?|Fuerteventura, Was?, Was?|Anlass]|Urlaub, Was?|Aufnahme]|Landschaftsbild, Was?|Natur]|Wind, Was?|Natur]|Sand, Wo?|Fuerteventura|ProCenter Rene Egli, Was?|Sport, Was?|Sport|Windsurfen",
	strlen ("Wo?, Wo?|Fuerteventura, Was?, Was?|Anlass]|Urlaub, Was?|Aufnahme]|Landschaftsbild, Was?|Natur]|Wind, Was?|Natur]|Sand, Wo?|Fuerteventura|ProCenter Rene Egli, Was?|Sport, Was?|Sport|Windsurfen") + 1,
	0 
      },
      { 0, 0, NULL, NULL, 0, -1 }
    };
  struct ProblemSet ps[] =
    {
      { "testdata/exiv2_iptc.jpg",
	exiv2_iptc_sol },
      { NULL, NULL }
    };
  return ET_main ("exiv2", ps);
}

/* end of test_exiv2.c */
