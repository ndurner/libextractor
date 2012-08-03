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
 * @file main/extractor_ipc_gnu.c
 * @brief IPC with plugin for GNU/POSIX systems
 * @author Christian Grothoff
 */
#include "platform.h"
#include "plibc.h"
#include "extractor.h"
#include "extractor_datasource.h"
#include "extractor_logging.h"
#include "extractor_plugin_main.h"
#include "extractor_ipc.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>


/**
 * A shared memory resource (often shared with several
 * other processes).
 */
struct EXTRACTOR_SharedMemory
{
  /**
   * Pointer to the mapped region of the shm (covers the whole shm)
   */ 
  void *shm_ptr;

  /**
   * Page size. Mmap offset is a multiple of this number.
   */
  long allocation_granularity;

  /**
   * Allocated size of the shm
   */ 
  size_t shm_size;

  /**
   * POSIX id of the shm into which data is uncompressed
   */ 
  int shm_id;

  /**
   * Name of the shm
   */ 
  char shm_name[MAX_SHM_NAME + 1];

  /**
   * Reference counter describing how many references share this SHM.
   */
  unsigned int rc;

};


/**
 * Definition of an IPC communication channel with
 * some plugin.
 */
struct EXTRACTOR_Channel
{

  /**
   * Buffer for reading data from the plugin.
   * FIXME: we might want to grow this
   * buffer dynamically instead of always using 32 MB!
   */
  char data[MAX_META_DATA];

  /**
   * Memory segment shared with this process.
   */
  struct EXTRACTOR_SharedMemory *shm;

  /**
   * The plugin this channel is to communicate with.
   */
  struct EXTRACTOR_PluginList *plugin;

  /**
   * Pipe used to communicate information to the plugin child process.
   * NULL if not initialized.
   */
  int cpipe_in;

  /**
   * Number of valid bytes in the channel's buffer.
   */
  size_t size;

  /**
   * Pipe used to read information about extracted meta data from
   * the plugin child process.  -1 if not initialized.
   */
  int cpipe_out;

  /**
   * Process ID of the child process for this plugin. 0 for none.
   */
  pid_t cpid;

};


/**
 * Create a shared memory area.
 *
 * @param size size of the shared area
 * @return NULL on error
 */
struct EXTRACTOR_SharedMemory *
EXTRACTOR_IPC_shared_memory_create_ (size_t size)
{
  struct EXTRACTOR_SharedMemory *shm;
  const char *tpath;

  if (NULL == (shm = malloc (sizeof (struct EXTRACTOR_SharedMemory))))
    {
      LOG_STRERROR ("malloc");
      return NULL;
    }
#if SOMEBSD
  /* this works on FreeBSD, not sure about others... */
  tpath = getenv ("TMPDIR");
  if (tpath == NULL)
    tpath = "/tmp/";
#else
  tpath = "/"; /* Linux */
#endif 
  snprintf (shm->shm_name,
	    MAX_SHM_NAME, 
	    "%slibextractor-shm-%u-%u", 
	    tpath, getpid (),
	    (unsigned int) RANDOM());
  if (-1 == (shm->shm_id = shm_open (shm->shm_name,
				     O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)))
    {
      LOG_STRERROR_FILE ("shm_open", shm->shm_name);
      free (shm);
      return NULL;
    }
  if ( (0 != ftruncate (shm->shm_id, size)) ||
       (NULL == (shm->shm_ptr = mmap (NULL, size, 
				      PROT_WRITE, MAP_SHARED, 
				      shm->shm_id, 0))) ||
       (((void*) -1) == shm->shm_ptr) )
  {
    LOG_STRERROR ("ftruncate/mmap");
    (void) close (shm->shm_id);
    (void) shm_unlink (shm->shm_name);
    free (shm);
    return NULL;
  }
  shm->shm_size = size;
  shm->rc = 0;
  return shm; 
}


/**
 * Change the reference counter for this shm instance.
 *
 * @param shm instance to update
 * @param delta value to change RC by
 * @return new RC
 */
unsigned int
EXTRACTOR_IPC_shared_memory_change_rc_ (struct EXTRACTOR_SharedMemory *shm,
					int delta)
{
  shm->rc += delta;
  return shm->rc;
}


/**
 * Destroy shared memory area.
 *
 * @param shm memory area to destroy
 * @return NULL on error
 */
void
EXTRACTOR_IPC_shared_memory_destroy_ (struct EXTRACTOR_SharedMemory *shm)
{  
  munmap (shm->shm_ptr, shm->shm_size);
  (void) close (shm->shm_id);
  (void) shm_unlink (shm->shm_name);
  free (shm);
}


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
				  size_t size)
{
  if (-1 ==
      EXTRACTOR_datasource_seek_ (ds, off, SEEK_SET))
    return -1;
  if (size > shm->shm_size)
    size = shm->shm_size;
  return EXTRACTOR_datasource_read_ (ds,
				     shm->shm_ptr,
				     size);
}

/**
 * Query datasource for current position
 *
 * @param ds data source to query
 * @return current position in the datasource or UINT_MAX on error
 */
uint64_t
EXTRACTOR_datasource_get_pos_ (struct EXTRACTOR_Datasource *ds)
{
  int64_t pos = EXTRACTOR_datasource_seek_ (ds, 0, SEEK_CUR);
  if (-1 == pos)
    return UINT_MAX;
  return pos;
}

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
			       struct EXTRACTOR_SharedMemory *shm)
{
  struct EXTRACTOR_Channel *channel;
  int p1[2];
  int p2[2];
  pid_t pid;
  struct InitMessage *init;
  size_t slen;

