/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *   This file is part of libextractor.
 *   (C) 2004,2005 Vidyut Samanta and Christian Grothoff
 *
 * gsf-infile-msole.c :
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
#include <glib-object.h>
#include "gsf-input.h"
#include "gsf-infile-msole.h"
#include "gsf-utils.h"

#include <string.h>
#include <stdio.h>

#define OLE_HEADER_SIZE		 0x200	/* independent of big block size size */
#define OLE_HEADER_SIGNATURE	 0x00
#define OLE_HEADER_CLSID	 0x08	/* See ReadClassStg */
#define OLE_HEADER_MINOR_VER	 0x18	/* 0x33 and 0x3e have been seen */
#define OLE_HEADER_MAJOR_VER	 0x1a	/* 0x3 been seen in wild */
#define OLE_HEADER_BYTE_ORDER	 0x1c	/* 0xfe 0xff == Intel Little Endian */
#define OLE_HEADER_BB_SHIFT      0x1e
#define OLE_HEADER_SB_SHIFT      0x20
/* 0x22..0x27 reserved == 0 */
#define OLE_HEADER_CSECTDIR	 0x28
#define OLE_HEADER_NUM_BAT	 0x2c
#define OLE_HEADER_DIRENT_START  0x30
/* 0x34..0x37 transacting signature must be 0 */
#define OLE_HEADER_THRESHOLD	 0x38
#define OLE_HEADER_SBAT_START    0x3c
#define OLE_HEADER_NUM_SBAT      0x40
#define OLE_HEADER_METABAT_BLOCK 0x44
#define OLE_HEADER_NUM_METABAT   0x48
#define OLE_HEADER_START_BAT	 0x4c
#define BAT_INDEX_SIZE		 4
#define OLE_HEADER_METABAT_SIZE	 ((OLE_HEADER_SIZE - OLE_HEADER_START_BAT) / BAT_INDEX_SIZE)

#define DIRENT_MAX_NAME_SIZE	0x40
#define DIRENT_DETAILS_SIZE	0x40
#define DIRENT_SIZE		(DIRENT_MAX_NAME_SIZE + DIRENT_DETAILS_SIZE)
#define DIRENT_NAME_LEN		0x40	/* length in bytes incl 0 terminator */
#define DIRENT_TYPE		0x42
#define DIRENT_COLOUR		0x43
#define DIRENT_PREV		0x44
#define DIRENT_NEXT		0x48
#define DIRENT_CHILD		0x4c
#define DIRENT_CLSID		0x50	/* only for dirs */
#define DIRENT_USERFLAGS	0x60	/* only for dirs */
#define DIRENT_CREATE_TIME	0x64	/* for files */
#define DIRENT_MODIFY_TIME	0x6c	/* for files */
#define DIRENT_FIRSTBLOCK	0x74
#define DIRENT_FILE_SIZE	0x78
/* 0x7c..0x7f reserved == 0 */

#define DIRENT_TYPE_INVALID	0
#define DIRENT_TYPE_DIR		1
#define DIRENT_TYPE_FILE	2
#define DIRENT_TYPE_LOCKBYTES	3	/* ? */
#define DIRENT_TYPE_PROPERTY	4	/* ? */
#define DIRENT_TYPE_ROOTDIR	5
#define DIRENT_MAGIC_END	0xffffffff

/* flags in the block allocation list to denote special blocks */
#define BAT_MAGIC_UNUSED	0xffffffff	/*		   -1 */
#define BAT_MAGIC_END_OF_CHAIN	0xfffffffe	/*		   -2 */
#define BAT_MAGIC_BAT		0xfffffffd	/* a bat block,    -3 */
#define BAT_MAGIC_METABAT	0xfffffffc	/* a metabat block -4 */




typedef struct {
	guint32 *block;
	guint32  num_blocks;
} MSOleBAT;

typedef struct {
	char	 *name;
	char	 *collation_name;
	int	  index;
	size_t    size;
	gboolean  use_sb;
	guint32   first_block;
	gboolean  is_directory;
	GList	 *children;
	unsigned char clsid[16];	/* 16 byte GUID used by some apps */
} MSOleDirent;

typedef struct {
	struct {
		MSOleBAT bat;
		unsigned shift;
		unsigned filter;
		size_t   size;
	} bb, sb;
	off_t max_block;
	guint32 threshold; /* transition between small and big blocks */
        guint32 sbat_start, num_sbat;

	MSOleDirent *root_dir;
	struct GsfInput *sb_file;

	int ref_count;
} MSOleInfo;

