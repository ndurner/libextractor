// ***************************************************************** -*- C++ -*-
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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
/**
 * @file plugins/exiv2_extractor.cc
 * @brief libextractor plugin for Exif using exiv2
 * @author Andreas Huggel (ahu)
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <math.h>
#include <exiv2/exif.hpp>
#include <exiv2/error.hpp>
#include <exiv2/image.hpp>
#include <exiv2/futils.hpp>

/**
 * Enable debugging to get error messages.
 */
#define DEBUG 0


/**
 * Implementation of EXIV2's BasicIO interface based
 * on the 'struct EXTRACTOR_ExtractContext.
 */
class ExtractorIO : public Exiv2::BasicIo
{
private:

  /**
   * Extract context we are using.
   */
  struct EXTRACTOR_ExtractContext *ec;

public:

  /**
   * Constructor.
   * 
   * @param s_ec extract context to wrap
   */
  ExtractorIO (struct EXTRACTOR_ExtractContext *s_ec)
  {
    ec = s_ec;
  }

  /**
   * Destructor.
   */
  virtual ~ExtractorIO ()
  {
    /* nothing to do */
  }

  /**
   * Open stream.
   * 
   * @return 0 (always successful)
   */
  virtual int open ();

  /**
   * Close stream.
   * 
   * @return 0 (always successful)
   */
  virtual int close ();
  
  /**
   * Read up to 'rcount' bytes into a buffer
   *
   * @param rcount number of bytes to read
   * @return buffer with data read, empty buffer (!) on failure (!)
   */
  virtual Exiv2::DataBuf read (long rcount);

  /**
   * Read up to 'rcount' bytes into 'buf'.
   *
   * @param buf buffer to fill
   * @param rcount size of 'buf'
   * @return number of bytes read successfully, 0 on failure (!)
   */
  virtual long read (Exiv2::byte *buf,
		     long rcount);

  /**
   * Read a single character.
   *
   * @return the character
   * @throw exception on errors
   */
  virtual int getb ();

  /**
   * Write to stream.
   *
   * @param data data to write
   * @param wcount how many bytes to write
   * @return -1 (always fails)
   */ 
  virtual long write (const Exiv2::byte *data,
		      long wcount);

  /**
   * Write to stream.
   *
   * @param src stream to copy
   * @return -1 (always fails)
   */ 
  virtual long write (Exiv2::BasicIo &src);

  /**
   * Write a single byte.
   *
   * @param data byte to write
   * @return -1 (always fails)
   */
  virtual int putb (Exiv2::byte data);

  /**
   * Not supported.
   *
   * @throws error
   */
  virtual void transfer (Exiv2::BasicIo& src);

  /**
   * Seek to the given offset.
   *
   * @param offset desired offset
   * @parma pos offset is relative to where?
   * @return -1 on failure, 0 on success
   */
  virtual int seek (long offset,
		    Exiv2::BasicIo::Position pos);

  /**
   * Not supported.
   *
   * @throws error
   */
  virtual Exiv2::byte* mmap (bool isWritable);
  
  /**
   * Not supported.
   *
   * @return -1 (error)
   */
  virtual int munmap ();

  /**
   * Return our current offset in the file.
   *
   * @return -1 on error
   */
  virtual long int tell (void) const;

  /**
   * Return overall size of the file.
   *
   * @return -1 on error
   */
  virtual long int size (void) const;

  /**
   * Check if file is open.
   * 
   * @return true (always).
   */
  virtual bool isopen () const;

  /**
   * Check if this file source is in error mode.
   *
   * @return 0 (always all is fine).
   */
  virtual int error () const;

  /**
   * Check if current position of the file is at the end
   * 
   * @return true if at EOF, false if not.
   */
  virtual bool eof () const;

  /**
   * Not supported.
   *
   * @throws error
   */
  virtual std::string path () const;

#ifdef EXV_UNICODE_PATH
  /**
   * Not supported.
   *
   * @throws error
   */
  virtual std::wstring wpath () const;
#endif
  
  /**
   * Not supported.
   *
   * @throws error
   */
  virtual Exiv2::BasicIo::AutoPtr temporary () const;

};
  

/**
 * Open stream.
 * 
 * @return 0 (always successful)
 */
