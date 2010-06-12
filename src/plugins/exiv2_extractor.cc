// ***************************************************************** -*- C++ -*-
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
/*!
  @file    exiv2_extractor.cc
  @brief   libextractor plugin for Exif using exiv2
  @version $Rev$
  @author  Andreas Huggel (ahu)
  <a href="mailto:ahuggel@gmx.net">ahuggel@gmx.net</a>
  @date    30-Jun-05, ahu: created; 15-Dec-09, cg: updated
*/

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <math.h>

#include "platform.h"
#include "extractor.h"

#include <exiv2/exif.hpp>
#include <exiv2/image.hpp>
#include <exiv2/futils.hpp>

#define SUPPRESS_WARNINGS 1

#define ADD(s, type) do { if (0!=proc(proc_cls, "exiv2", type, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)

static int
addExiv2Tag(const Exiv2::ExifData& exifData,
	    const std::string& key,
	    enum EXTRACTOR_MetaType type,
	    EXTRACTOR_MetaDataProcessor proc,
	    void *proc_cls)
{
  const char * str;
  
  Exiv2::ExifKey ek(key);
  Exiv2::ExifData::const_iterator md = exifData.findKey(ek);
  if (md != exifData.end()) {
    std::string ccstr = Exiv2::toString(*md);
    str = ccstr.c_str();
    while ( (strlen(str) > 0) && isspace((unsigned char) str[0])) str++;
    if (strlen(str) > 0)
      ADD (str, type);
    md++;
  }
  return 0;
}


static int
addIptcData(const Exiv2::IptcData& iptcData,
	    const std::string& key,
	    enum EXTRACTOR_MetaType type,
	    EXTRACTOR_MetaDataProcessor proc,
	    void *proc_cls)
{
  const char * str;
  
  Exiv2::IptcKey ek(key);
  Exiv2::IptcData::const_iterator md = iptcData.findKey(ek);
  while (md != iptcData.end()) 
    {
      if (0 != strcmp (Exiv2::toString(md->key()).c_str(), key.c_str()))
	  break;
      std::string ccstr = Exiv2::toString(*md);
      str = ccstr.c_str();
      while ( (strlen(str) > 0) && isspace( (unsigned char) str[0])) str++;
      if (strlen(str) > 0)
	ADD (str, type);
      md++;
    }
  return 0;
}


static int
addXmpData(const Exiv2::XmpData& xmpData,
	   const std::string& key,
	   enum EXTRACTOR_MetaType type,
	   EXTRACTOR_MetaDataProcessor proc,
	   void *proc_cls)
{
  const char * str;
  
  Exiv2::XmpKey ek(key);
  Exiv2::XmpData::const_iterator md = xmpData.findKey(ek);
  while (md != xmpData.end()) 
    {
      if (0 != strcmp (Exiv2::toString(md->key()).c_str(), key.c_str()))
	break;
      std::string ccstr = Exiv2::toString(*md);
      str = ccstr.c_str();
      while ( (strlen(str) > 0) && isspace( (unsigned char) str[0])) str++;
      if (strlen(str) > 0)
	ADD (str, type);
      md++;
    }
  return 0;
}

#define ADDEXIV(s,t) do { if (0 != addExiv2Tag (exifData, s, t, proc, proc_cls)) return 1; } while (0)
#define ADDIPTC(s,t) do { if (0 != addIptcData (iptcData, s, t, proc, proc_cls)) return 1; } while (0)
#define ADDXMP(s,t)  do { if (0 != addXmpData  (xmpData,  s, t, proc, proc_cls)) return 1; } while (0)


