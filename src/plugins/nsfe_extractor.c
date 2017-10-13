/*
 * This file is part of libextractor.
 * Copyright (C) 2007, 2009, 2012 Toni Ruottu and Christian Grothoff
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3, or (at your
 * option) any later version.
 *
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/**
 * @file plugins/nsfe_extractor.c
 * @brief plugin to support Nes Sound Format files
 * @author Toni Ruottu
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include "convert.h"


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

/**
 * "Header" of an NSFE file.
 */
struct header
{
  char magicid[4];
};


/**
 * Read an unsigned integer at the current offset.
 *
 * @param data input data to parse
 * @return parsed integer
 */
static uint32_t
nsfeuint (const char *data)
{
  uint32_t value = 0;

  for (int i = 3; i > 0; i--)
    {
      value += (unsigned char) data[i];
      value *= 0x100;
    }
  value += (unsigned char) data[0];
  return value;
}


/**
 * Copy string starting at 'data' with at most
 * 'size' bytes. (strndup).
 *
 * @param data input data to copy
 * @param size number of bytes in 'data'
 * @return copy of the string at data
 */
static char *
nsfestring (const char *data,
	    size_t size)
{
  char *s;
  size_t length;

  length = 0;
  while ( (length < size) &&
	  (data[length] != '\0') )
    length++;
  if (NULL == (s = malloc (length + 1)))
    return NULL;
  memcpy (s, data, length);
  s[length] = '\0';
  return s;
}


/**
 * Give metadata to LE; return if 'proc' returns non-zero.
 *
 * @param s metadata value as UTF8
 * @param t metadata type to use
 */
#define ADD(s,t) do { if (0 != ec->proc (ec->cls, "nsfe", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) return  1; } while (0)


/**
 * Give metadata to LE; return if 'proc' returns non-zero.
 *
 * @param s metadata value as UTF8, free at the end
 * @param t metadata type to use
 */
#define ADDF(s,t) do { if (0 != ec->proc (ec->cls, "nsfe", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen (s) + 1)) { free (s); return 1; } free (s); } while (0)


/**
 * Format of an 'INFO' chunk.  Last two bytes are optional.
 */
struct infochunk
{
  /**
   * Unknown.
   */
  uint16_t loadaddr;

  /**
   * Unknown.
   */
  uint16_t initaddr;

  /**
   * Unknown.
   */
  uint16_t playaddr;

  /**
   * TV encoding flags.
   */
  char tvflags;

  /**
   * Chipset encoding flags.
   */
  char chipflags;

  /**
   * Number of songs.
   */
  unsigned char songs;

  /**
   * Starting song.
   */
  unsigned char firstsong;
};


/**
 * Extract data from the INFO chunk.
 *
 * @param ec extraction context
 * @param size number of bytes in INFO chunk
 * @return 0 to continue extrating
 */
