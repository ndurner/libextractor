/*
     This file is part of libextractor.
     Copyright (C) 2008 Heikki Lindholm

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

/*
 * handles AppleSingle and AppleDouble header files
 * see RFC 1740
 */
#include "platform.h"
#include "extractor.h"
#include "pack.h"

#define DEBUG 0

#define APPLESINGLE_SIGNATURE "\x00\x05\x16\x00"
#define APPLEDOUBLE_SIGNATURE "\x00\x05\x16\x07"

typedef struct
{
  unsigned char magic[4];
  unsigned int version;
  char homeFileSystem[16]; /* v1: in ASCII v2: zero filler */
  unsigned short entries;
} ApplefileHeader;

#define APPLEFILE_HEADER_SIZE 26
#define APPLEFILE_HEADER_SPEC "4bW16bH"
#define APPLEFILE_HEADER_FIELDS(p) \
 &(p)->magic, \
 &(p)->version, \
 &(p)->homeFileSystem, \
 &(p)->entries

typedef struct
{
  unsigned int id;
  unsigned int offset;
  unsigned int length;
} ApplefileEntryDescriptor;

#define APPLEFILE_ENTRY_DESCRIPTOR_SIZE 12
#define APPLEFILE_ENTRY_DESCRIPTOR_SPEC "WWW"
#define APPLEFILE_ENTRY_DESCRIPTOR_FIELDS(p) \
 &(p)->id, \
 &(p)->offset, \
 &(p)->length

#define AED_ID_DATA_FORK           1
#define AED_ID_RESOURCE_FORK       2
#define AED_ID_REAL_NAME           3
#define AED_ID_COMMENT             4
#define AED_ID_ICON_BW             5
#define AED_ID_ICON_COLOUR         6
#define AED_ID_FILE_DATES_INFO     8
#define AED_ID_FINDER_INFO         9
#define AED_ID_MACINTOSH_FILE_INFO 10
#define AED_ID_PRODOS_FILE_INFO    11
#define AED_ID_MSDOS_FILE_INFO     12
#define AED_ID_SHORT_NAME          13
#define AED_ID_AFP_FILE_INFO       14
#define AED_ID_DIRECTORY_ID        15

static int readApplefileHeader(const unsigned char *data,
                               size_t *offset,
                               size_t size,
                               ApplefileHeader *hdr)
{
  if ((*offset + APPLEFILE_HEADER_SIZE) > size)
    return -1;

  EXTRACTOR_common_cat_unpack(data + *offset,
             APPLEFILE_HEADER_SPEC,
             APPLEFILE_HEADER_FIELDS(hdr));
  *offset += APPLEFILE_HEADER_SIZE;
  return 0;
}

static int readEntryDescriptor(const unsigned char *data,
                               size_t *offset,
                               size_t size,
                               ApplefileEntryDescriptor *dsc)
{
  if ((*offset + APPLEFILE_ENTRY_DESCRIPTOR_SIZE) > size)
    return -1;

  EXTRACTOR_common_cat_unpack(data + *offset,
             APPLEFILE_ENTRY_DESCRIPTOR_SPEC,
             APPLEFILE_ENTRY_DESCRIPTOR_FIELDS(dsc));
  *offset += APPLEFILE_ENTRY_DESCRIPTOR_SIZE;
  return 0;
}