  if (NULL == (channel = malloc (sizeof (struct EXTRACTOR_Channel))))
    {
      LOG_STRERROR ("malloc");
      return NULL;
    }
  channel->shm = shm;
  channel->plugin = plugin;
  channel->size = 0;
  if (0 != pipe (p1))
    {
      LOG_STRERROR ("pipe");
      free (channel);
      return NULL;
    }
  if (0 != pipe (p2))
    {
      LOG_STRERROR ("pipe");
      (void) close (p1[0]);
      (void) close (p1[1]);
      free (channel);
      return NULL;
    }
  pid = fork ();
  if (pid == -1)
    {
      LOG_STRERROR ("fork");
      (void) close (p1[0]);
      (void) close (p1[1]);
      (void) close (p2[0]);
      (void) close (p2[1]);
      free (channel);
      return NULL;
    }
  if (0 == pid)
    {
      (void) close (p1[1]);
      (void) close (p2[0]);
      EXTRACTOR_plugin_main_ (plugin, p1[0], p2[1]);
      _exit (0);
    }
  (void) close (p1[0]);
  (void) close (p2[1]);
  channel->cpipe_in = p1[1];
  channel->cpipe_out = p2[0];
  channel->cpid = pid;
  slen = strlen (shm->shm_name) + 1;
  if (NULL == (init = malloc (sizeof (struct InitMessage) + slen)))
    {
      LOG_STRERROR ("malloc");
      EXTRACTOR_IPC_channel_destroy_ (channel);
      return NULL;
    }  
  init->opcode = MESSAGE_INIT_STATE;
  init->reserved = 0;
  init->reserved2 = 0;
  init->shm_name_length = slen;
  init->shm_map_size = shm->shm_size;
  memcpy (&init[1], shm->shm_name, slen);
  if (sizeof (struct InitMessage) + slen !=
      EXTRACTOR_IPC_channel_send_ (channel,
				   init,
				   sizeof (struct InitMessage) + slen) )
    {
      LOG ("Failed to send INIT_STATE message to plugin\n");
      EXTRACTOR_IPC_channel_destroy_ (channel);
      free (init);
      return NULL;
    }
  free (init);
  return channel;
}


/**
 * Destroy communication channel with a plugin/process.  Also
 * destroys the process.
 *
 * @param channel channel to communicate with the plugin
 */
void
EXTRACTOR_IPC_channel_destroy_ (struct EXTRACTOR_Channel *channel)
{
  int status;

  if (0 != kill (channel->cpid, SIGKILL))
    LOG_STRERROR ("kill");
  if (-1 == waitpid (channel->cpid, &status, 0))
    LOG_STRERROR ("waitpid");
  if (0 != close (channel->cpipe_out))
    LOG_STRERROR ("close");
  if (0 != close (channel->cpipe_in))
    LOG_STRERROR ("close");
  free (channel);
}


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
			     size_t size)
{
  const char *cdata = data;
  size_t off = 0;
  ssize_t ret;

  while (off < size)
    {
      ret = write (channel->cpipe_in, &cdata[off], size - off);
      if (ret <= 0)
	{
	  if (-1 == ret)
	    LOG_STRERROR ("write");
	  return -1;
	}
      off += ret;
    }
  return size;
}


/**
 * Receive data from any of the given IPC channels (blocking).
 * Wait for one of the plugins to reply.
 * Selects on plugin output pipes, runs 'receive_reply'
 * on each activated pipe until it gets a seek request
 * or a done message. Called repeatedly by the user until all pipes are dry or
 * broken.
 *
 * @param channels array of channels, channels that break may be set to NULL
 * @param num_channels length of the 'channels' array
 * @param proc function to call to process messages (may be called
 *             more than once)
 * @param proc_cls closure for 'proc'
 * @return -1 on error, 1 on success
 */
int
EXTRACTOR_IPC_channel_recv_ (struct EXTRACTOR_Channel **channels,
			     unsigned int num_channels,
			     EXTRACTOR_ChannelMessageProcessor proc,
			     void *proc_cls)
{
  struct timeval tv;
  fd_set to_check;
  int max;
  unsigned int i;
  struct EXTRACTOR_Channel *channel;
  ssize_t ret;
  ssize_t iret;

  FD_ZERO (&to_check);
  max = -1;
  for (i=0;i<num_channels;i++)
    {
      channel = channels[i];
      if (NULL == channel)
	continue;
      FD_SET (channel->cpipe_out, &to_check);
      if (max < channel->cpipe_out)
	max = channel->cpipe_out;
    }
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  if (-1 == select (max + 1, &to_check, NULL, NULL, &tv))
    {
      /* an error or timeout -> something's wrong or all plugins hung up */
      if (EINTR != errno)
	LOG_STRERROR ("select");
      return -1;
    }
  for (i=0;i<num_channels;i++)
    {
      channel = channels[i];
      if (NULL == channel)
	continue;
      if (! FD_ISSET (channel->cpipe_out, &to_check))
	continue;
      if ( (-1 == (iret = read (channel->cpipe_out,
				&channel->data[channel->size],
				MAX_META_DATA - channel->size)) ) ||
	   (-1 == (ret = EXTRACTOR_IPC_process_reply_ (channel->plugin,
						       channel->data, 
						       channel->size + iret, 
						       proc, proc_cls)) ) )
	{
	  if (-1 == iret)
	    LOG_STRERROR ("read");
	  EXTRACTOR_IPC_channel_destroy_ (channel);
	  channels[i] = NULL;
	}
      else
	{
	  memmove (channel->data,
		   &channel->data[ret],
		   channel->size + iret - ret);
	  channel->size = channel->size + iret - ret;
	}
    }
  return 1;
}

/* end of extractor_ipc_gnu.c */