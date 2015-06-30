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
 * @file main/extractor_plugins.h
 * @brief code to load plugins
 * @author Christian Grothoff
 */
#ifndef EXTRACTOR_PLUGINS_H
#define EXTRACTOR_PLUGINS_H

#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#include <signal.h>
#include <ltdl.h>


/**
 * Linked list of extractor plugins.  An application builds this list
 * by telling libextractor to load various meta data extraction
 * plugins.  Plugins can also be unloaded (removed from this list,
 * see EXTRACTOR_plugin_remove).
 */
struct EXTRACTOR_PluginList
{
  /**
   * This is a linked list.
   */
  struct EXTRACTOR_PluginList *next;

  /**
   * Pointer to the plugin (as returned by lt_dlopen).
   */
  void *libraryHandle;

  /**
   * Name of the library (i.e., 'libextractor_foo.so')
   */
  char *libname;

  /**
   * Short name of the plugin (i.e., 'foo')
   */
  char *short_libname;
  
  /**
   * Pointer to the function used for meta data extraction.
   */
  EXTRACTOR_extract_method extract_method;

  /**
   * Options for the plugin.
   */
  char *plugin_options;

  /**
   * Special options for the plugin
   * (as returned by the plugin's "options" method;
   * typically NULL).
   */
  const char *specials;

  /**
   * Channel to communicate with out-of-process plugin, NULL if not setup.
   */
  struct EXTRACTOR_Channel *channel;

  /**
   * Memory segment shared with the channel of this plugin, NULL for none.
   */
  struct EXTRACTOR_SharedMemory *shm;

  /**
   * A position this plugin wants us to seek to. -1 if it's finished.
   * A positive value from the end of the file is used of 'whence' is
   * SEEK_END; a postive value from the start is used of 'whence' is
   * SEEK_SET.  'SEEK_CUR' is never used.
   */
  int64_t seek_request;

  /**
   * Flags to control how the plugin is executed.
   */
  enum EXTRACTOR_Options flags;

  /**
   * Is this plugin finished extracting for this round?
   * 0: no, 1: yes
   */
  int round_finished;

  /**
   * 'whence' value for the seek operation;
   * 0 = SEEK_SET, 1 = SEEK_CUR, 2 = SEEK_END.
   * Note that 'SEEK_CUR' is never used here.
   */
  uint16_t seek_whence;

};


/**
 * Load a plugin.
 *
 * @param plugin plugin to load
 * @return 0 on success, -1 on error
 */
int
EXTRACTOR_plugin_load_ (struct EXTRACTOR_PluginList *plugin);

#endif /* EXTRACTOR_PLUGINS_H */
