/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2005, 2006, 2009, 2012 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */
/**
 * @file main/extractor_plugpath.h
 * @brief determine path where plugins are installed
 * @author Christian Grothoff
 */
#ifndef EXTRACTOR_PLUGPATH_H
#define EXTRACTOR_PLUGPATH_H

/**
 * Given a short name of a library (i.e. "mime"), find
 * the full path of the respective plugin.
 */
char * 
EXTRACTOR_find_plugin_ (const char *short_name);


#endif 
/* EXTRACTOR_PLUGPATH_H */
