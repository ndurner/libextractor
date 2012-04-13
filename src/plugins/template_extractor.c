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
#include "le_architecture.h"

int
EXTRACTOR_template_extract_method (struct EXTRACTOR_PluginList *plugin,
    EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  int64_t offset;
  unsigned char *data;

  /* temporary variables are declared here */

  if (plugin == NULL)
    return 1;

  /* initialize state here */

  /* Call pl_seek (plugin, POSITION, WHENCE) to seek (if you know where
   * data starts.
   */
  /* Call pl_read (plugin, &data, COUNT) to read COUNT bytes (will be stored
   * as data[0]..data[COUNT-1], no need to allocate data or free it; but it
   * "goes away" when you make another read call, so store interesting values
   * somewhere once you find them).
   */
  /* If you need to search for a magic id that is not at the beginning of the
   * file, do pl_read() calls, reading sizable (1 megabyte or so) chunks,
   * then use memchr() on them to find first byte of the magic sequence,
   * then compare the rest of the sequence, if found.
   * Mind the fact that you need to iterate over COUNT - SEQUENCE_LENGTH chars,
   * and seek to POS + COUNT - SEQUENCE_LENGTH once you run out of bytes,
   * otherwise you'd have a chance to skip bytes at chunk boundaries.
   */
  /* Do try to make a reasonable assumption about the amount of data you're
   * going to search through. Iterating over the whole file, byte-by-byte is
   * NOT a good idea, if the search itself is slow. Try to make the search as
   * efficient as possible.
   */
  /* Avoid making long seeks backwards (for performance reasons)
   */
  /* pl_get_pos (plugin) will return current offset from the beginning of
   * the file (i.e. index of the data[0] in the file, if you call pl_read
   * at that point). You might need it do calculate forward-searches, if
   * there are offsets stored within the file.
   * pl_get_fsize (plugin) will return file size OR -1 if it is not known
   * yet (file is not decompressed completely). Don't rely on fsize.
   */
  /* Seeking forward is safe
   */
  /* If you asked to read X bytes, but got less - it's EOF
   */
  /* Seeking backward a bit shouldn't hurt performance (i.e. read 4 bytes,
   * then immediately seek 4 bytes back).
   */
  /* Don't read too much (you can't read more than MAX_READ from extractor.c,
   * which is 32MB at the moment) in one call.
   */
  /* Once you find something, call proc(). If it returns non-0 - you're done.
   */
  /* Return 1 to indicate that you're done. */
  /* Don't forget to free anything you've allocated before returning! */
  return 1;
}
