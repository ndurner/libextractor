/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */
#ifndef EXTRACTOR_IPC_H
#define EXTRACTOR_IPC_H

/**
 * Definition of an IPC communication channel with
 * some plugin.
 */
struct EXTRACTOR_Channel;


/**
 * Create a channel to communicate with a process wrapping
 * the plugin of the given name.  Starts the process as well.
 *
 * @param short_libname name of the plugin
 * @return NULL on error, otherwise IPC channel
 */ 
struct EXTRACTOR_Channel *
EXTRACTOR_IPC_channel_create_ (const char *short_libname);


/**
 * Destroy communication channel with a plugin/process.  Also
 * destroys the process.
 *
 * @param channel channel to communicate with the plugin
 */
void
EXTRACTOR_IPC_channel_destroy_ (struct EXTRACTOR_Channel *channel);


/**
 * Map the given buffer
 *
 * @param channel channel to communicate with the plugin
 */
void
EXTRACTOR_IPC_channel_xxx_ (struct EXTRACTOR_Channel *channel);



#endif
