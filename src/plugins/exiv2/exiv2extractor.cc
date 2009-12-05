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
  @file    exiv2extractor.cc
  @brief   Prototype libextractor plugin for Exif using exiv2
  @version $Rev$
  @author  Andreas Huggel (ahu)
  <a href="mailto:ahuggel@gmx.net">ahuggel@gmx.net</a>
  @date    30-Jun-05, ahu: created
*/

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <math.h>

#include "platform.h"
#include "extractor.h"

#include "exiv2/exif.hpp"
#include "exiv2/image.hpp"
#include "exiv2/futils.hpp"

#define WORKAROUND_905 1
#if WORKAROUND_905
#include <pthread.h>
#endif

#define SUPPRESS_WARNINGS 1

extern "C" {

  static struct EXTRACTOR_Keywords * 
  addKeyword(EXTRACTOR_KeywordType type,
	     char * keyword,
	     struct EXTRACTOR_Keywords * next)
  {
    EXTRACTOR_KeywordList * result;
    
    if (keyword == NULL)
      return next;
    result = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
    result->next = next;
    result->keyword = keyword;
    result->keywordType = type;
    return result;
  }
  
}


static struct EXTRACTOR_Keywords * 
addExiv2Tag(const Exiv2::ExifData& exifData,
	    const std::string& key,
	    EXTRACTOR_KeywordType type,
	    struct EXTRACTOR_Keywords * result)
{
    const char * str;
	
    Exiv2::ExifKey ek(key);
    Exiv2::ExifData::const_iterator md = exifData.findKey(ek);
    if (md != exifData.end()) {
	std::string ccstr = Exiv2::toString(*md);
	str = ccstr.c_str();
        while ( (strlen(str) > 0) && isspace(str[0])) str++;
	if (strlen(str) > 0)
        result = addKeyword(type,
                            strdup(str),
                            result);
	md++;
    }
    return result;
}



static struct EXTRACTOR_Keywords * 
addIptcData(const Exiv2::IptcData& iptcData,
	    const std::string& key,
	    EXTRACTOR_KeywordType type,
	    struct EXTRACTOR_Keywords * result)
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
        while ( (strlen(str) > 0) && isspace(str[0])) str++;
	if (strlen(str) > 0)
        result = addKeyword(type,
                            strdup(str),
                            result);
	md++;
    }
    return result;
}




static struct EXTRACTOR_Keywords * 
addXmpData(const Exiv2::XmpData& xmpData,
	    const std::string& key,
	    EXTRACTOR_KeywordType type,
	    struct EXTRACTOR_Keywords * result)
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
        while ( (strlen(str) > 0) && isspace(str[0])) str++;
	if (strlen(str) > 0)
        result = addKeyword(type,
                            strdup(str),
                            result);
	md++;
    }
    return result;
}







