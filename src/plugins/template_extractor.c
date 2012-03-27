/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2009 Vidyut Samanta and Christian Grothoff

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

#include "extractor_plugins.h"

struct template_state
{
  int state;

  /* more state fields here
   * all variables that should survive more than one atomic read
   * from the "file" are to be placed here.
   */
};

enum TemplateState
{
  TEMPLATE_INVALID = -1,
  TEMPLATE_LOOKING_FOR_FOO = 0,
  TEMPLATE_READING_FOO,
  TEMPLATE_READING_BAR,
  TEMPLATE_SEEKING_TO_ZOOL
};

void
EXTRACTOR_template_init_state_method (struct EXTRACTOR_PluginList *plugin)
{
  struct template_state *state;
  state = plugin->state = malloc (sizeof (struct template_state));
  if (state == NULL)
    return;
  state->state = TEMPLATE_LOOKING_FOR_FOO; /* or whatever is the initial one */
  /* initialize other fields to their "uninitialized" values or defaults */
}

void
EXTRACTOR_template_discard_state_method (struct EXTRACTOR_PluginList *plugin)
{
  if (plugin->state != NULL)
  {
    /* free other state fields that are heap-allocated */
    free (plugin->state);
  }
  plugin->state = NULL;
}

int
EXTRACTOR_template_extract_method (struct EXTRACTOR_PluginList *plugin,
    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int64_t file_position;
  int64_t file_size;
  size_t offset = 0;
  size_t size;
  unsigned char *data;
  unsigned char *ff;
  struct mp3_state *state;

  /* temporary variables are declared here */

  if (plugin == NULL || plugin->state == NULL)
    return 1;

  /* for easier access (and conforms better with the old plugins var names) */
  state = plugin->state;
  file_position = plugin->position;
  file_size = plugin->fsize;
  size = plugin->map_size;
  data = plugin->shm_ptr;

  /* sanity checks */
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
    case TEMPLATE_INVALID:
      plugin->seek_request = -1;
      return 1;
    case TEMPLATE_LOOKING_FOR_FOO:
      /* Find FOO in data buffer.
       * If found, set offset to its position and set state to TEMPLATE_READING_FOO
       * If not found, set seek_request to file_position + offset and return 1
       * (but it's better to give up as early as possible, to avoid reading the whole
       * file byte-by-byte).
       */ 
      break;
    case TEMPLATE_READING_FOO:
      /* See if offset + sizeof(foo) < size, otherwise set seek_request to offset and return 1;
       * If file_position is 0, and size is still to small, give up.
       * Read FOO, maybe increase offset to reflect that (depends on the parser logic).
       * Either process FOO right here, or jump to another state (see ebml plugin for an example of complex
       * state-jumps).
       * If FOO says you need to seek somewhere - set offset to seek_target - file_position and set the
       * next state (next state will check that offset < size; all states that do reading should do that,
       * and also check for EOF).
       */
      /* ... */
      break;
    }
  }
  /* Should not reach this */
  return 1;
}
