/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-msole.h:
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

#ifndef GSF_INFILE_MSOLE_H
#define GSF_INFILE_MSOLE_H

#include "gsf-input.h"

struct GsfInfileMSOle;

struct GsfInfileMSOle * gsf_infile_msole_new (struct GsfInput *source);

int
gsf_infile_msole_num_children (struct GsfInfileMSOle *infile);

struct GsfInput *
gsf_infile_msole_child_by_index (struct GsfInfileMSOle *infile, int target);

char const *
gsf_infile_msole_name_by_index (struct GsfInfileMSOle *infile, int target);


void
gsf_infile_msole_finalize (struct GsfInfileMSOle * ole);

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
gsf_infile_msole_get_class_id (const struct GsfInfileMSOle *ole,
                               unsigned char *res);

#endif /* GSF_INFILE_MSOLE_H */
