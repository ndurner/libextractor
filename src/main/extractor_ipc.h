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
 * @file main/extractor_ipc.h
 * @brief IPC with plugin (OS-independent API)
 * @author Christian Grothoff
 *
 * @detail
 * The IPC communication between plugins and the main library works
 * as follows.  Each message begins with a 1-character opcode which
 * specifies the message type.  The main library starts the plugins
 * by forking the helper process and establishes two pipes for
 * communication in both directions. 
 * First, the main library send an 'INIT_STATE' message
 * to the plugin.  The start message specifies the name (and size)
 * of a shared memory segment which will contain parts of the (uncompressed)
 * data of the file that is being processed.  The same shared memory
 * segment is used throughout the lifetime of the plugin.
 *
 * Then, the following messages are exchanged for each file.
 * First, an EXTRACT_START message is sent with the specific
 * size of the file (or -1 if unknown) and the number of bytes
 * ready in the shared memory segment.  The plugin then answers
 * with either:
 * 1) MESSAGE_DONE to indicate that no further processing is 
 *    required for this file; the IPC continues with the
 *    EXTRACT_START message for the next file afterwards;
 * 2) MESSAGE_SEEK to indicate that the plugin would like to
 *    read data at a different offset; the main library can
 *    then either
 *    a) respond with a MESSAGE_DISCARD_STATE to
 *       tell the plugin to abort processing (the next message will
 *       then be another EXTRACT_START)
 *    b) respond with a MESSAGE_UPDATED_SHM which notifies the
 *       plugin that the shared memory segment was moved to a
 *       different location in the overall file; the target of the
 *       seek should now be within the new range (but does NOT have
 *       to be at the beginning of the seek)
 * 3) MESSAGE_META to provide extracted meta data to the main
 *    library.  The main library can then either:
 *    a) respond with a MESSAGE_DISCARD_STATE to
 *       tell the plugin to abort processing (the next message will
 *       then be another EXTRACT_START)
 *    b) respond with a MESSAGE_CONTINUE_EXTRACTING to
 *       tell the plugin to continue extracting meta data; in this
 *       case, the plugin is then expected to produce another
 *       MESSAGE_DONE, MESSAGE_SEEK or MESSAGE_META round of messages.
 */
#ifndef EXTRACTOR_IPC_H
#define EXTRACTOR_IPC_H

#include "extractor_datasource.h"


/**
 * How long do we allow an individual meta data object to be?
 * Used to guard against (broken) plugns causing us to use
 * excessive amounts of memory.
 */
#define MAX_META_DATA 32 * 1024 * 1024

/**
 * Maximum length of a shared memory object name
 */
#define MAX_SHM_NAME 255

/**
 * Sent from LE to a plugin to initialize it (opens shm).
 */
#define MESSAGE_INIT_STATE 0x00

/**
 * IPC message send to plugin to initialize SHM.
 */
struct InitMessage
{
  /**
   * Set to MESSAGE_INIT_STATE.
   */
  unsigned char opcode;

  /**
   * Always zero.
   */
  unsigned char reserved;

  /**
   * Always zero.
   */
  uint16_t reserved2;

  /**
   * Name of the shared-memory name.
   */
  uint32_t shm_name_length;

  /**
   * Maximum size of the shm map.
   */
  uint32_t shm_map_size;

  /* followed by name of the SHM */
};


/**
 * Sent from LE to a plugin to tell it extracting
 * can now start.  The SHM will point to offset 0
 * of the file.
 */
#define MESSAGE_EXTRACT_START 0x01

/**
 * IPC message send to plugin to start extracting.
 */
struct StartMessage
{
  /**
   * Set to MESSAGE_EXTRACT_START.
   */
  unsigned char opcode;

  /**
   * Always zero.
   */
  unsigned char reserved;

  /**
   * Always zero.
   */
  uint16_t reserved2;

  /**
   * Number of bytes ready in SHM.
   */
  uint32_t shm_ready_bytes;

  /**
   * Overall size of the file.
   */
  uint64_t file_size;

};

/**
 * Sent from LE to a plugin to tell it that shm contents
 * were updated. 
 */
#define MESSAGE_UPDATED_SHM 0x02

/**
 * IPC message send to plugin to notify it about a change in the SHM.
 */
struct UpdateMessage
{
  /**
   * Set to MESSAGE_UPDATED_SHM.
   */
  unsigned char opcode;

  /**
   * Always zero.
   */
  unsigned char reserved;

  /**
   * Always zero.
   */
  uint16_t reserved2;

  /**
   * Number of bytes ready in SHM.
   */
  uint32_t shm_ready_bytes;

