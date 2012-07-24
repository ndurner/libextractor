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
   * Always zero.
   */
  uint16_t reserved2;

  /**
   * Number of bytes requested for SHM.
   */
  uint32_t requested_bytes;

  /**
   * Requested offset.
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
 * @param value_len number of bytes in 'value'
 * @param value 'data' send from the plugin
 * @param mime mime string send from the plugin
 */
typedef void (*EXTRACTOR_ChannelMessageProcessor) (void *cls,
						   struct EXTRACTOR_PluginList *plugin,
						   enum EXTRACTOR_MetaType meta_type,
						   enum EXTRACTOR_MetaFormat meta_format,
						   size_t value_len,
						   const void *value,
						   const char *mime);


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