typedef struct GsfInfileMSOle {
	off_t size;
	off_t cur_offset;
	struct GsfInput    *input;
	MSOleInfo   *info;
	MSOleDirent *dirent;
	MSOleBAT     bat;
	off_t    cur_block;

	struct {
		guint8  *buf;
		size_t  buf_size;
	} stream;
} GsfInfileMSOle;

/* utility macros */
#define OLE_BIG_BLOCK(index, ole)	((index) >> ole->info->bb.shift)

static struct GsfInput *gsf_infile_msole_new_child (GsfInfileMSOle *parent,
					     MSOleDirent *dirent);

/**
 * ole_get_block :
 * @ole    : the infile
 * @block  :
 * @buffer : optionally NULL
 *
 * Read a block of data from the underlying input.
 * Be really anal.
 **/
static const guint8 *
ole_get_block (const GsfInfileMSOle *ole, guint32 block, guint8 *buffer)
{
	g_return_val_if_fail (block < ole->info->max_block, NULL);

	/* OLE_HEADER_SIZE is fixed at 512, but the sector containing the
	 * header is padded out to bb.size (sector size) when bb.size > 512. */
	if (gsf_input_seek (ole->input,
		(off_t)(MAX (OLE_HEADER_SIZE, ole->info->bb.size) + (block << ole->info->bb.shift)),
		SEEK_SET) < 0)
		return NULL;

	return gsf_input_read (ole->input, ole->info->bb.size, buffer);
}

/**
 * ole_make_bat :
 * @metabat	: a meta bat to connect to the raw blocks (small or large)
 * @size_guess	: An optional guess as to how many blocks are in the file
 * @block	: The first block in the list.
 * @res		: where to store the result.
 *
 * Walk the linked list of the supplied block allocation table and build up a
 * table for the list starting in @block.
 *
 * Returns TRUE on error.
 */
static gboolean
ole_make_bat (MSOleBAT const *metabat, size_t size_guess, guint32 block,
	      MSOleBAT *res)
{
	/* NOTE : Only use size as a suggestion, sometimes it is wrong */
	GArray *bat = g_array_sized_new (FALSE, FALSE,
		sizeof (guint32), size_guess);

	guint8 *used = (guint8*)g_alloca (1 + metabat->num_blocks / 8);
	memset (used, 0, 1 + metabat->num_blocks / 8);

	if (block < metabat->num_blocks)
		do {
			/* Catch cycles in the bat list */
			g_return_val_if_fail (0 == (used[block/8] & (1 << (block & 0x7))), TRUE);
			used[block/8] |= 1 << (block & 0x7);

			g_array_append_val (bat, block);
			block = metabat->block [block];
		} while (block < metabat->num_blocks);

	res->block = NULL;

	res->num_blocks = bat->len;
	res->block = (guint32 *) (gpointer) g_array_free (bat, FALSE);

	if (block != BAT_MAGIC_END_OF_CHAIN) {
#if 0
		g_warning ("This OLE2 file is invalid.\n"
			   "The Block Allocation  Table for one of the streams had %x instead of a terminator (%x).\n"
			   "We might still be able to extract some data, but you'll want to check the file.",
			   block, BAT_MAGIC_END_OF_CHAIN);
#endif
	}

	return FALSE;
}

static void
ols_bat_release (MSOleBAT *bat)
{
	if (bat->block != NULL) {
		g_free (bat->block);
		bat->block = NULL;
		bat->num_blocks = 0;
	}
}

/**
 * ole_info_read_metabat :
 * @ole  :
 * @bats :
 *
 * A small utility routine to read a set of references to bat blocks
 * either from the OLE header, or a meta-bat block.
 *
 * Returns a pointer to the element after the last position filled.
 **/
static guint32 *
ole_info_read_metabat (GsfInfileMSOle *ole, guint32 *bats, guint32 max,
		       guint32 const *metabat, guint32 const *metabat_end)
{
	guint8 const *bat, *end;

	for (; metabat < metabat_end; metabat++) {
		bat = ole_get_block (ole, *metabat, NULL);
		if (bat == NULL)
			return NULL;
		end = bat + ole->info->bb.size;
		for ( ; bat < end ; bat += BAT_INDEX_SIZE, bats++) {
			*bats = GSF_LE_GET_GUINT32 (bat);
			g_return_val_if_fail (*bats < max ||
					      *bats >= BAT_MAGIC_METABAT, NULL);
		}
	}
	return bats;
}

