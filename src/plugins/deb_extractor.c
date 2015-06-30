/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/deb_extractor.c
 * @brief plugin to support Debian archives 
 * @author Christian Grothoff
 *
 * The .deb is an ar-chive file.  It contains a tar.gz file
 * named "control.tar.gz" which then contains a file 'control'
 * that has the meta-data.  And which variant of the various
 * ar file formats is used is also not quite certain. Yuck.
 *
 * References:
 * http://www.mkssoftware.com/docs/man4/tar.4.asp
 * http://lists.debian.org/debian-policy/2003/12/msg00000.html
 * http://www.opengroup.org/onlinepubs/009695399/utilities/ar.html
 */
#include "platform.h"
#include "extractor.h"
#include <zlib.h>


/**
 * Maximum file size we allow for control.tar.gz files.
 * This is a sanity check to avoid allocating huge amounts
 * of memory.
 */
#define MAX_CONTROL_SIZE (1024 * 1024)


/**
 * Re-implementation of 'strndup'.
 *
 * @param str string to duplicate
 * @param n maximum number of bytes to copy
 * @return NULL on error, otherwise 0-terminated copy of 'str'
 *         with at most n characters
 */
static char *
stndup (const char *str, size_t n)
{
  char *tmp;

  if (NULL == (tmp = malloc (n + 1)))
    return NULL;
  tmp[n] = '\0';
  memcpy (tmp, str, n);
  return tmp;
}


/**
 * Entry in the mapping from control data to LE types.
 */
struct Matches
{
  /**
   * Key in the Debian control file.
   */
  const char *text;

  /**
   * Corresponding type in LE.
   */
  enum EXTRACTOR_MetaType type;
};


/**
 * Map from deb-control entries to LE types.
 *
 * see also: "man 5 deb-control" 
 */
static struct Matches tmap[] = {
  {"Package: ",       EXTRACTOR_METATYPE_PACKAGE_NAME},
  {"Version: ",       EXTRACTOR_METATYPE_PACKAGE_VERSION},
  {"Section: ",       EXTRACTOR_METATYPE_SECTION},
  {"Priority: ",      EXTRACTOR_METATYPE_UPLOAD_PRIORITY},
  {"Architecture: ",  EXTRACTOR_METATYPE_TARGET_ARCHITECTURE},
  {"Depends: ",       EXTRACTOR_METATYPE_PACKAGE_DEPENDENCY},
  {"Recommends: ",    EXTRACTOR_METATYPE_PACKAGE_RECOMMENDS},
  {"Suggests: ",      EXTRACTOR_METATYPE_PACKAGE_SUGGESTS},
  {"Installed-Size: ",EXTRACTOR_METATYPE_PACKAGE_INSTALLED_SIZE},
  {"Maintainer: ",    EXTRACTOR_METATYPE_PACKAGE_MAINTAINER},
  {"Description: ",   EXTRACTOR_METATYPE_DESCRIPTION},
  {"Source: ",        EXTRACTOR_METATYPE_PACKAGE_SOURCE},
  {"Pre-Depends: ",   EXTRACTOR_METATYPE_PACKAGE_PRE_DEPENDENCY},
  {"Conflicts: ",     EXTRACTOR_METATYPE_PACKAGE_CONFLICTS},
  {"Replaces: ",      EXTRACTOR_METATYPE_PACKAGE_REPLACES},
  {"Provides: ",      EXTRACTOR_METATYPE_PACKAGE_PROVIDES},
  {"Essential: ",     EXTRACTOR_METATYPE_PACKAGE_ESSENTIAL},
  {NULL, 0}
};


