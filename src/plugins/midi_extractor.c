/*
     This file is part of libextractor.
     Copyright (C) 2012 Christian Grothoff

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
 * @file plugins/midi_extractor.c
 * @brief plugin to support MIDI files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <smf.h>


/**
 * Types of events in MIDI.
 */
enum EventType
  {
    ET_SEQUENCE_NUMBER = 0,
    ET_TEXT_EVENT = 1,
    ET_COPYRIGHT_NOTICE = 2,
    ET_TRACK_NAME = 3,
    ET_INSTRUMENT_NAME = 4,
    ET_LYRIC_TEXT = 5,
    ET_MARKER_TEXT = 6,
    ET_CUE_POINT = 7,
    ET_CHANNEL_PREFIX_ASSIGNMENT = 0x20,
    ET_END_OF_TRACK = 0x2F,
    ET_TEMPO_SETTING = 0x51,
    ET_SMPTE_OFFSET = 0x54,
    ET_TIME_SIGNATURE = 0x58,
    ET_KEY_SIGNATURE = 0x59,
    ET_SEQUENCE_SPECIRFIC_EVENT = 0x7F
  };


/**
 * Main entry method for the 'audio/midi' extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_midi_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  void *buf;
  unsigned char *data;
  uint64_t size;
  uint64_t off;
  ssize_t iret;
  smf_t *m = NULL;
  smf_event_t *event;
  uint8_t len;
  
  if (4 >= (iret = ec->read (ec->cls, &buf, 1024)))
    return;
  data = buf;
  if ( (data[0] != 0x4D) || (data[1] != 0x54) ||
       (data[2] != 0x68) || (data[3] != 0x64) )
    return;                /* cannot be MIDI */
  size = ec->get_size (ec->cls);
  if (size > 16 * 1024 * 1024)
    return; /* too large */
  if (NULL == (data = malloc ((size_t) size)))
    return; /* out of memory */
  memcpy (data, buf, iret);
  off = iret;
  while (off < size)
    {
      if (0 >= (iret = ec->read (ec->cls, &buf, 16 * 1024)))
	{
	  free (data);
	  return;
	}
      memcpy (&data[off], buf, iret);
      off += iret;
    } 
  if (0 != ec->proc (ec->cls, 
		     "midi",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "audio/midi",
		     strlen ("audio/midi") + 1))
    goto CLEANUP;
  if (NULL == (m = smf_load_from_memory (data, size)))
    goto CLEANUP;
  while (NULL != (event = smf_get_next_event (m)))
    {
      if (! smf_event_is_metadata (event))
	break;
      len = event->midi_buffer[2];
      if ( (len > 0) &&
	   isspace (event->midi_buffer[2 + len]))
	len--;
#if 0
      fprintf (stderr, 
	       "type: %d, len: %d value: %.*s\n",
	       event->midi_buffer[1],
	       event->midi_buffer[2],
	       (int) event->midi_buffer_length - 3,
	       (char *) &event->midi_buffer[3]);
#endif
      if (1 != event->track_number)
	continue; /* heuristic to not get instruments */
      if (0 == len)
	continue;
      switch (event->midi_buffer[1])
	{
	case ET_TEXT_EVENT:
	  if (0 != ec->proc (ec->cls, 
			     "midi",
			     EXTRACTOR_METATYPE_COMMENT,
			     EXTRACTOR_METAFORMAT_UTF8,
			     "text/plain",
			     (void*) &event->midi_buffer[3],
			     len))
	    goto CLEANUP;
	  break;
	case ET_COPYRIGHT_NOTICE:
	  if (0 != ec->proc (ec->cls, 
			     "midi",
			     EXTRACTOR_METATYPE_COPYRIGHT,
			     EXTRACTOR_METAFORMAT_UTF8,
			     "text/plain",
			     (void*) &event->midi_buffer[3],
			     len))
	    goto CLEANUP;	
	  break;
	case ET_TRACK_NAME:
	  if (0 != ec->proc (ec->cls, 
			     "midi",
			     EXTRACTOR_METATYPE_TITLE,
			     EXTRACTOR_METAFORMAT_UTF8,
			     "text/plain",
			     (void*) &event->midi_buffer[3],
			     len))
	    goto CLEANUP;
	  break;
	case ET_INSTRUMENT_NAME:
	  if (0 != ec->proc (ec->cls, 
			     "midi",
			     EXTRACTOR_METATYPE_SOURCE_DEVICE,
			     EXTRACTOR_METAFORMAT_UTF8,
			     "text/plain",
			     (void*) &event->midi_buffer[3],
			     len))
	    goto CLEANUP;
	  break;
	case ET_LYRIC_TEXT:
	  if (0 != ec->proc (ec->cls, 
			     "midi",
			     EXTRACTOR_METATYPE_LYRICS,
			     EXTRACTOR_METAFORMAT_UTF8,
			     "text/plain",
			     (void*) &event->midi_buffer[3],
			     len))
	    goto CLEANUP;
	  break;
	default:
	  break;
	}
    }
  
 CLEANUP:
  if (NULL != m)
    smf_delete (m);
  free (data);
}


/* end of midi_extractor.c */