/* mimetype = application/applefile */
int 
EXTRACTOR_applefile_extract (const char *sdata,
			     size_t size,
			     EXTRACTOR_MetaDataProcessor proc,
			     void *proc_cls,
			     const char *options)
{
  const unsigned char *data = (const unsigned char*) sdata;
  size_t offset;
  ApplefileHeader header;
  ApplefileEntryDescriptor dsc;
  int i;

  offset = 0;
  if (readApplefileHeader(data, &offset, size, &header) == -1)
    return 0;
  if ( (memcmp(header.magic, APPLESINGLE_SIGNATURE, 4) != 0) &&
       (memcmp(header.magic, APPLEDOUBLE_SIGNATURE, 4) != 0) )
    return 0;
  if (0 != proc (proc_cls, 
		 "applefile",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "application/applefile",
		 strlen ("application/applefile")+1))
    return 1;
#if DEBUG
  fprintf(stderr,
	  "applefile header: %08x %d\n", header.version, header.entries);
#endif
  if ( (header.version != 0x00010000) && 
       (header.version != 0x00020000) )
    return 0;

  for (i = 0; i < header.entries; i++) {
    if (readEntryDescriptor(data, &offset, size, &dsc) == -1)
      break;

#if DEBUG
    fprintf(stderr,
	    "applefile entry: %u %u %u\n", dsc.id, dsc.offset, dsc.length);
#endif
    switch (dsc.id) 
      {
      case AED_ID_DATA_FORK:
        {
	  /* same as in filenameextractor.c */
	  char s[14];

        if (dsc.length >= 1000000000)
          snprintf (s, 13, "%.2f %s", dsc.length / 1000000000.0,
                    _("GB"));
        else if (dsc.length >= 1000000)
          snprintf (s, 13, "%.2f %s", dsc.length / 1000000.0, _("MB"));
        else if (dsc.length >= 1000)
          snprintf (s, 13, "%.2f %s", dsc.length / 1000.0, _("KB"));
        else
          snprintf (s, 13, "%.2f %s", (double) dsc.length, _("Bytes"));

	if (0 != proc (proc_cls, 
		       "applefile",
		       EXTRACTOR_METATYPE_EMBEDDED_FILE_SIZE,
		       EXTRACTOR_METAFORMAT_UTF8,
		       "text/plain",
		       s,
		       strlen (s) + 1))
	  return 1;
	}
	break;      
      case AED_ID_REAL_NAME:
	{
	  char s[2048];
	  if ( (dsc.length < sizeof(s)) && 
	       ((dsc.offset + dsc.length) < size) ) {
	    memcpy(s, data + dsc.offset, dsc.length);
	    s[dsc.length] = '\0';
	    if (0 != proc (proc_cls, 
			   "applefile",
			   EXTRACTOR_METATYPE_FILENAME,
			   EXTRACTOR_METAFORMAT_UTF8,
			   "text/plain",
			   s,
			   dsc.length + 1))
	      return 1;	
	  }
	}
	break;
      case AED_ID_COMMENT:
        if ( (dsc.length < 65536) && ((dsc.offset + dsc.length) < size) ) {
	  char *s = malloc(dsc.length + 1);
	  if (s != NULL) {
            memcpy(s, data + dsc.offset, dsc.length);
	    s[dsc.length] = '\0';
	    if (0 != proc (proc_cls, 
			   "applefile",
			   EXTRACTOR_METATYPE_COMMENT,
			   EXTRACTOR_METAFORMAT_UTF8,
			   "text/plain",
			   s,
			   dsc.length + 1))
	      {
		free (s);
		return 1;	
	      }
	    free (s);	    
	  }
	}      
	break;
      case AED_ID_FINDER_INFO:
        if (dsc.length >= 16 && (dsc.offset + dsc.length) < size) {
          char s[5];
	  memcpy(s, data + dsc.offset, 4);
	  s[4] = '\0';
	  if (0 != proc (proc_cls, 
			 "applefile",
			 EXTRACTOR_METATYPE_FINDER_FILE_TYPE,
			 EXTRACTOR_METAFORMAT_C_STRING,
			 "text/plain",
			 s,
			 strlen(s) + 1))
	    return 1;	
          
	  memcpy(s, data + dsc.offset + 4, 4);
	  s[4] = '\0';
	  if (0 != proc (proc_cls, 
			 "applefile",
			 EXTRACTOR_METATYPE_FINDER_FILE_CREATOR,
			 EXTRACTOR_METAFORMAT_C_STRING,
			 "text/plain",
			 s,
			 strlen(s) + 1))
	    return 1;	
        }
        break;
      default:
        break;
    }
  }
  return 0;
}