/**
 * Process the "control" file from the control.tar.gz
 *
 * @param data decompressed control data
 * @param size number of bytes in data
 * @param proc function to call with meta data
 * @param proc_cls closure for 'proc'
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processControl (const char *data,
                const size_t size,
		EXTRACTOR_MetaDataProcessor proc,
		void *proc_cls)
{
  size_t pos;
  char *key;
  char *val;
  size_t colon;
  size_t eol;
  unsigned int i;
  
  pos = 0;
  while (pos < size)
    {
      for (colon = pos; ':' != data[colon]; colon++)
	if ((colon > size) || ('\n' == data[colon]))
	  return 0;
      colon++;
      while ((colon < size) && (isspace ((unsigned char) data[colon])))
        colon++;
      eol = colon;
      while ((eol < size) &&
             (('\n' != data[eol]) ||
              ((eol + 1 < size) && (' '  == data[eol + 1]))))
        eol++;
      if ((eol == colon) || (eol > size))
        return 0;
      if (NULL == (key = stndup (&data[pos], colon - pos)))
	return 0;
      for (i = 0; NULL != tmap[i].text; i++)
        {
          if (0 != strcmp (key, tmap[i].text))
	    continue;
	  if (NULL == (val = stndup (&data[colon], eol - colon)))
	    {
	      free (key);
	      return 0;
	    }
	  if (0 != proc (proc_cls, 
			 "deb",
			 tmap[i].type,
			 EXTRACTOR_METAFORMAT_UTF8,
			 "text/plain",
			 val,
			 strlen(val) + 1))
	    {
	      free (val);
	      free (key);
	      return 1;
	    }
	  free (val);
	  break;
	}
      free (key);
      pos = eol + 1;
    }
  return 0;
}


/**
 * Header of an entry in a TAR file.
 */
struct TarHeader
{
  /**
   * Filename.
   */
  char name[100];
 
  /**
   * File access modes.
   */
  char mode[8];

  /**
   * Owner of the file.
   */
  char userId[8];

  /**
   * Group of the file.
   */
  char groupId[8];

  /**
   * Size of the file, in octal.
   */
  char filesize[12];
  
  /**
   * Last modification time.
   */
  char lastModTime[12];

  /**
   * Checksum of the file.
   */
  char chksum[8];

  /**
   * Is the file a link?
   */
  char link;

  /**
   * Destination of the link.
   */
  char linkName[100];
};


/**
 * Extended TAR header for USTar format.
 */
struct USTarHeader
{
  /**
   * Original TAR header.
   */
  struct TarHeader tar;

  /**
   * Additinal magic for USTar.
   */
  char magic[6];

  /**
   * Format version.
   */
  char version[2];

  /**
   * User name.
   */
  char uname[32];

  /**
   * Group name.
   */
  char gname[32];

  /**
   * Device major number.
   */
  char devmajor[8];

  /**
   * Device minor number.
   */
  char devminor[8];

  /**
   * Unknown (padding?).
   */
  char prefix[155];
};


/**
 * Process the control.tar file.
 *
 * @param data the deflated control.tar file data
 * @param size number of bytes in data
 * @param proc function to call with meta data
 * @param proc_cls closure for 'proc'
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processControlTar (const char *data,
		   size_t size,
		   EXTRACTOR_MetaDataProcessor proc,
		   void *proc_cls)
{
  struct TarHeader *tar;
  struct USTarHeader *ustar;
  size_t pos;

  pos = 0;
  while (pos + sizeof (struct TarHeader) < size)
    {
      unsigned long long fsize;
      char buf[13];

      tar = (struct TarHeader *) & data[pos];
      if (pos + sizeof (struct USTarHeader) < size)
        {
          ustar = (struct USTarHeader *) & data[pos];
          if (0 == strncmp ("ustar", &ustar->magic[0], strlen ("ustar")))
            pos += 512;         /* sizeof (struct USTarHeader); */
          else
            pos += 257;         /* sizeof (struct TarHeader); minus gcc alignment... */
        }
      else
        {
          pos += 257;           /* sizeof (struct TarHeader); minus gcc alignment... */
        }

      memcpy (buf, &tar->filesize[0], 12);
      buf[12] = '\0';
      if (1 != sscanf (buf, "%12llo", &fsize))  /* octal! Yuck yuck! */
        return 0;
      if ((pos + fsize > size) || (fsize > size) || (pos + fsize < pos))
        return 0;

      if (0 == strncmp (&tar->name[0], "./control", strlen ("./control")))
        {
	  /* found the 'control' file we were looking for */
          return processControl (&data[pos], fsize, proc, proc_cls);
        }
      if (0 != (fsize & 511))
        fsize = (fsize | 511) + 1;      /* round up! */
      if (pos + fsize < pos)
        return 0;
      pos += fsize;
    }
  return 0;
}


/**
 * Process the control.tar.gz file.
 *
 * @param ec extractor context with control.tar.gz at current read position
 * @param size number of bytes in the control file
 * @return 0 to continue extracting, 1 if we are done
 */