extern "C" {
  
  int 
  EXTRACTOR_exiv2_extract (const char *data,
			   size_t size,
			   EXTRACTOR_MetaDataProcessor proc,
			   void *proc_cls,
			   const char *options)
  {
    try 
      {	    
	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open( (Exiv2::byte*) data,
								 size);
	if (image.get() == 0)
	  return 0;
	image->readMetadata();
	Exiv2::ExifData &exifData = image->exifData();
	if (!exifData.empty()) 
	  {	   		
	    Exiv2::ExifData::const_iterator md;
	    
	    /* FIXME: this should be a loop over data,
	       not a looooong block of code */
	    ADDEXIV ("Exif.Image.Copyright", EXTRACTOR_METATYPE_COPYRIGHT);
	    ADDEXIV ("Exif.Photo.UserComment", EXTRACTOR_METATYPE_COMMENT);
	    ADDEXIV ("Exif.GPSInfo.GPSLatitudeRef", EXTRACTOR_METATYPE_GPS_LATITUDE_REF);
	    ADDEXIV ("Exif.GPSInfo.GPSLatitude", EXTRACTOR_METATYPE_GPS_LATITUDE);
	    ADDEXIV ("Exif.GPSInfo.GPSLongitudeRef", EXTRACTOR_METATYPE_GPS_LONGITUDE_REF);
	    ADDEXIV ("Exif.GPSInfo.GPSLongitude", EXTRACTOR_METATYPE_GPS_LONGITUDE);
	    ADDEXIV ("Exif.Image.Make", EXTRACTOR_METATYPE_CAMERA_MAKE);    
	    ADDEXIV ("Exif.Image.Model", EXTRACTOR_METATYPE_CAMERA_MODEL);
	    ADDEXIV ("Exif.Image.Orientation", EXTRACTOR_METATYPE_ORIENTATION);
	    ADDEXIV ("Exif.Photo.DateTimeOriginal", EXTRACTOR_METATYPE_CREATION_DATE);
	    ADDEXIV ("Exif.Photo.ExposureBiasValue", EXTRACTOR_METATYPE_EXPOSURE_BIAS);
	    ADDEXIV ("Exif.Photo.Flash", EXTRACTOR_METATYPE_FLASH);
	    ADDEXIV ("Exif.CanonSi.FlashBias", EXTRACTOR_METATYPE_FLASH_BIAS);
	    ADDEXIV ("Exif.Panasonic.FlashBias", EXTRACTOR_METATYPE_FLASH_BIAS);
	    ADDEXIV ("Exif.Olympus.FlashBias", EXTRACTOR_METATYPE_FLASH_BIAS);
	    ADDEXIV ("Exif.Photo.FocalLength", EXTRACTOR_METATYPE_FOCAL_LENGTH);
	    ADDEXIV ("Exif.Photo.FocalLengthIn35mmFilm", EXTRACTOR_METATYPE_FOCAL_LENGTH_35MM);
	    ADDEXIV ("Exif.Photo.ISOSpeedRatings", EXTRACTOR_METATYPE_ISO_SPEED);
	    ADDEXIV ("Exif.CanonSi.ISOSpeed", EXTRACTOR_METATYPE_ISO_SPEED);
	    ADDEXIV ("Exif.Nikon1.ISOSpeed", EXTRACTOR_METATYPE_ISO_SPEED);
	    ADDEXIV ("Exif.Nikon2.ISOSpeed", EXTRACTOR_METATYPE_ISO_SPEED);
	    ADDEXIV ("Exif.Nikon3.ISOSpeed", EXTRACTOR_METATYPE_ISO_SPEED);
	    ADDEXIV ("Exif.Photo.ExposureProgram", EXTRACTOR_METATYPE_EXPOSURE_MODE);
	    ADDEXIV ("Exif.CanonCs.ExposureProgram", EXTRACTOR_METATYPE_EXPOSURE_MODE);
	    ADDEXIV ("Exif.Photo.MeteringMode", EXTRACTOR_METATYPE_METERING_MODE);
	    ADDEXIV ("Exif.CanonCs.Macro", EXTRACTOR_METATYPE_MACRO_MODE);
	    ADDEXIV ("Exif.Fujifilm.Macro", EXTRACTOR_METATYPE_MACRO_MODE);
	    ADDEXIV ("Exif.Olympus.Macro", EXTRACTOR_METATYPE_MACRO_MODE);
	    ADDEXIV ("Exif.Panasonic.Macro", EXTRACTOR_METATYPE_MACRO_MODE);
	    ADDEXIV ("Exif.CanonCs.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Fujifilm.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Sigma.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Nikon1.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Nikon2.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Nikon3.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Olympus.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.Panasonic.Quality", EXTRACTOR_METATYPE_IMAGE_QUALITY);
	    ADDEXIV ("Exif.CanonSi.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Fujifilm.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Sigma.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Nikon1.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Nikon2.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Nikon3.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Olympus.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);
	    ADDEXIV ("Exif.Panasonic.WhiteBalance", EXTRACTOR_METATYPE_WHITE_BALANCE);

	    ADDEXIV ("Exif.Photo.FNumber", EXTRACTOR_METATYPE_APERTURE);
	    md = exifData.findKey(Exiv2::ExifKey("Exif.Photo.ApertureValue"));
	    if (md != exifData.end()) {
	      std::ostringstream os;
	      os << std::fixed << std::setprecision(1)
		 << "F" << exp(log(2.0) * md->toFloat() / 2);
	      ADD (os.str().c_str(), EXTRACTOR_METATYPE_APERTURE);
	    }
    
	    ADDEXIV ("Exif.Photo.ExposureTime", EXTRACTOR_METATYPE_EXPOSURE);
	    md = exifData.findKey(Exiv2::ExifKey("Exif.Photo.ShutterSpeedValue"));
	    if (md != exifData.end()) {
	      double tmp = exp(log(2.0) * md->toFloat()) + 0.5;
	      std::ostringstream os;
	      if (tmp > 1) {
		os << "1/" << static_cast<long>(tmp) << " s";
	      }
	      else {
		os << static_cast<long>(1/tmp) << " s";
	      }
	      ADD (os.str().c_str(), EXTRACTOR_METATYPE_EXPOSURE);
	    }

	    /* this can sometimes be wrong (corrupt exiv2 data?).
	       Either way, we should get the data directly from
	       the specific file format parser (i.e. jpeg, tiff). */
	    // Exif Resolution
	    unsigned long xdim = 0;
	    unsigned long ydim = 0;
	    md = exifData.findKey(Exiv2::ExifKey("Exif.Photo.PixelXDimension"));
	    if (md != exifData.end()) xdim = md->toLong();
	    md = exifData.findKey(Exiv2::ExifKey("Exif.Photo.PixelYDimension"));
	    if (md != exifData.end()) ydim = md->toLong();
	    if ( (xdim != 0) && (ydim != 0))
	      {
		std::ostringstream os;
		os << xdim << "x" << ydim;
		ADD (os.str().c_str(), EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
	      }	    
	  } 

	Exiv2::IptcData &iptcData = image->iptcData();
	if (! iptcData.empty()) {
	  ADDIPTC ("Iptc.Application2.Keywords", EXTRACTOR_METATYPE_KEYWORDS);
	  ADDIPTC ("Iptc.Application2.City", EXTRACTOR_METATYPE_LOCATION_CITY);
	  ADDIPTC ("Iptc.Application2.SubLocation", EXTRACTOR_METATYPE_LOCATION_SUBLOCATION);
	  ADDIPTC ("Iptc.Application2.CountryName", EXTRACTOR_METATYPE_LOCATION_COUNTRY);
	  ADDIPTC ("Xmp.photoshop.Country", EXTRACTOR_METATYPE_RATING);
	}

	Exiv2::XmpData &xmpData = image->xmpData();
	if (! xmpData.empty()) {
	  ADDXMP ("Xmp.photoshop.City", EXTRACTOR_METATYPE_LOCATION_CITY);
	  ADDXMP ("Xmp.xmp.Rating", EXTRACTOR_METATYPE_RATING);
	  ADDXMP ("Xmp.MicrosoftPhoto.Rating", EXTRACTOR_METATYPE_RATING);
	  ADDXMP ("Xmp.iptc.CountryCode", EXTRACTOR_METATYPE_LOCATION_COUNTRY_CODE);
	  ADDXMP ("Xmp.xmp.CreatorTool", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);
	  ADDXMP ("Xmp.lr.hierarchicalSubject", EXTRACTOR_METATYPE_SUBJECT);
	}	
      }
    catch (const Exiv2::AnyError& e) {
#ifndef SUPPRESS_WARNINGS
      std::cout << "Caught Exiv2 exception '" << e << "'\n";
#endif
    }
    
    return 0;
  }


}
