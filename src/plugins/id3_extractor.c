/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2006, 2009, 2010 Vidyut Samanta and Christian Grothoff

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
#include "convert.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "extractor_plugins.h"

typedef struct
{
  char *title;
  char *artist;
  char *album;
  char *year;
  char *comment;
  const char *genre;
  unsigned int track_number;
} id3tag;

static const char *const genre_names[] = {
  gettext_noop ("Blues"),
  gettext_noop ("Classic Rock"),
  gettext_noop ("Country"),
  gettext_noop ("Dance"),
  gettext_noop ("Disco"),
  gettext_noop ("Funk"),
  gettext_noop ("Grunge"),
  gettext_noop ("Hip-Hop"),
  gettext_noop ("Jazz"),
  gettext_noop ("Metal"),
  gettext_noop ("New Age"),
  gettext_noop ("Oldies"),
  gettext_noop ("Other"),
  gettext_noop ("Pop"),
  gettext_noop ("R&B"),
  gettext_noop ("Rap"),
  gettext_noop ("Reggae"),
  gettext_noop ("Rock"),
  gettext_noop ("Techno"),
  gettext_noop ("Industrial"),
  gettext_noop ("Alternative"),
  gettext_noop ("Ska"),
  gettext_noop ("Death Metal"),
  gettext_noop ("Pranks"),
  gettext_noop ("Soundtrack"),
  gettext_noop ("Euro-Techno"),
  gettext_noop ("Ambient"),
  gettext_noop ("Trip-Hop"),
  gettext_noop ("Vocal"),
  gettext_noop ("Jazz+Funk"),
  gettext_noop ("Fusion"),
  gettext_noop ("Trance"),
  gettext_noop ("Classical"),
  gettext_noop ("Instrumental"),
  gettext_noop ("Acid"),
  gettext_noop ("House"),
  gettext_noop ("Game"),
  gettext_noop ("Sound Clip"),
  gettext_noop ("Gospel"),
  gettext_noop ("Noise"),
  gettext_noop ("Alt. Rock"),
  gettext_noop ("Bass"),
  gettext_noop ("Soul"),
  gettext_noop ("Punk"),
  gettext_noop ("Space"),
  gettext_noop ("Meditative"),
  gettext_noop ("Instrumental Pop"),
  gettext_noop ("Instrumental Rock"),
  gettext_noop ("Ethnic"),
  gettext_noop ("Gothic"),
  gettext_noop ("Darkwave"),
  gettext_noop ("Techno-Industrial"),
  gettext_noop ("Electronic"),
  gettext_noop ("Pop-Folk"),
  gettext_noop ("Eurodance"),
  gettext_noop ("Dream"),
  gettext_noop ("Southern Rock"),
  gettext_noop ("Comedy"),
  gettext_noop ("Cult"),
  gettext_noop ("Gangsta Rap"),
  gettext_noop ("Top 40"),
  gettext_noop ("Christian Rap"),
  gettext_noop ("Pop/Funk"),
  gettext_noop ("Jungle"),
  gettext_noop ("Native American"),
  gettext_noop ("Cabaret"),
  gettext_noop ("New Wave"),
  gettext_noop ("Psychedelic"),
  gettext_noop ("Rave"),
  gettext_noop ("Showtunes"),
  gettext_noop ("Trailer"),
  gettext_noop ("Lo-Fi"),
  gettext_noop ("Tribal"),
  gettext_noop ("Acid Punk"),
  gettext_noop ("Acid Jazz"),
  gettext_noop ("Polka"),
  gettext_noop ("Retro"),
  gettext_noop ("Musical"),
  gettext_noop ("Rock & Roll"),
  gettext_noop ("Hard Rock"),
  gettext_noop ("Folk"),
  gettext_noop ("Folk/Rock"),
  gettext_noop ("National Folk"),
  gettext_noop ("Swing"),
  gettext_noop ("Fast-Fusion"),
  gettext_noop ("Bebob"),
  gettext_noop ("Latin"),
  gettext_noop ("Revival"),
  gettext_noop ("Celtic"),
  gettext_noop ("Bluegrass"),
  gettext_noop ("Avantgarde"),
  gettext_noop ("Gothic Rock"),
  gettext_noop ("Progressive Rock"),
  gettext_noop ("Psychedelic Rock"),
  gettext_noop ("Symphonic Rock"),
  gettext_noop ("Slow Rock"),
  gettext_noop ("Big Band"),
  gettext_noop ("Chorus"),
  gettext_noop ("Easy Listening"),
  gettext_noop ("Acoustic"),
  gettext_noop ("Humour"),
  gettext_noop ("Speech"),
  gettext_noop ("Chanson"),
  gettext_noop ("Opera"),
  gettext_noop ("Chamber Music"),
  gettext_noop ("Sonata"),
  gettext_noop ("Symphony"),
  gettext_noop ("Booty Bass"),
  gettext_noop ("Primus"),
  gettext_noop ("Porn Groove"),
  gettext_noop ("Satire"),
  gettext_noop ("Slow Jam"),
  gettext_noop ("Club"),
  gettext_noop ("Tango"),
  gettext_noop ("Samba"),
  gettext_noop ("Folklore"),
  gettext_noop ("Ballad"),
  gettext_noop ("Power Ballad"),
  gettext_noop ("Rhythmic Soul"),
  gettext_noop ("Freestyle"),
  gettext_noop ("Duet"),
  gettext_noop ("Punk Rock"),
  gettext_noop ("Drum Solo"),
  gettext_noop ("A Cappella"),
  gettext_noop ("Euro-House"),
  gettext_noop ("Dance Hall"),
  gettext_noop ("Goa"),
  gettext_noop ("Drum & Bass"),
  gettext_noop ("Club-House"),
  gettext_noop ("Hardcore"),
  gettext_noop ("Terror"),
  gettext_noop ("Indie"),
  gettext_noop ("BritPop"),
  gettext_noop ("Negerpunk"),
  gettext_noop ("Polsk Punk"),
  gettext_noop ("Beat"),
  gettext_noop ("Christian Gangsta Rap"),
  gettext_noop ("Heavy Metal"),
  gettext_noop ("Black Metal"),
  gettext_noop ("Crossover"),
  gettext_noop ("Contemporary Christian"),
  gettext_noop ("Christian Rock"),
  gettext_noop ("Merengue"),
  gettext_noop ("Salsa"),
  gettext_noop ("Thrash Metal"),
  gettext_noop ("Anime"),
  gettext_noop ("JPop"),
  gettext_noop ("Synthpop"),
};

