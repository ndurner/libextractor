/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-utils.h:
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

#ifndef GSF_UTILS_H
#define GSF_UTILS_H

#include <glib-object.h>
void gsf_le_set_float  (void *p, float f);
void gsf_le_set_double (void *p, double d);


char const *gsf_extension_pointer (char const * path);
void	    gsf_iconv_close (GIConv handle);

#undef g_return_val_if_fail
#define g_return_val_if_fail(cond,ret) do{if (!(cond)) return ret;}while(0)


#endif /* GSF_UTILS_H */
