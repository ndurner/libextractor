/*
     This file is part of libextractor.
     Copyright (C) 2012 Vidyut Samanta and Christian Grothoff

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
 * @file main/extractor_plugin_main.c
 * @brief main loop for an out-of-process plugin
 * @author Christian Grothoff
 */
#ifndef EXTRACTOR_PLUGIN_MAIN_H
#define EXTRACTOR_PLUGIN_MAIN_H

#include "extractor.h"


/**
 * 'main' function of the child process. Loads the plugin,
 * sets up its in and out pipes, then runs the request serving function.
 *
 * @param plugin extractor plugin to use
 * @param in stream to read from
 * @param out stream to write to
 */
void
EXTRACTOR_plugin_main_ (struct EXTRACTOR_PluginList *plugin, 
			int in, int out);

#endif
