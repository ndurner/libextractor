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
#ifndef EXTRACTOR_DATASOURCE_H
#define EXTRACTOR_DATASOURCE_H

/**
 * Handle to a datasource we can use for the plugins.
 */ 
struct EXTRACTOR_Datasource;


/**
 * Create a datasource from a file on disk.
 *
 * @param filename name of the file on disk
 * @return handle to the datasource
 */
struct EXTRACTOR_Datasource *
EXTRACTOR_datasource_create_from_file_ (const char *filename);


/**
 * Create a datasource from a buffer in memory.
 *
 * @param buf data in memory
 * @param size number of bytes in 'buf'
 * @return handle to the datasource
 */
struct EXTRACTOR_Datasource *
EXTRACTOR_datasource_create_from_buffer_ (const char *buf,
					  size_t size);


/**
 * Destroy a data source.
 *
 * @param datasource source to destroy
 */
void
EXTRACTOR_datasource_destroy_ (struct EXTRACTOR_Datasource *datasource);


/**
 * Make 'size' bytes of data from the data source available at '*data'.
 *
 * @param cls must be a 'struct EXTRACTOR_Datasource'
 * @param data where the data should be copied to
 * @param size maximum number of bytes requested
 * @return number of bytes now available in data (can be smaller than 'size'),
 *         -1 on error
 */
ssize_t
EXTRACTOR_datasource_read_ (void *cls,
			    void *data,
			    size_t size);


/**
 * Seek in the datasource.  Use 'SEEK_CUR' for whence and 'pos' of 0 to
 * obtain the current position in the file.
 * 
 * @param cls must be a 'struct EXTRACTOR_Datasource'
 * @param pos position to seek (see 'man lseek')
 * @param whence how to see (absolute to start, relative, absolute to end)
 * @return new absolute position, UINT64_MAX on error (i.e. desired position
 *         does not exist)
 */
uint64_t
EXTRACTOR_datasource_seek_ (void *cls,
			    uint64_t pos,
			    int whence);


/**
 * Determine the overall size of the data source (after compression).
 * 
 * @param cls must be a 'struct EXTRACTOR_Datasource'
 * @return overall file size, UINT64_MAX on error (i.e. IPC failure)
 */ 
uint64_t 
EXTRACTOR_datasource_get_size_ (void *cls);


#endif
