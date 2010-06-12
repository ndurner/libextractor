/*
 * This file is part of libextractor.
 * (C) 2007, 2009 Toni Ruottu
 * 
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 * 
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "platform.h"
#include "extractor.h"
#include "convert.h"

#define HEADER_SIZE  0x04

/* television system flags */

#define PAL_FLAG     0x01
#define DUAL_FLAG    0x02

/* sound chip flags */

#define VRCVI_FLAG   0x01
#define VRCVII_FLAG  0x02
#define FDS_FLAG     0x04
#define MMC5_FLAG    0x08
#define NAMCO_FLAG   0x10
#define SUNSOFT_FLAG 0x20

#define UINT16 unsigned short

struct header
{
  char magicid[4];
};

struct infochunk
{
  UINT16 loadaddr;
  UINT16 initaddr;
  UINT16 playaddr;
  char tvflags;
  char chipflags;
  char songs;
  char firstsong;
};

static int
nsfeuint (const char *data)
{
  int i;
  int value = 0;

  for (i = 3; i > 0; i--)
    {
      value += (unsigned char) data[i];
      value *= 0x100;
    }
  value += (unsigned char) data[0];
  return value;
}


static char *
nsfestring (const char *data, size_t size)
{
  char *s;
  size_t length;

  length = 0;
  while ( (length < size) &&
	  (data[length] != '\0') )
    length++;
  s = malloc (length + 1);
  if (s == NULL)
    return NULL;
  strncpy (s, data, length);
  s[strlen (data)] = '\0';
  return s;
}

#define ADD(s,t) do { if (0 != proc (proc_cls, "nsfe", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)

#define ADDF(s,t) do { if (0 != proc (proc_cls, "nsfe", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) { free(s); return 1; } free (s); } while (0)

static int
libextractor_nsfe_info_extract(const char *data, 
			       size_t size, 
			       EXTRACTOR_MetaDataProcessor proc,
			       void *proc_cls)
{
  const struct infochunk *ichunk;
  char songs[32];

  if (size < 8)    
    return 0;
  ichunk = (const struct infochunk *) data;
  if (ichunk->tvflags & DUAL_FLAG)
    {
      ADD ("PAL/NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
    }
  else
    {
      if (ichunk->tvflags & PAL_FLAG)
        ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
      else
        ADD ("NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);        
    }

  if (ichunk->chipflags & VRCVI_FLAG)
    ADD ("VRCVI", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (ichunk->chipflags & VRCVII_FLAG)
    ADD ("VRCVII", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (ichunk->chipflags & FDS_FLAG)
    ADD ("FDS Sound", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);    
  if (ichunk->chipflags & MMC5_FLAG)
    ADD ("MMC5 audio", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);    
  if (ichunk->chipflags & NAMCO_FLAG)
    ADD ("Namco 106", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (ichunk->chipflags & SUNSOFT_FLAG)
    ADD ("Sunsoft FME-07", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);    
  if (size < 9)
    {
      ADD ("1", EXTRACTOR_METATYPE_SONG_COUNT);
      return 0;
    }
  snprintf (songs, 
	    sizeof(songs),
	    "%d",
	    ichunk->songs);
  ADD (songs, EXTRACTOR_METATYPE_SONG_COUNT);
  return 0;
}


static int
libextractor_nsfe_tlbl_extract(const char *data, 
			       size_t size,
			       EXTRACTOR_MetaDataProcessor proc,
			       void *proc_cls)

{
  char *title;
  ssize_t left;
  size_t length;

  for (left = size; left > 0; left -= length)
    {
      title = nsfestring (&data[size - left], left);
      if (title == NULL)
	return 0;	
      length = strlen (title) + 1;
      ADDF (title, EXTRACTOR_METATYPE_TITLE);
    }
  return 0;
}

static int
libextractor_nsfe_auth_extract (const char *data, size_t size, 
				EXTRACTOR_MetaDataProcessor proc,
				void *proc_cls)
{
  char *album;
  char *artist;
  char *copyright;
  char *ripper;
  int left = size;

  if (left < 1)
    return 0;
  album = nsfestring (&data[size - left], left);
  if (album != NULL)
    {
      left -= (strlen (album) + 1);
      ADDF (album, EXTRACTOR_METATYPE_ALBUM);
      if (left < 1)    
	return 0;    
    }

  artist = nsfestring (&data[size - left], left);
  if (artist != NULL)
    {
      left -= (strlen (artist) + 1);
      ADDF (artist, EXTRACTOR_METATYPE_ARTIST);
      if (left < 1)    
	return 0;
    }

  copyright = nsfestring (&data[size - left], left);
  if (copyright != NULL)
    {
      left -= (strlen (copyright) + 1);
      ADDF (copyright, EXTRACTOR_METATYPE_COPYRIGHT);
      if (left < 1)
	return 0;
    }
  ripper = nsfestring (&data[size - left], left);
  if (ripper != NULL)
    ADDF (ripper, EXTRACTOR_METATYPE_RIPPER);
  return 0;
}


/* "extract" keyword from an Extended Nintendo Sound Format file
 *
 * NSFE specification revision 2 (Sep. 3, 2003)
 * was used, while this piece of software was
 * originally written.
 *
 */
int 
EXTRACTOR_nsfe_extract (const char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  const struct header *head;
  int i;
  char chunkid[5] = "     ";
  int ret;

  if (size < HEADER_SIZE)    
    return 0;
  head = (const struct header *) data;
  if (memcmp (head->magicid, "NSFE", 4))    
    return 0;
  ADD ("audio/x-nsfe", EXTRACTOR_METATYPE_MIMETYPE);
  i = 4;                        /* Jump over magic id */
  ret = 0;
  while (i + 7 < size && strncmp (chunkid, "NEND", 4))  /* CHECK */
    {
      unsigned int chunksize = nsfeuint (&data[i]);

      i += 4;                   /* Jump over chunk size */
      memcpy (&chunkid, data + i, 4);
      chunkid[4] = '\0';

      i += 4;                   /* Jump over chunk id */
      if (!strncmp (chunkid, "INFO", 4))
        ret = libextractor_nsfe_info_extract (data + i, chunksize, proc, proc_cls);        
      else if (!strncmp (chunkid, "auth", 4))
	ret = libextractor_nsfe_auth_extract (data + i, chunksize, proc, proc_cls);        
      else if (!strncmp (chunkid, "tlbl", 4))
	ret = libextractor_nsfe_tlbl_extract (data + i, chunksize, proc, proc_cls);
      /* Ignored chunks: DATA, NEND, plst, time, fade, BANK */
      i += chunksize;
      if (ret != 0)
	break;
    }
  return ret;
}