/**
 * gsf_ole_get_guint32s :
 * @dst :
 * @src :
 * @num_bytes :
 *
 * Copy some some raw data into an array of guint32.
 **/
static void
gsf_ole_get_guint32s (guint32 *dst, guint8 const *src, int num_bytes)
{
	for (; (num_bytes -= BAT_INDEX_SIZE) >= 0 ; src += BAT_INDEX_SIZE)
		*dst++ = GSF_LE_GET_GUINT32 (src);
}

static struct GsfInput *
ole_info_get_sb_file (GsfInfileMSOle *parent)
{
	MSOleBAT meta_sbat;

	if (parent->info->sb_file != NULL)
		return parent->info->sb_file;

	parent->info->sb_file = gsf_infile_msole_new_child (parent,
		parent->info->root_dir);

	if (NULL == parent->info->sb_file)
		return NULL;

	g_return_val_if_fail (parent->info->sb.bat.block == NULL, NULL);

	if (ole_make_bat (&parent->info->bb.bat,
			  parent->info->num_sbat, 
                          parent->info->sbat_start, 
                          &meta_sbat)) {
		return NULL;
	}

	parent->info->sb.bat.num_blocks = meta_sbat.num_blocks * (parent->info->bb.size / BAT_INDEX_SIZE);
	parent->info->sb.bat.block	= g_new0 (guint32, parent->info->sb.bat.num_blocks);
	ole_info_read_metabat (parent, parent->info->sb.bat.block,
		parent->info->sb.bat.num_blocks,
		meta_sbat.block, meta_sbat.block + meta_sbat.num_blocks);
	ols_bat_release (&meta_sbat);

	return parent->info->sb_file;
}

static gint
ole_dirent_cmp (const MSOleDirent *a, const MSOleDirent *b)
{
	g_return_val_if_fail (a, 0);
	g_return_val_if_fail (b, 0);

	g_return_val_if_fail (a->collation_name, 0);
	g_return_val_if_fail (b->collation_name, 0);

	return strcmp (b->collation_name, a->collation_name);
}

/**
 * ole_dirent_new :
 * @ole    :
 * @entry  :
 * @parent : optional
 *
 * Parse dirent number @entry and recursively handle its siblings and children.
 **/