#define GENRE_NAME_COUNT \
    ((unsigned int)(sizeof genre_names / sizeof (const char *const)))



#define OK         0
#define INVALID_ID3 1

struct id3_state
{
  int state;
  id3tag info;
};

enum ID3State
{
  ID3_INVALID = -1,
  ID3_SEEKING_TO_TAIL = 0,
  ID3_READING_TAIL = 1
};

void
EXTRACTOR_id3_init_state_method (struct EXTRACTOR_PluginList *plugin)
{
  struct id3_state *state;
  state = plugin->state = malloc (sizeof (struct id3_state));
  if (state == NULL)
    return;
  memset (state, 0, sizeof (struct id3_state));
  state->state = ID3_SEEKING_TO_TAIL;
}

void
EXTRACTOR_id3_discard_state_method (struct EXTRACTOR_PluginList *plugin)
{
  struct id3_state *state = plugin->state;
  if (state != NULL)
  {
    if (state->info.title != NULL) free (state->info.title);
    if (state->info.year != NULL) free (state->info.year);
    if (state->info.album != NULL) free (state->info.album);
    if (state->info.artist != NULL) free (state->info.artist);
    if (state->info.comment != NULL) free (state->info.comment);
    free (state);
  }
  plugin->state = NULL;
}

static void
trim (char *k)
{
  if (k == NULL)
    return;
  while ((strlen (k) > 0) && (isspace ((unsigned char) k[strlen (k) - 1])))
    k[strlen (k) - 1] = '\0';
}