int 
ExtractorIO::open ()
{
  return 0;
}

/**
 * Close stream.
 * 
 * @return 0 (always successful)
 */
int 
ExtractorIO::close ()
{
  return 0;
}


/**
 * Read up to 'rcount' bytes into a buffer
 *
 * @param rcount number of bytes to read
 * @return buffer with data read, empty buffer (!) on failure (!)
 */
Exiv2::DataBuf
ExtractorIO::read (long rcount)
{
  void *data;
  ssize_t ret;

  if (-1 == (ret = ec->read (ec->cls, &data, rcount)))
    return Exiv2::DataBuf (NULL, 0);
  return Exiv2::DataBuf ((const Exiv2::byte *) data, ret);
}


/**
 * Read up to 'rcount' bytes into 'buf'.
 *
 * @param buf buffer to fill
 * @param rcount size of 'buf'
 * @return number of bytes read successfully, 0 on failure (!)
 */
long 
ExtractorIO::read (Exiv2::byte *buf,
		   long rcount)
{
  void *data;
  ssize_t ret;
  long got;

  got = 0;
  while (got < rcount)
    {
      if (-1 == (ret = ec->read (ec->cls, &data, rcount - got)))
	return got;
      if (0 == ret)
	break;
      memcpy (&buf[got], data, ret);
      got += ret;
    }
  return got;
}


/**
 * Read a single character.
 *
 * @return the character
 * @throw exception on errors
 */
int 
ExtractorIO::getb ()
{
  void *data;
  const unsigned char *r;
  
  if (1 != ec->read (ec->cls, &data, 1))
    throw Exiv2::BasicError<char> (42 /* error code */);
  r = (const unsigned char *) data;
  return *r;
}


/**
 * Write to stream.
 *
 * @param data data to write
 * @param wcount how many bytes to write
 * @return -1 (always fails)
 */ 
long 
ExtractorIO::write (const Exiv2::byte *data,
		    long wcount)
{
  return -1;
}


/**
 * Write to stream.
 *
 * @param src stream to copy
 * @return -1 (always fails)
 */ 
long 
ExtractorIO::write (Exiv2::BasicIo &src)
{
  return -1;
}


/**
 * Write a single byte.
 *
 * @param data byte to write
 * @return -1 (always fails)
 */
int 
ExtractorIO::putb (Exiv2::byte data)
{
  return -1;
}


/**
 * Not supported.
 *
 * @throws error
 */
void
ExtractorIO::transfer (Exiv2::BasicIo& src)
{
  throw Exiv2::BasicError<char> (42 /* error code */);
}


/**
 * Seek to the given offset.
 *
 * @param offset desired offset
 * @parma pos offset is relative to where?
 * @return -1 on failure, 0 on success
 */
int 
ExtractorIO::seek (long offset,
		   Exiv2::BasicIo::Position pos)
{
  int rel;
  
  switch (pos)
    {
    case beg: // Exiv2::BasicIo::beg:
      rel = SEEK_SET;
      break;
    case cur:
      rel = SEEK_CUR;
      break;
    case end:
      rel = SEEK_END;
      break;
    default:
      abort ();
    }
  if (-1 == ec->seek (ec->cls, offset, rel))
    return -1;
  return 0;
}


/**
 * Not supported.
 *
 * @throws error
 */
Exiv2::byte *
ExtractorIO::mmap (bool isWritable)
{
  throw Exiv2::BasicError<char> (42 /* error code */);
}


/**
 * Not supported.
 *
 * @return -1 error
 */
int
ExtractorIO::munmap ()
{
  return -1;
}


/**
 * Return our current offset in the file.
 *
 * @return -1 on error
 */
long int
ExtractorIO::tell (void) const
{
  return (long) ec->seek (ec->cls, 0, SEEK_CUR);
}


/**
 * Return overall size of the file.
 *
 * @return -1 on error
 */
long int 
ExtractorIO::size (void) const
{
  return (long) ec->get_size (ec->cls);
}


/**
 * Check if file is open.
 * 
 * @return true (always).
 */
bool 
ExtractorIO::isopen () const
{
  return true;
}


/**
 * Check if this file source is in error mode.
 *
 * @return 0 (always all is fine).
 */
int
ExtractorIO::error () const
{
  return 0;
}


