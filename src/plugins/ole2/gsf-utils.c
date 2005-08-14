/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-utils.c:
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
#include "gsf-utils.h"
#include "gsf-input.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * Glib gets this wrong, really.  ARM's floating point format is a weird
 * mixture.
 */
#define G_ARMFLOAT_ENDIAN 56781234
#if defined(__arm__) && !defined(__vfp__) && (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define G_FLOAT_BYTE_ORDER G_ARMFLOAT_ENDIAN
#else
#define G_FLOAT_BYTE_ORDER G_BYTE_ORDER
#endif

guint64
gsf_le_get_guint64 (void const *p)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (guint64) == 8) {
		guint64 li;
		int     i;
		guint8 *t  = (guint8 *)&li;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (li);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return li;
	} else {
		g_error ("Big endian machine, but weird size of guint64");
	}
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (guint64) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		guint64 data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of guint64");
	}
#else
#error "Byte order not recognised -- out of luck"
#endif
}

float
gsf_le_get_float (void const *p)
{
#if G_FLOAT_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (float) == 4) {
		float   f;
		int     i;
		guint8 *t  = (guint8 *)&f;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (f);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return f;
	} else {
		g_error ("Big endian machine, but weird size of floats");
	}
#elif (G_FLOAT_BYTE_ORDER == G_LITTLE_ENDIAN) || (G_FLOAT_BYTE_ORDER == G_ARMFLOAT_ENDIAN)
	if (sizeof (float) == 4) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		float data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of floats");
	}
#else
#error "Floating-point byte order not recognised -- out of luck"
#endif
}

void
gsf_le_set_float (void *p, float d)
{
#if G_FLOAT_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (float) == 4) {
		int     i;
		guint8 *t  = (guint8 *)&d;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (d);

		for (i = 0; i < sd; i++)
			p2[sd - 1 - i] = t[i];
	} else {
		g_error ("Big endian machine, but weird size of floats");
	}
#elif (G_FLOAT_BYTE_ORDER == G_LITTLE_ENDIAN) || (G_FLOAT_BYTE_ORDER == G_ARMFLOAT_ENDIAN)
	if (sizeof (float) == 4) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		memcpy (p, &d, sizeof (d));
	} else {
		g_error ("Little endian machine, but weird size of floats");
	}
#else
#error "Floating-point byte order not recognised -- out of luck"
#endif
}

double
gsf_le_get_double (void const *p)
{
#if G_FLOAT_BYTE_ORDER == G_ARMFLOAT_ENDIAN
	double data;
	memcpy ((char *)&data + 4, p, 4);
	memcpy ((char *)&data, (const char *)p + 4, 4);
	return data;
#elif G_FLOAT_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (double) == 8) {
		double  d;
		int     i;
		guint8 *t  = (guint8 *)&d;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (d);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return d;
	} else {
		g_error ("Big endian machine, but weird size of doubles");
	}
#elif G_FLOAT_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (double) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		double data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of doubles");
	}
#else
#error "Floating-point byte order not recognised -- out of luck"
#endif
}

void
gsf_le_set_double (void *p, double d)
{
#if G_FLOAT_BYTE_ORDER == G_ARMFLOAT_ENDIAN
	memcpy (p, (const char *)&d + 4, 4);
	memcpy ((char *)p + 4, &d, 4);
#elif G_FLOAT_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (double) == 8) {
		int     i;
		guint8 *t  = (guint8 *)&d;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (d);

		for (i = 0; i < sd; i++)
			p2[sd - 1 - i] = t[i];
	} else {
		g_error ("Big endian machine, but weird size of doubles");
	}
#elif G_FLOAT_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (double) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		memcpy (p, &d, sizeof (d));
	} else {
		g_error ("Little endian machine, but weird size of doubles");
	}
#else
#error "Floating-point byte order not recognised -- out of luck"
#endif
}

/**
 * gsf_extension_pointer:
 * @path: A filename or file path.
 *
 * Extracts the extension from the end of a filename (the part after the final
 * '.' in the filename).
 *
 * Returns: A pointer to the extension part of the filename, or a
 * pointer to the end of the string if the filename does not
 * have an extension.
 */
char const *
gsf_extension_pointer (char const *path)
{
	char *s, *t;
	
	g_return_val_if_fail (path != NULL, NULL);

	t = strrchr (path, G_DIR_SEPARATOR);
	s = strrchr ((t != NULL) ? t : path, '.');
	if (s != NULL)
		return s + 1;
	return path + strlen(path);
}

/**
 * gsf_iconv_close : A utility wrapper to safely close an iconv handle
 * @handle :
 **/
void
gsf_iconv_close (GIConv handle)
{
	if (handle != NULL && handle != ((GIConv)-1))
		g_iconv_close (handle);
}

/* FIXME: what about translations?  */
#ifndef _
#define _(x) x
#endif