static int
info_extract (struct EXTRACTOR_ExtractContext *ec,
	      uint32_t size)
{
  void *data;
  const struct infochunk *ichunk;
  char songs[32];

  if (size < 8)
    return 0;
  if (size >
      ec->read (ec->cls,
		&data,
		size))
    return 1;
  ichunk = data;

  if (0 != (ichunk->tvflags & DUAL_FLAG))
    {
      ADD ("PAL/NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
    }
  else
    {
      if (0 != (ichunk->tvflags & PAL_FLAG))
        ADD ("PAL", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
      else
        ADD ("NTSC", EXTRACTOR_METATYPE_BROADCAST_TELEVISION_SYSTEM);
    }

  if (0 != (ichunk->chipflags & VRCVI_FLAG))
    ADD ("VRCVI", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (ichunk->chipflags & VRCVII_FLAG))
    ADD ("VRCVII", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (ichunk->chipflags & FDS_FLAG))
    ADD ("FDS Sound", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (ichunk->chipflags & MMC5_FLAG))
    ADD ("MMC5 audio", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (ichunk->chipflags & NAMCO_FLAG))
    ADD ("Namco 106", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
  if (0 != (ichunk->chipflags & SUNSOFT_FLAG))
    ADD ("Sunsoft FME-07", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);

  if (size < sizeof (struct infochunk))
    {
      ADD ("1", EXTRACTOR_METATYPE_SONG_COUNT);
      return 0;
    }
  snprintf (songs,
	    sizeof (songs),
	    "%d",
	    ichunk->songs);
  ADD (songs, EXTRACTOR_METATYPE_SONG_COUNT);
  snprintf (songs,
	    sizeof (songs),
	    "%d",
	    ichunk->firstsong);
  ADD (songs, EXTRACTOR_METATYPE_STARTING_SONG);
  return 0;
}


/**
 * Extract data from the TLBL chunk.
 *
 * @param ec extraction context
 * @param size number of bytes in TLBL chunk
 * @return 0 to continue extrating
 */
static int
tlbl_extract (struct EXTRACTOR_ExtractContext *ec,
	      uint32_t size)
{
  char *title;
  ssize_t left;
  size_t length;
  void *data;
  const char *cdata;

  if (size >
      ec->read (ec->cls,
		&data,
		size))
    return 1;
  cdata = data;

  left = size;
  while (left > 0)
    {
      title = nsfestring (&cdata[size - left], left);
      if (NULL == title)
	return 0;
      length = strlen (title) + 1;
      ADDF (title, EXTRACTOR_METATYPE_TITLE);
      left -= length;
    }
  return 0;
}


/**
 * Extract data from the AUTH chunk.
 *
 * @param ec extraction context
 * @param size number of bytes in AUTH chunk
 * @return 0 to continue extrating
 */
static int
auth_extract (struct EXTRACTOR_ExtractContext *ec,
	      uint32_t size)
{
  char *album;
  char *artist;
  char *copyright;
  char *ripper;
  uint32_t left = size;
  void *data;
  const char *cdata;

  if (left < 1)
    return 0;
  if (size >
      ec->read (ec->cls,
		&data,
		size))
    return 1;
  cdata = data;

  album = nsfestring (&cdata[size - left], left);
  if (NULL != album)
    {
      left -= (strlen (album) + 1);
      ADDF (album, EXTRACTOR_METATYPE_ALBUM);
      if (left < 1)
	return 0;
    }

  artist = nsfestring (&cdata[size - left], left);
  if (NULL != artist)
    {
      left -= (strlen (artist) + 1);
      ADDF (artist, EXTRACTOR_METATYPE_ARTIST);
      if (left < 1)
	return 0;
    }

  copyright = nsfestring (&cdata[size - left], left);
  if (NULL != copyright)
    {
      left -= (strlen (copyright) + 1);
      ADDF (copyright, EXTRACTOR_METATYPE_COPYRIGHT);
      if (left < 1)
	return 0;
    }
  ripper = nsfestring (&cdata[size - left], left);
  if (NULL != ripper)
    ADDF (ripper, EXTRACTOR_METATYPE_RIPPER);
  return 0;
}


/**
 * "extract" meta data from an Extended Nintendo Sound Format file
 *
 * NSFE specification revision 2 (Sep. 3, 2003) was used, while this
 * piece of software was originally written.
 *
 * @param ec extraction context
 */
void
EXTRACTOR_nsfe_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  const struct header *head;
  void *data;
  uint64_t off;
  uint32_t chunksize;
  int ret;

  if (sizeof (struct header) >
      ec->read (ec->cls,
		&data,
		sizeof (struct header)))
    return;
  head = data;
  if (0 != memcmp (head->magicid, "NSFE", 4))
    return;

  if (0 != ec->proc (ec->cls,
		     "nsfe",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "audio/x-nsfe",
		     strlen ("audio/x-nsfe") + 1))
    return;
  off = sizeof (struct header);
  ret = 0;
  while (0 == ret)
    {
      if (off != ec->seek (ec->cls,
			   off,
			   SEEK_SET))
	break;
      if (8 >
	  ec->read (ec->cls,
		    &data,
		    8))
	break;
      chunksize = nsfeuint (data);
      if (off + chunksize + 8LLU <= off)
        break; /* protect against looping */
      off += 8LLU + chunksize;
      if (0 == memcmp (data + 4, "INFO", 4))
        ret = info_extract (ec, chunksize);
      else if (0 == memcmp (data + 4, "auth", 4))
	ret = auth_extract (ec, chunksize);
      else if (0 == memcmp (data + 4, "tlbl", 4))
	ret = tlbl_extract (ec, chunksize);
      /* Ignored chunks: DATA, NEND, plst, time, fade, BANK */
    }
}

/* end of nsfe_extractor.c */