/**
 * Check if current position of the file is at the end
 * 
 * @return true if at EOF, false if not.
 */
bool 
ExtractorIO::eof () const
{
  return size () == tell ();
}


/**
 * Not supported.
 *
 * @throws error
 */
std::string
ExtractorIO::path () const
{
  throw Exiv2::BasicError<char> (42 /* error code */);
}


#ifdef EXV_UNICODE_PATH
/**
 * Not supported.
 *
 * @throws error
 */
std::wstring
ExtractorIO::wpath () const
{
  throw Exiv2::BasicError<char> (42 /* error code */);
}
#endif


/**
 * Not supported.
 *
 * @throws error
 */
Exiv2::BasicIo::AutoPtr
ExtractorIO::temporary () const
{
  fprintf (stderr, "throwing temporary error\n");
  throw Exiv2::BasicError<char> (42 /* error code */);
}


/**
 * Pass the given UTF-8 string to the 'proc' callback using
 * the given type.  Uses 'return 1' if 'proc' returns non-0.
 *
 * @param s 0-terminated UTF8 string value with the meta data
 * @param type libextractor type for the meta data
 */
#define ADD(s, type) do { if (0 != proc (proc_cls, "exiv2", type, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return 1; } while (0)


/**
 * Try to find a given key in the exifData and if a value is
 * found, pass it to 'proc'.
 *
 * @param exifData metadata set to inspect
 * @param key key to lookup in exifData
 * @param type extractor type to use
 * @param proc function to call with results
 * @param proc_cls closurer for proc
 * @return 0 to continue extracting, 1 to abort
 */
static int
add_exiv2_tag (const Exiv2::ExifData& exifData,
	       const std::string& key,
	       enum EXTRACTOR_MetaType type,
	       EXTRACTOR_MetaDataProcessor proc,
	       void *proc_cls)
{
  const char *str;
  Exiv2::ExifKey ek (key);
  Exiv2::ExifData::const_iterator md = exifData.findKey (ek);

  if (exifData.end () == md) 
    return 0; /* not found */
  std::string ccstr = Exiv2::toString(*md);
  str = ccstr.c_str();
  /* skip over whitespace */
  while ( (strlen (str) > 0) && isspace ((unsigned char) str[0]))
    str++;
  if (strlen (str) > 0)
    ADD (str, type);
  md++;
  return 0;
}


/**
 * Try to find a given key in the iptcData and if a value is
 * found, pass it to 'proc'.
 *
 * @param ipctData metadata set to inspect
 * @param key key to lookup in exifData
 * @param type extractor type to use
 * @param proc function to call with results
 * @param proc_cls closurer for proc
 * @return 0 to continue extracting, 1 to abort
 */
static int
add_iptc_data (const Exiv2::IptcData& iptcData,
	       const std::string& key,
	       enum EXTRACTOR_MetaType type,
	       EXTRACTOR_MetaDataProcessor proc,
	       void *proc_cls)
{
  const char *str;
  Exiv2::IptcKey ek (key);
  Exiv2::IptcData::const_iterator md = iptcData.findKey (ek);

  while (iptcData.end () !=  md) 
    {
      if (0 != strcmp (Exiv2::toString (md->key ()).c_str (), key.c_str ()))
	  break;
      std::string ccstr = Exiv2::toString (*md);
      str = ccstr.c_str ();
      /* skip over whitespace */
      while ((strlen (str) > 0) && isspace ((unsigned char) str[0])) 
	str++;
      if (strlen (str) > 0)
	ADD (str, type);
      md++;
    }
  return 0;
}


/**
 * Try to find a given key in the xmpData and if a value is
 * found, pass it to 'proc'.
 *
 * @param xmpData metadata set to inspect
 * @param key key to lookup in exifData
 * @param type extractor type to use
 * @param proc function to call with results
 * @param proc_cls closurer for proc
 * @return 0 to continue extracting, 1 to abort
 */