static MSOleDirent *
ole_dirent_new (GsfInfileMSOle *ole, guint32 entry, MSOleDirent *parent)
{
	MSOleDirent *dirent;
	guint32 block, next, prev, child, size;
	guint8 const *data;
	guint8 type;
	guint16 name_len;

	if (entry >= DIRENT_MAGIC_END)
		return NULL;

	block = OLE_BIG_BLOCK (entry * DIRENT_SIZE, ole);

	g_return_val_if_fail (block < ole->bat.num_blocks, NULL);
	data = ole_get_block (ole, ole->bat.block [block], NULL);
	if (data == NULL)
		return NULL;
	data += (DIRENT_SIZE * entry) % ole->info->bb.size;

	type = GSF_LE_GET_GUINT8 (data + DIRENT_TYPE);
	if (type != DIRENT_TYPE_DIR &&
	    type != DIRENT_TYPE_FILE &&
	    type != DIRENT_TYPE_ROOTDIR) {
#if 0
		g_warning ("Unknown stream type 0x%x", type);
#endif
		return NULL;
	}

	/* It looks like directory (and root directory) sizes are sometimes bogus */
	size = GSF_LE_GET_GUINT32 (data + DIRENT_FILE_SIZE);
	g_return_val_if_fail (type == DIRENT_TYPE_DIR || type == DIRENT_TYPE_ROOTDIR ||
			      size <= (guint32)gsf_input_size(ole->input), NULL);

	dirent = g_new0 (MSOleDirent, 1);
	dirent->index	     = entry;
	dirent->size	     = size;
	/* Store the class id which is 16 byte identifier used by some apps */
	memcpy(dirent->clsid, data + DIRENT_CLSID, sizeof(dirent->clsid));

	/* root dir is always big block */
	dirent->use_sb	     = parent && (size < ole->info->threshold);
	dirent->first_block  = (GSF_LE_GET_GUINT32 (data + DIRENT_FIRSTBLOCK));
	dirent->is_directory = (type != DIRENT_TYPE_FILE);
	dirent->children     = NULL;
	prev  = GSF_LE_GET_GUINT32 (data + DIRENT_PREV);
	next  = GSF_LE_GET_GUINT32 (data + DIRENT_NEXT);
	child = GSF_LE_GET_GUINT32 (data + DIRENT_CHILD);
	name_len = GSF_LE_GET_GUINT16 (data + DIRENT_NAME_LEN);
	dirent->name = NULL;
	if (0 < name_len && name_len <= DIRENT_MAX_NAME_SIZE) {
		gunichar2 uni_name [DIRENT_MAX_NAME_SIZE+1];
		gchar const *end;
		int i;

		/* !#%!@$#^
		 * Sometimes, rarely, people store the stream name as ascii
		 * rather than utf16.  Do a validation first just in case.
		 */
		if (!g_utf8_validate (data, -1, &end) ||
		    ((guint8 const *)end - data + 1) != name_len) {
			/* be wary about endianness */
			for (i = 0 ; i < name_len ; i += 2)
				uni_name [i/2] = GSF_LE_GET_GUINT16 (data + i);
			uni_name [i/2] = 0;

			dirent->name = g_utf16_to_utf8 (uni_name, -1, NULL, NULL, NULL);
		} else
			dirent->name = g_strndup ((gchar *)data, (gsize)((guint8 const *)end - data + 1));
	}
	/* be really anal in the face of screwups */
	if (dirent->name == NULL)
		dirent->name = g_strdup ("");
	dirent->collation_name = g_utf8_collate_key (dirent->name, -1);

	if (parent != NULL)
		parent->children = g_list_insert_sorted (parent->children,
			dirent, (GCompareFunc)ole_dirent_cmp);

	/* NOTE : These links are a tree, not a linked list */
	if (prev != entry) 
		ole_dirent_new (ole, prev, parent);
	if (next != entry) 
		ole_dirent_new (ole, next, parent);

	if (dirent->is_directory)
		ole_dirent_new (ole, child, dirent);
	return dirent;
}

static void
ole_dirent_free (MSOleDirent *dirent)
{
	GList *tmp;
	g_return_if_fail (dirent != NULL);

	g_free (dirent->name);
	g_free (dirent->collation_name);

	for (tmp = dirent->children; tmp; tmp = tmp->next)
		ole_dirent_free ((MSOleDirent *)tmp->data);
	g_list_free (dirent->children);
	g_free (dirent);
}

/*****************************************************************************/

static void
ole_info_unref (MSOleInfo *info)
{
	if (info->ref_count-- != 1)
		return;

	ols_bat_release (&info->bb.bat);
	ols_bat_release (&info->sb.bat);
	if (info->root_dir != NULL) {
		ole_dirent_free (info->root_dir);
		info->root_dir = NULL;
	}
	if (info->sb_file != NULL)  {
		gsf_input_finalize(info->sb_file);
		info->sb_file = NULL;
	}
	g_free (info);
}

static MSOleInfo *
ole_info_ref (MSOleInfo *info)
{
	info->ref_count++;
	return info;
}

static void
gsf_infile_msole_init (GsfInfileMSOle * ole)
{
	ole->cur_offset = 0;
	ole->size = 0;
	ole->input		= NULL;
	ole->info		= NULL;
	ole->bat.block		= NULL;
	ole->bat.num_blocks	= 0;
	ole->cur_block		= BAT_MAGIC_UNUSED;
	ole->stream.buf		= NULL;
	ole->stream.buf_size	= 0;
}

/**
 * ole_dup :
 * @src :
 *
 * Utility routine to _partially_ replicate a file.  It does NOT copy the bat
 * blocks, or init the dirent.
 *
 * Return value: the partial duplicate.
 **/
static GsfInfileMSOle *
ole_dup (GsfInfileMSOle const * src)
{
	GsfInfileMSOle	*dst;
	struct GsfInput *input;

	g_return_val_if_fail (src != NULL, NULL);

	dst = malloc(sizeof(GsfInfileMSOle));
	if (dst == NULL)
		return NULL;
	gsf_infile_msole_init(dst);
	input = gsf_input_dup (src->input);
	if (input == NULL) {
		gsf_infile_msole_finalize(dst);
		return NULL;
	}
	dst->input = input;
	dst->info  = ole_info_ref (src->info);

	/* buf and buf_size are initialized to NULL */

	return dst;
}
	