static int
processControlTGZ (struct EXTRACTOR_ExtractContext *ec,
                   unsigned long long size)
{
  uint32_t bufSize;
  char *buf;
  void *data;
  unsigned char *cdata;
  z_stream strm;
  int ret;
  ssize_t sret;
  unsigned long long off;

  if (size > MAX_CONTROL_SIZE)
    return 0;
  if (NULL == (cdata = malloc (size)))
    return 0;
  off = 0;
  while (off < size)
    {
      if (0 >= (sret = ec->read (ec->cls, &data, size - off)))
	{
	  free (cdata);
	  return 0;
	}
      memcpy (&cdata[off], data, sret);
      off += sret;
    }
  bufSize = cdata[size - 4] + (cdata[size - 3] << 8) + (cdata[size - 2] << 16) + (cdata[size - 1] << 24);
  if (bufSize > MAX_CONTROL_SIZE)
    {
      free (cdata);
      return 0;
    }
  if (NULL == (buf = malloc (bufSize)))
    {
      free (cdata);
      return 0;
    }
  ret = 0;
  memset (&strm, 0, sizeof (z_stream));
  strm.next_in = (Bytef *) data;
  strm.avail_in = size;
  if (Z_OK == inflateInit2 (&strm, 15 + 32))
    {  
      strm.next_out = (Bytef *) buf;
      strm.avail_out = bufSize;
      inflate (&strm, Z_FINISH);
      if (strm.total_out > 0)
	ret = processControlTar (buf, strm.total_out, 
				 ec->proc, ec->cls);
      inflateEnd (&strm);
    }
  free (buf);
  free (cdata);
  return ret;
}


/**
 * Header of an object in an "AR"chive file.
 */
struct ObjectHeader
{
  /**
   * Name of the file.
   */
  char name[16];

  /**
   * Last modification time for the file.
   */
  char lastModTime[12];

  /**
   * User ID of the owner.
   */
  char userId[6];

  /**
   * Group ID of the owner.
   */
  char groupId[6];

  /**
   * File access modes.
   */
  char modeInOctal[8];

  /**
   * Size of the file (as decimal string)
   */
  char filesize[10];

  /**
   * Tailer of the object header ("`\n")
   */
  char trailer[2];
};


/**
 * Main entry method for the DEB extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void 
EXTRACTOR_deb_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  uint64_t pos;
  int done = 0;
  const struct ObjectHeader *hdr;
  uint64_t fsize;
  unsigned long long csize;
  char buf[11];
  void *data;

  fsize = ec->get_size (ec->cls);
  if (fsize < 128)
    return;
  if (8 !=
      ec->read (ec->cls, &data, 8))
    return;
  if (0 != strncmp ("!<arch>\n", data, 8))
    return;
  pos = 8;
  while (pos + sizeof (struct ObjectHeader) < fsize)
    {
      if (pos !=
	  ec->seek (ec->cls, pos, SEEK_SET))
	return;
      if (sizeof (struct ObjectHeader) !=
	  ec->read (ec->cls, &data, sizeof (struct ObjectHeader)))
	return;
      hdr = data;
      if (0 != strncmp (&hdr->trailer[0], "`\n", 2))
        return;
      memcpy (buf, &hdr->filesize[0], 10);
      buf[10] = '\0';
      if (1 != sscanf (buf, "%10llu", &csize))
        return;
      pos += sizeof (struct ObjectHeader);
      if ((pos + csize > fsize) || (csize > fsize) || (pos + csize < pos))
        return;
      if (0 == strncmp (&hdr->name[0],
                        "control.tar.gz", 
			strlen ("control.tar.gz")))
        {
	  if (0 != processControlTGZ (ec,
				      csize))
	    return;
          done++;
        }
      if (0 == strncmp (&hdr->name[0],
                        "debian-binary", strlen ("debian-binary")))
        {
	  if (0 != ec->proc (ec->cls, 
			     "deb",
			     EXTRACTOR_METATYPE_MIMETYPE,
			     EXTRACTOR_METAFORMAT_UTF8,
			     "text/plain",
			     "application/x-debian-package",
			     strlen ("application/x-debian-package")+1))
	    return;
          done++;
        }
      pos += csize;
      if (2 == done)
        break;                  /* no need to process the rest of the archive */
    }
}

/* end of deb_extractor.c */