extern "C" {
  
#if WORKAROUND_905
  static struct EXTRACTOR_Keywords * extract(const char * filename,
					     unsigned char * data,
					     size_t size,
					     struct EXTRACTOR_Keywords * prev)
#else
  struct EXTRACTOR_Keywords * libextractor_exiv2_extract(const char * filename,
                                                         unsigned char * data,
                                                         size_t size,
                                                         struct EXTRACTOR_Keywords * prev)
#endif
  {
    struct EXTRACTOR_Keywords * result = prev;
    struct EXTRACTOR_Keywords * newResult;
    try 
      {	    
	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(data, size);
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exifData = image->exifData();
	if (!exifData.empty()) 
	  {	   		
	    // Camera make
	    result = addExiv2Tag(exifData,
				 "Exif.Image.Make",
				 EXTRACTOR_CAMERA_MAKE,
				 result);
	    
	    // Camera model
	    result = addExiv2Tag(exifData,
				 "Exif.Image.Model",
				 EXTRACTOR_CAMERA_MODEL,
				 result);

	    // Camera model
	    result = addExiv2Tag(exifData,
				 "Exif.Image.Orientation",
				 EXTRACTOR_ORIENTATION,
				 result);
	    
	    // Image Timestamp
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.DateTimeOriginal",
				 EXTRACTOR_DATE,
				 result);
	    
	    // Exposure time
	    // From ExposureTime, failing that, try ShutterSpeedValue
	    newResult = addExiv2Tag(exifData,
				    "Exif.Photo.ExposureTime",
				    EXTRACTOR_EXPOSURE,
				    result);
	    Exiv2::ExifData::const_iterator md;
	    if (newResult == result) {
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
                newResult = addKeyword(EXTRACTOR_EXPOSURE,
                                       strdup(os.str().c_str()),
                                       result);
	      }
	    }
	    result = newResult;
	    
	    // Aperture
	    // Get if from FNumber and, failing that, try ApertureValue
	    newResult = addExiv2Tag(exifData,
				    "Exif.Photo.FNumber",
				    EXTRACTOR_APERTURE,
				    result);
	    if (newResult == result) {
	      md = exifData.findKey(Exiv2::ExifKey("Exif.Photo.ApertureValue"));
	      if (md != exifData.end()) {
                std::ostringstream os;
                os << std::fixed << std::setprecision(1)
                   << "F" << exp(log(2.0) * md->toFloat() / 2);
                newResult = addKeyword(EXTRACTOR_APERTURE,
                                       strdup(os.str().c_str()),
                                       result);
	      }
	    }
	    result = newResult;
	    
	    // Exposure bias
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.ExposureBiasValue",
				 EXTRACTOR_EXPOSURE_BIAS,
				 result);
	    
	    // Flash
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.Flash",
				 EXTRACTOR_FLASH,
				 result);
	    
	    // Flash bias
	    // Todo: Implement this for other cameras
	    newResult = addExiv2Tag(exifData,
				    "Exif.CanonSi.FlashBias",
				    EXTRACTOR_FLASH_BIAS,
				    result);
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Panasonic.FlashBias",
				      EXTRACTOR_FLASH_BIAS,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Olympus.FlashBias",
				      EXTRACTOR_FLASH_BIAS,
				      result);
	    }
	    result = newResult;
	    
	    // Actual focal length and 35 mm equivalent
	    // Todo: Calculate 35 mm equivalent a la jhead
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.FocalLength",
				 EXTRACTOR_FOCAL_LENGTH,
				 result);
	    
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.FocalLengthIn35mmFilm",
				 EXTRACTOR_FOCAL_LENGTH_35MM,
				 result);
	    
	    // ISO speed
	    // from ISOSpeedRatings or the Makernote
	    newResult = addExiv2Tag(exifData,
				    "Exif.Photo.ISOSpeedRatings",
				    EXTRACTOR_ISO_SPEED,
				    result);
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.CanonSi.ISOSpeed",
				      EXTRACTOR_ISO_SPEED,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon1.ISOSpeed",
				      EXTRACTOR_ISO_SPEED,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon2.ISOSpeed",
				      EXTRACTOR_ISO_SPEED,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon3.ISOSpeed",
				      EXTRACTOR_ISO_SPEED,
				      result);
	    }
	    result = newResult;
	    
	    // Exposure mode
	    // From ExposureProgram or Canon Makernote
	    newResult = addExiv2Tag(exifData,
				    "Exif.Photo.ExposureProgram",
				    EXTRACTOR_EXPOSURE_MODE,
				    result);
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.CanonCs.ExposureProgram",
				      EXTRACTOR_EXPOSURE_MODE,
				      result);
	    }
	    result = newResult;
	    
	    // Metering mode
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.MeteringMode",
				 EXTRACTOR_METERING_MODE,
				 result);
	    
	    // Macro mode
	    // Todo: Implement this for other cameras
	    newResult = addExiv2Tag(exifData,
				    "Exif.CanonCs.Macro",
				    EXTRACTOR_MACRO_MODE,
				    result);
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Fujifilm.Macro",
				      EXTRACTOR_MACRO_MODE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Olympus.Macro",
				      EXTRACTOR_MACRO_MODE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Panasonic.Macro",
				      EXTRACTOR_MACRO_MODE,
				      result);
	    }
	    result = newResult;

	    // Image quality setting (compression)
	    // Todo: Implement this for other cameras
	    newResult = addExiv2Tag(exifData,
				    "Exif.CanonCs.Quality",
				    EXTRACTOR_IMAGE_QUALITY,
				    result);
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Fujifilm.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Sigma.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon1.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon2.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon3.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Olympus.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Panasonic.Quality",
				      EXTRACTOR_IMAGE_QUALITY,
				      result);
	    }
	    result = newResult;
	    

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
	    if (xdim != 0 && ydim != 0) {
	      std::ostringstream os;
	      os << xdim << "x" << ydim;
	      result = addKeyword(EXTRACTOR_SIZE,
				  strdup(os.str().c_str()),
				  result);
	    }
	    // White balance
	    // Todo: Implement this for other cameras
	    
	    newResult = addExiv2Tag(exifData,
				    "Exif.CanonSi.WhiteBalance",
				    EXTRACTOR_WHITE_BALANCE,
				    result);
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Fujifilm.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Sigma.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon1.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon2.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Nikon3.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Olympus.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    if (newResult == result) {
	      newResult = addExiv2Tag(exifData,
				      "Exif.Panasonic.WhiteBalance",
				      EXTRACTOR_WHITE_BALANCE,
				      result);
	    }
	    result = newResult;
	    
	    // Copyright
	    result = addExiv2Tag(exifData,
				 "Exif.Image.Copyright",
				 EXTRACTOR_COPYRIGHT,
				 result);
	    
	    // Exif Comment
	    result = addExiv2Tag(exifData,
				 "Exif.Photo.UserComment",
				 EXTRACTOR_COMMENT,
				 result);
	    // GPS
	    result = addExiv2Tag(exifData,
				 "Exif.GPSInfo.GPSLatitudeRef",
				 EXTRACTOR_GPS_LATITUDE_REF,
				 result);
	    result = addExiv2Tag(exifData,
				 "Exif.GPSInfo.GPSLatitude",
				 EXTRACTOR_GPS_LATITUDE,
				 result);
	    result = addExiv2Tag(exifData,
				 "Exif.GPSInfo.GPSLongitudeRef",
				 EXTRACTOR_GPS_LONGITUDE_REF,
				 result);
	    result = addExiv2Tag(exifData,
				 "Exif.GPSInfo.GPSLongitude",
				 EXTRACTOR_GPS_LONGITUDE,
				 result);
	  }

	Exiv2::IptcData &iptcData = image->iptcData();
	Exiv2::XmpData &xmpData = image->xmpData();

	if (! iptcData.empty()) {
	  result = addIptcData (iptcData,
				"Iptc.Application2.Keywords",
				EXTRACTOR_KEYWORDS,
				result);
	  newResult = addIptcData (iptcData,
				   "Iptc.Application2.City",
				   EXTRACTOR_LOCATION_CITY,
				   result);
	  if ((result == newResult) && (!xmpData.empty()))
	    result = addXmpData (xmpData,
				 "Xmp.photoshop.City",
				 EXTRACTOR_RATING,
				 result);
	  else
	    result = newResult;

	  result = addIptcData (iptcData,
				"Iptc.Application2.SubLocation",
				EXTRACTOR_LOCATION_SUBLOCATION,
				result);
	  newResult = addIptcData (iptcData,
				   "Iptc.Application2.CountryName",
				   EXTRACTOR_LOCATION_COUNTRY,
				   result);
	  if ( (result == newResult) && (!xmpData.empty())) 
	    result = addXmpData (xmpData,
				 "Xmp.photoshop.Country",
				 EXTRACTOR_RATING,
				 result);
	  else
	    result = newResult;
	  
	}

	if (! xmpData.empty()) {
	  newResult = addXmpData (xmpData,
				  "Xmp.xmp.Rating",
				  EXTRACTOR_RATING,
				  result);
	  if (result == newResult)
	    result = addXmpData (xmpData,
				 "Xmp.MicrosoftPhoto.Rating",
				 EXTRACTOR_RATING,
				 result);
	  else
	    result = newResult;
	  result = addXmpData (xmpData,
			       "Xmp.iptc.CountryCode",
			       EXTRACTOR_COUNTRY_CODE,
			       result);
	  result = addXmpData (xmpData,
			       "Xmp.xmp.CreatorTool",
			       EXTRACTOR_CREATED_BY_SOFTWARE,
			       result);
	  result = addXmpData (xmpData,
			       "Xmp.lr.hierarchicalSubject",
			       EXTRACTOR_SUBJECT,
			       result);
	}	
      }
    catch (const Exiv2::AnyError& e) {
#ifndef SUPPRESS_WARNINGS
      std::cout << "Caught Exiv2 exception '" << e << "'\n";
#endif
    }
    
    return result;
  }


#if WORKAROUND_905
  struct X {
    unsigned char * data;
    size_t size;
    struct EXTRACTOR_Keywords * prev;
  };


  static void * run(void * arg) {
    struct X * x = (struct X*) arg;
    return extract(NULL, x->data, x->size, x->prev);
  }

    struct EXTRACTOR_Keywords * libextractor_exiv2_extract(const char * filename,
                                                           unsigned char * data,
                                                           size_t size,
                                                           struct EXTRACTOR_Keywords * prev) {
      pthread_t pt;
      struct X cls;
      void * ret;
      cls.data = data;
      cls.size = size;
      cls.prev = prev;
      if (0 == pthread_create(&pt, NULL, &run, &cls))
	if (0 == pthread_join(pt, &ret))
	return (struct EXTRACTOR_Keywords*) ret;
      return prev;
    }

#endif
}