static int
add_xmp_data (const Exiv2::XmpData& xmpData,
	      const std::string& key,
	      enum EXTRACTOR_MetaType type,
	      EXTRACTOR_MetaDataProcessor proc,
	      void *proc_cls)
{
  const char * str;
  Exiv2::XmpKey ek (key);
  Exiv2::XmpData::const_iterator md = xmpData.findKey (ek);

  while (xmpData.end () != md) 
    {
      if (0 != strcmp (Exiv2::toString (md->key ()).c_str (), key.c_str ()))
	break;
      std::string ccstr = Exiv2::toString (*md);
      str = ccstr.c_str ();
      while ( (strlen (str) > 0) && isspace ((unsigned char) str[0])) str++;
      if (strlen (str) > 0)
	ADD (str, type);
      md++;
    }
  return 0;
}


/**
 * Call 'add_exiv2_tag' for the given key-type combination.
 * Uses 'return' if add_exiv2_tag returns non-0.
 *
 * @param s key to lookup
 * @param type libextractor type to use for the meta data found under the given key
 */
#define ADDEXIV(s,t) do { if (0 != add_exiv2_tag (exifData, s, t, ec->proc, ec->cls)) return; } while (0)


/**
 * Call 'add_iptc_data' for the given key-type combination.
 * Uses 'return' if add_iptc_data returns non-0.
 *
 * @param s key to lookup
 * @param type libextractor type to use for the meta data found under the given key
 */
#define ADDIPTC(s,t) do { if (0 != add_iptc_data (iptcData, s, t, ec->proc, ec->cls)) return; } while (0)


/**
 * Call 'add_xmp_data' for the given key-type combination.
 * Uses 'return' if add_xmp_data returns non-0.
 *
 * @param s key to lookup
 * @param type libextractor type to use for the meta data found under the given key
 */
#define ADDXMP(s,t)  do { if (0 != add_xmp_data  (xmpData,  s, t, ec->proc, ec->cls)) return; } while (0)


/**
 * Main entry method for the 'exiv2' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
extern "C" void
EXTRACTOR_exiv2_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  try
    {	    
#if EXIV2_MAKE_VERSION(0,23,0) <= EXIV2_VERSION
      Exiv2::LogMsg::setLevel (Exiv2::LogMsg::mute);
#endif
      std::auto_ptr<Exiv2::BasicIo> eio(new ExtractorIO (ec));
      Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open (eio);
      if (0 == image.get ())
	return;
      image->readMetadata ();
      Exiv2::ExifData &exifData = image->exifData ();
      if (! exifData.empty ()) 
	{	   			  
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
	  ADDEXIV ("Exif.Photo.ExposureTime", EXTRACTOR_METATYPE_EXPOSURE);
	} 
      
      Exiv2::IptcData &iptcData = image->iptcData();
      if (! iptcData.empty()) 
	{
	  ADDIPTC ("Iptc.Application2.Keywords", EXTRACTOR_METATYPE_KEYWORDS);
	  ADDIPTC ("Iptc.Application2.City", EXTRACTOR_METATYPE_LOCATION_CITY);
	  ADDIPTC ("Iptc.Application2.SubLocation", EXTRACTOR_METATYPE_LOCATION_SUBLOCATION);
	  ADDIPTC ("Iptc.Application2.CountryName", EXTRACTOR_METATYPE_LOCATION_COUNTRY);
	}
      
      Exiv2::XmpData &xmpData = image->xmpData();
      if (! xmpData.empty()) 
	{
	  ADDXMP ("Xmp.photoshop.Country", EXTRACTOR_METATYPE_LOCATION_COUNTRY);
	  ADDXMP ("Xmp.photoshop.City", EXTRACTOR_METATYPE_LOCATION_CITY);
	  ADDXMP ("Xmp.xmp.Rating", EXTRACTOR_METATYPE_RATING);
	  ADDXMP ("Xmp.MicrosoftPhoto.Rating", EXTRACTOR_METATYPE_RATING);
	  ADDXMP ("Xmp.iptc.CountryCode", EXTRACTOR_METATYPE_LOCATION_COUNTRY_CODE);
	  ADDXMP ("Xmp.xmp.CreatorTool", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);
	  ADDXMP ("Xmp.lr.hierarchicalSubject", EXTRACTOR_METATYPE_SUBJECT);
	}	
      }
  catch (const Exiv2::AnyError& e) 
    {
#if DEBUG
      std::cerr << "Caught Exiv2 exception '" << e << "'\n";
#endif
    }
  catch (void *anything)
    {
    }
}

/* end of exiv2_extractor.cc */
