/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-memory.c:
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
#include <string.h>
#include "gsf-input-memory.h"
#include "gsf-input-impl.h"
#include "gsf-impl-utils.h"
#include "gsf-utils.h"
#include "gsf-shared-memory.h"

static GObjectClass *parent_class;

struct _GsfInputMemory {
	GsfInput parent;
	GsfSharedMemory *shared;
};
typedef GsfInputClass GsfInputMemoryClass;

/**
 * gsf_input_memory_new:
 * @buf: The input bytes
 * @length: The length of @buf
 * @needs_free: Whether you want this memory to be free'd at object destruction
 *
 * Returns: A new #GsfInputMemory
 */
GsfInput *
gsf_input_memory_new (guint8 const *buf, gsf_off_t length, gboolean needs_free)
{
	GsfInputMemory *mem = g_object_new (GSF_INPUT_MEMORY_TYPE, NULL);
	if (mem == NULL)
		return NULL;
	mem->shared = gsf_shared_memory_new ((void *)buf, length, needs_free);
	gsf_input_set_size (GSF_INPUT (mem), length);
	return GSF_INPUT (mem);
}

/**
 * gsf_input_memory_new_clone:
 * @buf: The input bytes
 * @length: The length of @buf
 *
 * Returns: A new #GsfInputMemory
 */
GsfInput *
gsf_input_memory_new_clone (guint8 const *buf, gsf_off_t length)
{	
	GsfInputMemory *mem = NULL;
	guint8 * cpy = g_try_malloc (length * sizeof (guint8));
	if (cpy == NULL)
		return NULL;

	memcpy (cpy, buf, length);
	mem = g_object_new (GSF_INPUT_MEMORY_TYPE, NULL);
	if (mem == NULL)
		return NULL;
	mem->shared = gsf_shared_memory_new ((void *)cpy, length, TRUE);
	gsf_input_set_size (GSF_INPUT (mem), length);
	return GSF_INPUT (mem);
}

static void
gsf_input_memory_finalize (GObject *obj)
{
	GsfInputMemory *mem = (GsfInputMemory *) (obj);

	if (mem->shared)
		g_object_unref (G_OBJECT (mem->shared));

	parent_class->finalize (obj);
}

static GsfInput *
gsf_input_memory_dup (GsfInput *src_input, GError **err)
{
	GsfInputMemory const *src = (GsfInputMemory *) (src_input);
	GsfInputMemory *dst = g_object_new (GSF_INPUT_MEMORY_TYPE, NULL);
	if (dst == NULL)
		return NULL;
	(void) err;

	dst->shared = src->shared;
	g_object_ref (G_OBJECT (dst->shared));

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_input_memory_read (GsfInput *input, size_t num_bytes, guint8 *optional_buffer)
{
	GsfInputMemory *mem = (GsfInputMemory *) (input);
	guchar const *src = mem->shared->buf;

	if (src == NULL)
		return NULL;
	if (optional_buffer) {
		memcpy (optional_buffer, src + input->cur_offset, num_bytes);
		return optional_buffer;
	} else
		return src + input->cur_offset;
}

static gboolean
gsf_input_memory_seek (G_GNUC_UNUSED GsfInput *input,
		       G_GNUC_UNUSED gsf_off_t offset,
		       G_GNUC_UNUSED GSeekType whence)
{
	return FALSE;
}

static void
gsf_input_memory_init (GObject *obj)
{
	GsfInputMemory *mem = (GsfInputMemory *) (obj);
	mem->shared = NULL;

}

static void
gsf_input_memory_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_memory_finalize;
	input_class->Dup	= gsf_input_memory_dup;
	input_class->Read	= gsf_input_memory_read;
	input_class->Seek	= gsf_input_memory_seek;

	parent_class = g_type_class_peek_parent (gobject_class);
}

GSF_CLASS (GsfInputMemory, gsf_input_memory,
	   gsf_input_memory_class_init, gsf_input_memory_init,
	   GSF_INPUT_TYPE)