/**
 * ole_init_info :
 * @ole :
 *
 * Read an OLE header and do some sanity checking
 * along the way.
 *
 * Return value: TRUE on error 
 **/
static gboolean
ole_init_info (GsfInfileMSOle *ole)
{
	static guint8 const signature[] =
		{ 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
	guint8 const *header, *tmp;
	guint32 *metabat = NULL;
	MSOleInfo *info;
	guint32 bb_shift, sb_shift, num_bat, num_metabat, last, dirent_start;
	guint32 metabat_block, *ptr;

	/* check the header */
	if (gsf_input_seek (ole->input, (off_t) 0, SEEK_SET) ||
	    NULL == (header = gsf_input_read (ole->input, OLE_HEADER_SIZE, NULL)) ||
	    0 != memcmp (header, signature, sizeof (signature))) {
		return TRUE;
	}

	bb_shift      = GSF_LE_GET_GUINT16 (header + OLE_HEADER_BB_SHIFT);
	sb_shift      = GSF_LE_GET_GUINT16 (header + OLE_HEADER_SB_SHIFT);
	num_bat	      = GSF_LE_GET_GUINT32 (header + OLE_HEADER_NUM_BAT);
	dirent_start  = GSF_LE_GET_GUINT32 (header + OLE_HEADER_DIRENT_START);
        metabat_block = GSF_LE_GET_GUINT32 (header + OLE_HEADER_METABAT_BLOCK);
	num_metabat   = GSF_LE_GET_GUINT32 (header + OLE_HEADER_NUM_METABAT);

	/* Some sanity checks
	 * 1) There should always be at least 1 BAT block
	 * 2) It makes no sense to have a block larger than 2^31 for now.
	 *    Maybe relax this later, but not much.
	 */
	if (6 > bb_shift || bb_shift >= 31 || sb_shift > bb_shift) {
		return TRUE;
	}

	info = g_new0 (MSOleInfo, 1);
	ole->info = info;

	info->ref_count	     = 1;
	info->bb.shift	     = bb_shift;
	info->bb.size	     = 1 << info->bb.shift;
	info->bb.filter	     = info->bb.size - 1;
	info->sb.shift	     = sb_shift;
	info->sb.size	     = 1 << info->sb.shift;
	info->sb.filter	     = info->sb.size - 1;
	info->threshold	     = GSF_LE_GET_GUINT32 (header + OLE_HEADER_THRESHOLD);
        info->sbat_start     = GSF_LE_GET_GUINT32 (header + OLE_HEADER_SBAT_START);
        info->num_sbat       = GSF_LE_GET_GUINT32 (header + OLE_HEADER_NUM_SBAT);
	info->max_block	     = (gsf_input_size (ole->input) - OLE_HEADER_SIZE) / info->bb.size;
	info->sb_file	     = NULL;

	if (info->num_sbat == 0 && info->sbat_start != BAT_MAGIC_END_OF_CHAIN) {
#if 0
		g_warning ("There is are not supposed to be any blocks in the small block allocation table, yet there is a link to some.  Ignoring it.");
#endif
	}

	/* very rough heuristic, just in case */
	if (num_bat < info->max_block) {
		info->bb.bat.num_blocks = num_bat * (info->bb.size / BAT_INDEX_SIZE);
		info->bb.bat.block	= g_new0 (guint32, info->bb.bat.num_blocks);

		metabat = (guint32 *)g_alloca (MAX (info->bb.size, OLE_HEADER_SIZE));

		/* Reading the elements invalidates this memory, make copy */
		gsf_ole_get_guint32s (metabat, header + OLE_HEADER_START_BAT,
			OLE_HEADER_SIZE - OLE_HEADER_START_BAT);
		last = num_bat;
		if (last > OLE_HEADER_METABAT_SIZE)
			last = OLE_HEADER_METABAT_SIZE;

		ptr = ole_info_read_metabat (ole, info->bb.bat.block,
			info->bb.bat.num_blocks, metabat, metabat + last);
		num_bat -= last;
	} else
		ptr = NULL;

	last = (info->bb.size - BAT_INDEX_SIZE) / BAT_INDEX_SIZE;
	while (ptr != NULL && num_metabat-- > 0) {
		tmp = ole_get_block (ole, metabat_block, NULL);
		if (tmp == NULL) {
			ptr = NULL;
			break;
		}

		/* Reading the elements invalidates this memory, make copy */
		gsf_ole_get_guint32s (metabat, tmp, (int)info->bb.size);

		if (num_metabat == 0) {
			if (last < num_bat) {
				/* there should be less that a full metabat block
				 * remaining */
				ptr = NULL;
				break;
			}
			last = num_bat;
		} else if (num_metabat > 0) {
			metabat_block = metabat[last];
			num_bat -= last;
		}

		ptr = ole_info_read_metabat (ole, ptr,
			info->bb.bat.num_blocks, metabat, metabat + last);
	}

	if (ptr == NULL) {
		return TRUE;
	}

	/* Read the directory's bat, we do not know the size */
	if (ole_make_bat (&info->bb.bat, 0, dirent_start, &ole->bat)) {
		return TRUE;
	}

	/* Read the directory */
	ole->dirent = info->root_dir = ole_dirent_new (ole, 0, NULL);
	if (ole->dirent == NULL) {
		return TRUE;
	}

	return FALSE;
}

void
gsf_infile_msole_finalize (GsfInfileMSOle * ole)
{
	if (ole->input != NULL) {
		gsf_input_finalize(ole->input);
		ole->input = NULL;
	}
	if (ole->info != NULL) {
		ole_info_unref (ole->info);
		ole->info = NULL;
	}
	ols_bat_release (&ole->bat);

	g_free (ole->stream.buf);
	free(ole);
}
	
static guint8 const *
gsf_infile_msole_read (GsfInfileMSOle *ole, size_t num_bytes, guint8 *buffer)
{
	off_t first_block, last_block, raw_block, offset, i;
	guint8 const *data;
	guint8 *ptr;
	size_t count;

	/* small block files are preload */
	if (ole->dirent != NULL && ole->dirent->use_sb) {
		if (buffer != NULL) {
			memcpy (buffer, ole->stream.buf + ole->cur_offset, num_bytes);
			ole->cur_offset += num_bytes;
			return buffer;
		}
		data = ole->stream.buf + ole->cur_offset;
		ole->cur_offset += num_bytes;
		return data;
	}

	/* GsfInput guarantees that num_bytes > 0 */
	first_block = OLE_BIG_BLOCK (ole->cur_offset, ole);
	last_block = OLE_BIG_BLOCK (ole->cur_offset + num_bytes - 1, ole);
	offset = ole->cur_offset & ole->info->bb.filter;

	/* optimization : are all the raw blocks contiguous */
	i = first_block;
	raw_block = ole->bat.block [i];
	while (++i <= last_block && ++raw_block == ole->bat.block [i])
		;
	if (i > last_block) {
		/* optimization don't seek if we don't need to */
		if (ole->cur_block != first_block) {
			if (gsf_input_seek (ole->input,
				(off_t)(MAX (OLE_HEADER_SIZE, ole->info->bb.size) + (ole->bat.block [first_block] << ole->info->bb.shift) + offset),
				SEEK_SET) < 0)
				return NULL;
		}
		ole->cur_block = last_block;
		return gsf_input_read (ole->input, num_bytes, buffer);
	}

	/* damn, we need to copy it block by block */
	if (buffer == NULL) {
		if (ole->stream.buf_size < num_bytes) {
			if (ole->stream.buf != NULL)
				g_free (ole->stream.buf);
			ole->stream.buf_size = num_bytes;
			ole->stream.buf = g_new (guint8, num_bytes);
		}
		buffer = ole->stream.buf;
	}

	ptr = buffer;
	for (i = first_block ; i <= last_block ; i++ , ptr += count, num_bytes -= count) {
		count = ole->info->bb.size - offset;
		if (count > num_bytes)
			count = num_bytes;
		data = ole_get_block (ole, ole->bat.block [i], NULL);
		if (data == NULL)
			return NULL;

		/* TODO : this could be optimized to avoid the copy */
		memcpy (ptr, data + offset, count);
		offset = 0;
	}
	ole->cur_block = BAT_MAGIC_UNUSED;
	ole->cur_offset += num_bytes;
	return buffer;
}
	
static struct GsfInput *
gsf_infile_msole_new_child (GsfInfileMSOle *parent,
			    MSOleDirent *dirent)
{
	GsfInfileMSOle * child;
	MSOleInfo *info;
	MSOleBAT const *metabat;
	struct GsfInput *sb_file = NULL;
	size_t size_guess;
	char * buf;
	

	if ( (dirent->index != 0) &&
	     (dirent->is_directory) ) {
		/* be wary.  It seems as if some implementations pretend that the
		 * directories contain data */
		return gsf_input_new("",
				     (off_t) 0,
				     0);
	}
	child = ole_dup (parent);
	if (child == NULL) 
		return NULL;	
	child->dirent = dirent;
	child->size = (off_t) dirent->size;
		
	info = parent->info;

        if (dirent->use_sb) {	/* build the bat */
		metabat = &info->sb.bat;
		size_guess = dirent->size >> info->sb.shift;
		sb_file = ole_info_get_sb_file (parent);
	} else {
		metabat = &info->bb.bat;
		size_guess = dirent->size >> info->bb.shift;
	}
	if (ole_make_bat (metabat, size_guess + 1, dirent->first_block, &child->bat)) {
		gsf_infile_msole_finalize(child);
		return NULL;
	}

	if (dirent->use_sb) {
		unsigned i;
		guint8 const *data;
		
		if (sb_file == NULL) {
			gsf_infile_msole_finalize(child);
			return NULL;
		}

		child->stream.buf_size = info->threshold;
		child->stream.buf = g_new (guint8, info->threshold);

		for (i = 0 ; i < child->bat.num_blocks; i++)
			if (gsf_input_seek (sb_file,
					    (off_t)(child->bat.block [i] << info->sb.shift), SEEK_SET) < 0 ||
			    (data = gsf_input_read (sb_file,
						    info->sb.size,
				child->stream.buf + (i << info->sb.shift))) == NULL) {
				gsf_infile_msole_finalize(child);
				return NULL;
			}
	}
	buf = malloc(child->size);
	if (buf == NULL) {
		gsf_infile_msole_finalize(child);
		return NULL;
	}
	if (NULL == gsf_infile_msole_read(child,
					  child->size,
					  buf)) {
		gsf_infile_msole_finalize(child);	
		return NULL;
	}
	gsf_infile_msole_finalize(child);
	return gsf_input_new(buf,
			     (off_t) dirent->size,
			     1);
}
	

struct GsfInput *
gsf_infile_msole_child_by_index (GsfInfileMSOle * ole, int target)
{
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return gsf_infile_msole_new_child (ole,
				(MSOleDirent *)p->data);
	return NULL;
}

char const *
gsf_infile_msole_name_by_index (GsfInfileMSOle * ole, int target)
{
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return ((MSOleDirent *)p->data)->name;
	return NULL;
}

int
gsf_infile_msole_num_children (GsfInfileMSOle * ole)
{
	g_return_val_if_fail (ole->dirent != NULL, -1);

	if (!ole->dirent->is_directory)
		return -1;
	return g_list_length (ole->dirent->children);
}


/**
 * gsf_infile_msole_new :
 * @source :
 *
 * Opens the root directory of an MS OLE file.
 * NOTE : adds a reference to @source
 *
 * Returns : the new ole file handler
 **/
GsfInfileMSOle *
gsf_infile_msole_new (struct GsfInput *source)
{
	GsfInfileMSOle * ole;

	ole = malloc(sizeof(GsfInfileMSOle));
	if (ole == NULL)
		return NULL;
	gsf_infile_msole_init(ole);
	ole->input = source;
	ole->size = (off_t) 0;

	if (ole_init_info (ole)) {
		gsf_infile_msole_finalize(ole);
		return NULL;
	}

	return ole;
}

/**
 * gsf_infile_msole_get_class_id :
 * @ole: a #GsfInfileMSOle
 * @res: 16 byte identifier (often a GUID in MS Windows apps)
 *
 * Retrieves the 16 byte indentifier (often a GUID in MS Windows apps)
 * stored within the directory associated with @ole and stores it in @res.
 *
 * Returns TRUE on success
 **/
int
gsf_infile_msole_get_class_id (const GsfInfileMSOle *ole, 
                               unsigned char * res)
{
	g_return_val_if_fail (ole != NULL && ole->dirent != NULL, 0);

	memcpy (res, ole->dirent->clsid,
		sizeof(ole->dirent->clsid));
	return 1;
}