  /**
   * Offset of the shm in the overall file.
   */
  uint64_t shm_off;

  /**
   * Overall size of the file.
   */
  uint64_t file_size;

};

/**
 * Sent from plugin to LE to tell LE that plugin is done
 * analyzing current file and will send no more data.
 * No message format as this is only one byte.
 */
#define MESSAGE_DONE 0x03

/**
 * Sent from plugin to LE to tell LE that plugin needs
 * to read a different part of the source file.
 */
#define MESSAGE_SEEK 0x04

/**
 * IPC message send to plugin to start extracting.
 */
struct SeekRequestMessage
{
  /**
   * Set to MESSAGE_SEEK.
   */
  unsigned char opcode;

  /**
   * Always zero.
   */
  unsigned char reserved;

  /**
   * 'whence' value for the seek operation;
   * 0 = SEEK_SET, 1 = SEEK_CUR, 2 = SEEK_END.
   * Note that 'SEEK_CUR' is never used here.
   */
  uint16_t whence;

  /**
   * Number of bytes requested for SHM.
   */
  uint32_t requested_bytes;

  /**
   * Requested offset; a positive value from the end of the
   * file is used of 'whence' is SEEK_END; a postive value
   * from the start is used of 'whence' is SEEK_SET.  
   * 'SEEK_CUR' is never used.
   */
  uint64_t file_offset;

};

/**
 * Sent from plugin to LE to tell LE about metadata discovered.
 */
#define MESSAGE_META 0x05

/**
 * Plugin to parent: metadata discovered
 */
struct MetaMessage
{
  /**
   * Set to MESSAGE_META.
   */
  unsigned char opcode;

  /**
   * Always zero.
   */
  unsigned char reserved;

  /**
   * An 'enum EXTRACTOR_MetaFormat' in 16 bits.
   */
  uint16_t meta_format;

  /**
   * An 'enum EXTRACTOR_MetaType' in 16 bits.
   */
  uint16_t meta_type;

  /**
   * Length of the mime type string.
   */
  uint16_t mime_length;

  /**
   * Size of the value.
   */
  uint32_t value_size;

  /* followed by mime_length bytes of 0-terminated 
     mime-type (unless mime_length is 0) */
  
  /* followed by value_size bytes of value */

};

/**
 * Sent from LE to plugin to make plugin discard its state
 * (extraction aborted by application).  Only one byte.
 * Plugin should get ready for next 'StartMessage' after this.
 * (sent in response to META data or SEEK requests).
 */
#define MESSAGE_DISCARD_STATE 0x06

/**
 * Sent from LE to plugin to make plugin continue extraction.
 * (sent in response to META data).
 */
#define MESSAGE_CONTINUE_EXTRACTING 0x07


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
 * Change the reference counter for this shm instance.
 *
 * @param shm instance to update
 * @param delta value to change RC by
 * @return new RC
 */
unsigned int
EXTRACTOR_IPC_shared_memory_change_rc_ (struct EXTRACTOR_SharedMemory *shm,
					int delta);


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
 * Query datasource for current position
 *
 * @param ds data source to query
 * @return current position in the datasource or UINT_MAX on error
 */
uint64_t
EXTRACTOR_datasource_get_pos_ (struct EXTRACTOR_Datasource *ds);


/**
 * Create a channel to communicate with a process wrapping
 * the plugin of the given name.  Starts the process as well.
 *
 * @param plugin the plugin
 * @param shm memory to share with the process
 * @return NULL on error, otherwise IPC channel
 */ 
struct EXTRACTOR_Channel *
EXTRACTOR_IPC_channel_create_ (struct EXTRACTOR_PluginList *plugin,
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
 * @param plugin plugin of the channel sending the message
 * @param meta_type type of the meta data
 * @param meta_format format of the meta data
 * @param mime mime string send from the plugin
 * @param value 'data' send from the plugin
 * @param value_len number of bytes in 'value'
 */
typedef void (*EXTRACTOR_ChannelMessageProcessor) (void *cls,
						   struct EXTRACTOR_PluginList *plugin,
						   enum EXTRACTOR_MetaType meta_type,
						   enum EXTRACTOR_MetaFormat meta_format,
						   const char *mime,
						   const void *value,
						   size_t value_len);


/**
 * Process a reply from channel (seek request, metadata and done message)
 *
 * @param plugin plugin this communication is about
 * @param buf buffer with data from IPC channel
 * @param size number of bytes in buffer
 * @param proc metadata callback
 * @param proc_cls callback cls
 * @return number of bytes processed, -1 on error
 */
ssize_t
EXTRACTOR_IPC_process_reply_ (struct EXTRACTOR_PluginList *plugin,
			      const void *data,
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