static int
get_id3 (const char *data, int64_t offset, int64_t size, id3tag *id3)
{
  const char *pos;

  if (size < 128)
    return INVALID_ID3;

  pos = &data[offset];
  if (0 != strncmp ("TAG", pos, 3))
    return INVALID_ID3;
  pos += 3;

  id3->title = EXTRACTOR_common_convert_to_utf8 (pos, 30, "ISO-8859-1");
  trim (id3->title);
  pos += 30;
  id3->artist = EXTRACTOR_common_convert_to_utf8 (pos, 30, "ISO-8859-1");
  trim (id3->artist);
  pos += 30;
  id3->album = EXTRACTOR_common_convert_to_utf8 (pos, 30, "ISO-8859-1");
  trim (id3->album);
  pos += 30;
  id3->year = EXTRACTOR_common_convert_to_utf8 (pos, 4, "ISO-8859-1");
  trim (id3->year);
  pos += 4;
  id3->comment = EXTRACTOR_common_convert_to_utf8 (pos, 30, "ISO-8859-1");
  trim (id3->comment);
  if ( (pos[28] == '\0') &&
       (pos[29] != '\0') )
    {
      /* ID3v1.1 */
      id3->track_number = pos[29];
    }
  else
    {
      id3->track_number = 0;
    }
  pos += 30;
  id3->genre = "";
  if (pos[0] < GENRE_NAME_COUNT)
    id3->genre = dgettext (PACKAGE, genre_names[(unsigned) pos[0]]);
  return OK;
}


#define ADD(s,t) do { if ( (s != NULL) && (strlen(s) > 0) && (0 != proc (proc_cls, "id3", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1))) return 1; } while (0)


int
EXTRACTOR_id3_extract_method (struct EXTRACTOR_PluginList *plugin,
    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int64_t file_position;
  int64_t file_size;
  int64_t offset = 0;
  int64_t size;
  struct id3_state *state;
  char *data;
  
  char track[16];

  if (plugin == NULL || plugin->state == NULL)
    return 1;

  state = plugin->state;
  file_position = plugin->position;
  file_size = plugin->fsize;
  size = plugin->map_size;
  data = (char *) plugin->shm_ptr;

  if (plugin->seek_request < 0)
    return 1;
  if (file_position - plugin->seek_request > 0)
  {
    plugin->seek_request = -1;
    return 1;
  }
  if (plugin->seek_request - file_position < size)
    offset = plugin->seek_request - file_position;

  while (1)
  {
    switch (state->state)
    {
    case ID3_INVALID:
      plugin->seek_request = -1;
      return 1;
    case ID3_SEEKING_TO_TAIL:
      offset = file_size - 128 - file_position;
      if (offset > size)
      {
        state->state = ID3_READING_TAIL;
        plugin->seek_request = file_position + offset;
        return 0;
      }
      else if (offset < 0)
      {
        state->state = ID3_INVALID;
        break;
      }
      state->state = ID3_READING_TAIL;
       break;
    case ID3_READING_TAIL:
      if (OK != get_id3 (data, offset, size - offset, &state->info))
        return 1;
      ADD (state->info.title, EXTRACTOR_METATYPE_TITLE);
      ADD (state->info.artist, EXTRACTOR_METATYPE_ARTIST);
      ADD (state->info.album, EXTRACTOR_METATYPE_ALBUM);
      ADD (state->info.year, EXTRACTOR_METATYPE_PUBLICATION_YEAR);
      ADD (state->info.genre, EXTRACTOR_METATYPE_GENRE);
      ADD (state->info.comment, EXTRACTOR_METATYPE_COMMENT);
      if (state->info.track_number != 0)
      {
        snprintf(track, 
            sizeof(track), "%u", state->info.track_number);
        ADD (track, EXTRACTOR_METATYPE_TRACK_NUMBER);
      }
      state->state = ID3_INVALID;
    }
  }
  return 1;
}

/* end of id3_extractor.c */
