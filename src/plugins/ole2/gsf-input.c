/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input.c: interface for used by the ole layer to read raw data
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

#include "platform.h"
#include "gsf-input.h"
#include "gsf-utils.h"
#include <string.h>

typedef struct GsfInput {
	off_t size;
	off_t cur_offset;
	char * name;
	const unsigned char * buf;
	int needs_free;
} GsfInput;


static void
gsf_input_init (GsfInput * input)
{
	input->size = 0;
	input->cur_offset = 0;
	input->name = NULL;
	input->buf = NULL;
}

/**
 * gsf_input_memory_new:
 * @buf: The input bytes
 * @length: The length of @buf
 * @needs_free: Whether you want this memory to be free'd at object destruction
 *
 * Returns: A new #GsfInputMemory
 */
GsfInput *
gsf_input_new (const unsigned char * buf,
	       off_t length,
	       int needs_free)
{
	GsfInput *mem = malloc(sizeof(GsfInput));
	if (mem == NULL)
		return NULL;
	gsf_input_init(mem);
	mem->buf = buf;
	mem->size = length;
	mem->needs_free = needs_free;
	return mem;
}

void
gsf_input_finalize (GsfInput * input)
{
	if (input->name != NULL) {
		free (input->name);
		input->name = NULL;
	}
	if ( (input->buf) && input->needs_free)
		free((void*) input->buf);
	free(input);
}

GsfInput *
gsf_input_dup (GsfInput *src)
{
	GsfInput * dst = malloc(sizeof(GsfInput));
	if (dst == NULL)
		return NULL;
        gsf_input_init(dst);
	dst->buf = src->buf;
	dst->needs_free = 0;
	dst->size = src->size;
	if (src->name != NULL)
		gsf_input_set_name (dst, src->name);
	dst->cur_offset = src->cur_offset;
	return dst;
}

const unsigned char *
gsf_input_read (GsfInput * mem, size_t num_bytes, unsigned char * optional_buffer)
{
	const unsigned char *src = mem->buf;
	if (src == NULL)
		return NULL;
	if (optional_buffer) {
		memcpy (optional_buffer, src + mem->cur_offset, num_bytes);
		mem->cur_offset += num_bytes;

		return optional_buffer;
	} else {
		const unsigned char * ret = src + mem->cur_offset;
		mem->cur_offset += num_bytes;
		return ret;
	}
}

/**
 * gsf_input_name :
 * @input :
 *
 * Returns @input's name in utf8 form, DO NOT FREE THIS STRING
 **/
const char *
gsf_input_name (GsfInput *input)
{
	return input->name;
}

/**
 * gsf_input_size :
 * @input : The input
 *
 * Looks up and caches the number of bytes in the input
 *
 * Returns :  the size or -1 on error
 **/
off_t
gsf_input_size (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, -1);
	return input->size;
}

/**
 * gsf_input_eof :
 * @input : the input
 *
 * Are we at the end of the file ?
 *
 * Returns : TRUE if the input is at the eof.
 **/
int
gsf_input_eof (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->cur_offset >= input->size;
}

/**
 * gsf_input_remaining :
 * @input :
 *
 * Returns the number of bytes left in the file.
 **/
off_t
gsf_input_remaining (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->size - input->cur_offset;
}

/**
 * gsf_input_tell :
 * @input :
 *
 * Returns the current offset in the file.
 **/
off_t
gsf_input_tell (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->cur_offset;
}

/**
 * gsf_input_seek :
 * @input :
 * @offset :
 * @whence :
 *
 * Returns TRUE on error.
 **/
int
gsf_input_seek (GsfInput *input, off_t offset, int whence)
{
	off_t pos = offset;

	g_return_val_if_fail (input != NULL, 1);

	switch (whence) {
	case SEEK_SET : break;
	case SEEK_CUR : pos += input->cur_offset;	break;
	case SEEK_END : pos += input->size;		break;
	default : return 1;
	}

	if (pos < 0 || pos > input->size)
		return 1;

	/*
	 * If we go nowhere, just return.  This in particular handles null
	 * seeks for streams with no seek method.
	 */
	if (pos == input->cur_offset)
		return 0;

	input->cur_offset = pos;
	return 0;
}

/**
 * gsf_input_set_name :
 * @input :
 * @name :
 *
 * protected.
 *
 * Returns : TRUE if the assignment was ok.
 **/
int
gsf_input_set_name (GsfInput *input, char const *name)
{
	char *buf;

	g_return_val_if_fail (input != NULL, 0);

	buf = strdup (name);
	if (input->name != NULL)
		free (input->name);
	input->name = buf;
	return 1;
}

/**
 * gsf_input_set_size :
 * @input :
 * @size :
 *
 * Returns : TRUE if the assignment was ok.
 */
int
gsf_input_set_size (GsfInput *input, off_t size)
{
	g_return_val_if_fail (input != NULL, 0);

	input->size = size;
	return 1;
}

