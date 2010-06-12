/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
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

#include "platform.h"
#include "extractor.h"
#include <zlib.h>

/*
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


static char *
stndup (const char *str, size_t n)
{
  char *tmp;
  tmp = malloc (n + 1);
  if (tmp == NULL)
    return NULL;
  tmp[n] = '\0';
  memcpy (tmp, str, n);
  return tmp;
}



typedef struct
{
  const char *text;
  enum EXTRACTOR_MetaType type;
} Matches;

/* see also: "man 5 deb-control" */
static Matches tmap[] = {
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
 * Process the control file.
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

  pos = 0;
  while (pos < size)
    {
      size_t colon;
      size_t eol;
      int i;

      colon = pos;
      while (data[colon] != ':')
        {
          if ((colon > size) || (data[colon] == '\n'))
            return 0;
          colon++;
        }
      colon++;
      while ((colon < size) && (isspace ((unsigned char) data[colon])))
        colon++;
      eol = colon;
      while ((eol < size) &&
             ((data[eol] != '\n') ||
              ((eol + 1 < size) && (data[eol + 1] == ' '))))
        eol++;
      if ((eol == colon) || (eol > size))
        return 0;
      key = stndup (&data[pos], colon - pos);
      if (key == NULL)
	return 0;
      i = 0;
      while (tmap[i].text != NULL)
        {
          if (0 == strcmp (key, tmap[i].text))
            {
              val = stndup (&data[colon], eol - colon);
	      if (val == NULL)
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
          i++;
        }
      free (key);
      pos = eol + 1;
    }
  return 0;
}


typedef struct
{
  char name[100];
  char mode[8];
  char userId[8];
  char groupId[8];
  char filesize[12];
  char lastModTime[12];
  char chksum[8];
  char link;
  char linkName[100];
} TarHeader;

typedef struct
{
  TarHeader tar;
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
} USTarHeader;

/**
 * Process the control.tar file.
 */
static int
processControlTar (const char *data,
                   const size_t size,
		   EXTRACTOR_MetaDataProcessor proc,
		   void *proc_cls)
{
  TarHeader *tar;
  USTarHeader *ustar;
  size_t pos;

  pos = 0;
  while (pos + sizeof (TarHeader) < size)
    {
      unsigned long long fsize;
      char buf[13];

      tar = (TarHeader *) & data[pos];
      if (pos + sizeof (USTarHeader) < size)
        {
          ustar = (USTarHeader *) & data[pos];
          if (0 == strncmp ("ustar", &ustar->magic[0], strlen ("ustar")))
            pos += 512;         /* sizeof(USTarHeader); */
          else
            pos += 257;         /* sizeof(TarHeader); minus gcc alignment... */
        }
      else
        {
          pos += 257;           /* sizeof(TarHeader); minus gcc alignment... */
        }

      memcpy (buf, &tar->filesize[0], 12);
      buf[12] = '\0';
      if (1 != sscanf (buf, "%12llo", &fsize))  /* octal! Yuck yuck! */
        return 0;
      if ((pos + fsize > size) || (fsize > size) || (pos + fsize < pos))
        return 0;

      if (0 == strncmp (&tar->name[0], "./control", strlen ("./control")))
        {
          return processControl (&data[pos], fsize, proc, proc_cls);
        }
      if ((fsize & 511) != 0)
        fsize = (fsize | 511) + 1;      /* round up! */
      if (pos + fsize < pos)
        return 0;
      pos += fsize;
    }
  return 0;
}

#define MAX_CONTROL_SIZE (1024 * 1024)

static voidpf
Emalloc (voidpf opaque, uInt items, uInt size)
{
  if (SIZE_MAX / size <= items)
    return NULL;
  return malloc (size * items);
}

static void
Efree (voidpf opaque, voidpf ptr)
{
  free (ptr);
}

/**
 * Process the control.tar.gz file.
 */
static int
processControlTGZ (const unsigned char *data,
                   size_t size, 
		   EXTRACTOR_MetaDataProcessor proc,
		   void *proc_cls)
{
  uint32_t bufSize;
  char *buf;
  z_stream strm;
  int ret;

  bufSize = data[size - 4] + (data[size - 3] << 8) + (data[size - 2] << 16) + (data[size - 1] << 24);
  if (bufSize > MAX_CONTROL_SIZE)
    return 0;
  memset (&strm, 0, sizeof (z_stream));
  strm.next_in = (Bytef *) data;
  strm.avail_in = size;
  strm.total_in = 0;
  strm.zalloc = &Emalloc;
  strm.zfree = &Efree;
  strm.opaque = NULL;

  if (Z_OK == inflateInit2 (&strm, 15 + 32))
    {
      buf = malloc (bufSize);
      if (buf == NULL)
        {
          inflateEnd (&strm);
          return 0;
        }
      strm.next_out = (Bytef *) buf;
      strm.avail_out = bufSize;
      inflate (&strm, Z_FINISH);
      if (strm.total_out > 0)
        {
          ret = processControlTar (buf, strm.total_out, proc, proc_cls);
          inflateEnd (&strm);
          free (buf);
          return ret;
        }
      free (buf);
      inflateEnd (&strm);
    }
  return 0;
}

typedef struct
{
  char name[16];
  char lastModTime[12];
  char userId[6];
  char groupId[6];
  char modeInOctal[8];
  char filesize[10];
  char trailer[2];
} ObjectHeader;


int 
EXTRACTOR_deb_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  size_t pos;
  int done = 0;
  ObjectHeader *hdr;
  unsigned long long fsize;
  char buf[11];

  if (size < 128)
    return 0;
  if (0 != strncmp ("!<arch>\n", data, strlen ("!<arch>\n")))
    return 0;
  pos = strlen ("!<arch>\n");
  while (pos + sizeof (ObjectHeader) < size)
    {
      hdr = (ObjectHeader *) & data[pos];
      if (0 != strncmp (&hdr->trailer[0], "`\n", 2))
        return 0;
      memcpy (buf, &hdr->filesize[0], 10);
      buf[10] = '\0';
      if (1 != sscanf (buf, "%10llu", &fsize))
        return 0;
      pos += sizeof (ObjectHeader);
      if ((pos + fsize > size) || (fsize > size) || (pos + fsize < pos))
        return 0;
      if (0 == strncmp (&hdr->name[0],
                        "control.tar.gz", strlen ("control.tar.gz")))
        {
          if (0 != processControlTGZ ((const unsigned char *) &data[pos],
				      fsize, proc, proc_cls))
	    return 1;
          done++;
        }
      if (0 == strncmp (&hdr->name[0],
                        "debian-binary", strlen ("debian-binary")))
        {
	  if (0 != proc (proc_cls, 
			 "deb",
			 EXTRACTOR_METATYPE_MIMETYPE,
			 EXTRACTOR_METAFORMAT_UTF8,
			 "text/plain",
			 "application/x-debian-package",
			 strlen ("application/x-debian-package")+1))
	    return 1;
          done++;
        }
      pos += fsize;
      if (done == 2)
        break;                  /* no need to process the rest of the archive */
    }
  return 0;
}
