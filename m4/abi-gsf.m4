# start: abi/ac-helpers/abi-gsf.m4
# 
# Copyright (C) 2005 Christian Neumair
# 
# This file is free software; you may copy and/or distribute it with
# or without modifications, as long as this notice is preserved.
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even
# the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.
#
# The above license applies to THIS FILE ONLY, the GNUnet code
# itself may be copied and distributed under the terms of the GNU
# GPL, see COPYING for more details
#
# Usage: ABI_GSF

# Check for gsf

AC_DEFUN([ABI_GSF], [

test_gsf=true
have_gsf=false

test_gsf_gnome=true
have_gsf_gnome=false

AC_ARG_ENABLE(gsf,[  --disable-gsf Turn off gsf], [
	if test "x$enableval" = "xno"; then
		test_gsf=false
	fi
])

AC_ARG_ENABLE(gsf-gnome,[  --disable-gnome Turn off gsf-gnome], [
	if test "x$enableval" = "xno"; then
		test_gsf_gnome=false
	fi
])

if test "x$test_gsf" = "xtrue" ; then
	PKG_CHECK_MODULES(GSF,[libgsf-1 >= 1.10], [
		have_gsf=true
		GSF_CFLAGS="$GSF_CFLAGS -DHAVE_GSF"
	],
	[
		have_gsf=false
	])
fi

if test "x$have_gsf" = "xtrue" -a "x$test_gsf_gnome" = "xtrue" ; then
	PKG_CHECK_MODULES(GSF_GNOME, [libgsf-gnome-1 >= 1.10], [
		have_gsf_gnome=true
		GSF_GNOME_CFLAGS="$GSF_GNOME_CFLAGS -DHAVE_GSF_GNOME"
	],
	[
		have_gsf_gnome=false
	])
fi

AC_SUBST(GSF_CFLAGS)
AC_SUBST(GSF_LIBS)

AC_SUBST(GSF_GNOME_CFLAGS)
AC_SUBST(GSF_GNOME_LIBS)

AM_CONDITIONAL(WITH_GSF, test "x$have_gsf" = "xtrue")
AM_CONDITIONAL(WITH_GSF_GNOME, test "x$have_gsf_gnome" = "xtrue")

if test "x$have_gsf_gnome" = "xtrue" ; then
	abi_gsf_message="yes, with GNOME support"
        AC_DEFINE(HAVE_GSF, 1, [Have gsf])
else if test "x$have_gsf" = "xtrue" ; then
	abi_gsf_message="yes, without GNOME support"
        AC_DEFINE(HAVE_GSF, 1, [Have gsf])
else
	abi_gsf_message="no"
        AC_DEFINE(HAVE_GSF, 0, [Have gsf])
fi
fi

])
