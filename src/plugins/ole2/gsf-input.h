/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input.h: interface for used by the ole layer to read raw data
 *
 * Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GSF_INPUT_H
#define GSF_INPUT_H

#include <sys/types.h>
#include <unistd.h>

struct GsfInput;

/**
 * gsf_input_memory_new:
 * @buf: The input bytes
 * @length: The length of @buf
 * @needs_free: Whether you want this memory to be free'd at object destruction
 *
 * Returns: A new #GsfInputMemory
 */
struct GsfInput *
gsf_input_new (const unsigned char *buf, 
	       off_t length, 
	       int needs_free);

char const   *    gsf_input_name      (struct GsfInput *input);
struct GsfInput * gsf_input_dup	      (struct GsfInput *input);
void              gsf_input_finalize  (struct GsfInput *input);
struct GsfInput * gsf_input_sibling   (const struct GsfInput *input, char const *name);
off_t             gsf_input_size      (struct GsfInput *input);
int               gsf_input_eof	      (struct GsfInput *input);
const unsigned char * gsf_input_read  (struct GsfInput *input, 
                                       size_t num_bytes,
				       unsigned char * optional_buffer);
off_t             gsf_input_remaining (struct GsfInput *input);
off_t             gsf_input_tell      (struct GsfInput *input);
int               gsf_input_seek      (struct GsfInput *input,
				       off_t offset, 
                                       int whence);
int               gsf_input_set_name	(struct GsfInput *input, char const *name);
int               gsf_input_set_size	(struct GsfInput *input, off_t size);


#endif /* GSF_INPUT_H */
