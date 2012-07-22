/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

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
/**
 * @file main/extractor_ipc.h
 * @brief IPC with plugin (OS-independent API)
 * @author Christian Grothoff
 */
#ifndef EXTRACTOR_IPC_H
#define EXTRACTOR_IPC_H

/**
 * Definition of an IPC communication channel with
 * some plugin.
 */
struct EXTRACTOR_Channel;

/**
 * Definition of a shared memory area.
 */
struct EXTRACTOR_SharedMemory;


/**
 * Header used for our IPC replies.  A header
 * with all fields being zero is used to indicate
 * the end of the stream.
 */
struct IpcHeader
{
  /**
   * Type of the meta data.
   */
  enum EXTRACTOR_MetaType meta_type;

  /**
   * Format of the meta data.
   */
  enum EXTRACTOR_MetaFormat meta_format;

  /**
   * Number of bytes of meta data (value)
   */
  size_t data_len;
  
  /**
   * Length of the mime type string describing the meta data value's mime type,
   * including 0-terminator, 0 for mime type of "NULL".
   */
  size_t mime_len;
};


/**
 * Create a shared memory area.
 *
 * @param size size of the shared area
 * @return NULL on error
 */
struct EXTRACTOR_SharedMemory *
EXTRACTOR_IPC_shared_memory_create_ (size_t size);


/**
 * Destroy shared memory area.
 *
 * @param shm memory area to destroy
 * @return NULL on error
 */
void
EXTRACTOR_IPC_shared_memory_destroy_ (struct EXTRACTOR_SharedMemory *shm);


/**
 * Initialize shared memory area from data source.
 *
 * @param shm memory area to initialize
 * @param ds data source to use for initialization
 * @param off offset to use in data source
 * @param size number of bytes to copy
 * @return -1 on error, otherwise number of bytes copied
 */
ssize_t
EXTRACTOR_IPC_shared_memory_set_ (struct EXTRACTOR_SharedMemory *shm,
				  struct EXTRACTOR_Datasource *ds,
				  uint64_t off,
				  size_t size);


/**
 * Create a channel to communicate with a process wrapping
 * the plugin of the given name.  Starts the process as well.
 *
 * @param short_libname name of the plugin
 * @param shm memory to share with the process
 * @return NULL on error, otherwise IPC channel
 */ 
struct EXTRACTOR_Channel *
EXTRACTOR_IPC_channel_create_ (const char *short_libname,
			       struct EXTRACTOR_SharedMemory *shm);


/**
 * Destroy communication channel with a plugin/process.  Also
 * destroys the process.
 *
 * @param channel channel to communicate with the plugin
 */
void
EXTRACTOR_IPC_channel_destroy_ (struct EXTRACTOR_Channel *channel);


/**
 * Send data via the given IPC channel (blocking).
 *
 * @param channel channel to communicate with the plugin
 * @param buf data to send
 * @param size number of bytes in buf to send
 * @return -1 on error, number of bytes sent on success
 *           (never does partial writes)
 */
ssize_t
EXTRACTOR_IPC_channel_send_ (struct EXTRACTOR_Channel *channel,
			     const void *data,
			     size_t size);


/**
 * Handler for a message from one of the plugins.
 *
 * @param cls closure
 * @param short_libname library name of the channel sending the message
 * @param msg header of the message from the plugin
 * @param value 'data' send from the plugin
 * @param mime mime string send from the plugin
 */
typedef void (*EXTRACTOR_ChannelMessageProcessor) (void *cls,
						   const char *short_libname,
						   const struct IpcHeader *msg,
						   const void *value,
						   const char *mime);

/**
 * Process a reply from channel (seek request, metadata and done message)
 *
 * @param buf buffer with data from IPC channel
 * @param size number of bytes in buffer
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return number of bytes processed, -1 on error
 */
ssize_t
EXTRACTOR_IPC_process_reply_ (const void *data,
			      size_t size,
			      EXTRACTOR_ChannelMessageProcessor proc,
			      void *proc_cls);


/**
 * Receive data from any of the given IPC channels (blocking).
 * Wait for one of the plugins to reply.
 *
 * @param channels array of channels, channels that break may be set to NULL
 * @param num_channels length of the 'channels' array
 * @param proc function to call to process messages (may be called
 *             more than once)
 * @param proc_cls closure for 'proc'
 * @return -1 on error (i.e. no response in 10s), 1 on success
 */
int
EXTRACTOR_IPC_channel_recv_ (struct EXTRACTOR_Channel **channels,
			     unsigned int num_channels,
			     EXTRACTOR_ChannelMessageProcessor proc,
			     void *proc_cls);


#endif
