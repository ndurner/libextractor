/** 
     zipextractor.c  version 0.0.2

     Changes from 0.0.1 to 0.0.2 
     -> Searches for central dir struct from end of file if this is a self-extracting executable


     This file was based on mp3extractor.c  (0.1.2)

     Currently, this only returns a list of the filenames within a zipfile
     and any comments on each file or the whole file itself. File sizes, 
     modification times, and crc's are currently ignored.

     TODO: Break the comments up into small, atomically, searchable chunks (keywords)
         - might need some knowledge of English?

     It returns:

     one      EXTRACTOR_MIMETYPE
     multiple EXTRACTOR_FILENAME
     multiple EXTRACTOR_COMMENT
     
     ... from a .ZIP file

     TODO: EXTRACTOR_DATE, EXTRACTOR_DESCRIPTION, EXTRACTOR_KEYWORDS, others?

     Does NOT test data integrity (CRCs etc.)

     This version is not recursive (i.e. doesn't look inside zip 
     files within zip files)
     
     TODO: Run extract on files inside of archive (?) (i.e. gif, mp3, etc.)
     
     The current .ZIP format description:
     ftp://ftp.pkware.com/appnote.zip

     No Copyright 2003 Julia Wolf

 */  
  
/*
 * This file is part of libextractor.
 * (C) 2002, 2003, 2009 Vidyut Samanta and Christian Grothoff
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 * 
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */ 
  
#include "platform.h"
#include "extractor.h"
  
#define DEBUG_EXTRACT_ZIP 0
  
/* In a zipfile there are two kinds of comments. One is a big one for the
   entire .zip, it's usually a BBS ad. The other is a small comment on each
   individual file; most people don't use this.
 */ 
  
/* TODO: zip_entry linked list is handeled kinda messily, should clean up (maybe) */ 
  typedef struct
{
  char *filename;
   char *comment;
   void *next;
 } zip_entry;

/* mimetype = application/zip */ 
int 
EXTRACTOR_zip_extract (const unsigned char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  int ret;
  void *tmp;
  zip_entry * info;
  zip_entry * start;
  char *filecomment;
  const unsigned char *pos;
  unsigned int offset, stop;
  unsigned int name_length;
  unsigned int extra_length;
  unsigned int comment_length;
  unsigned int filecomment_length;
  unsigned int entry_count;
#if DEBUG_EXTRACT_ZIP 
  unsigned int entry_total;
#endif

  /* I think the smallest zipfile you can have is about 120 bytes */ 
  if ((NULL == data) || (size < 100))
    return 0;
  if (! (('P' == data[0]) && ('K' == data[1]) && (0x03 == data[2])
         && (0x04 == data[3])))
    return 0;
  
    /* The filenames for each file in a zipfile are stored in two locations.
     * There is one at the start of each entry, just before the compressed data,
     * and another at the end in a 'central directory structure'.
     *
     * In order to catch self-extracting executables, we scan backwards from the end
     * of the file looking for the central directory structure. The previous version
     * of this went forewards through the local headers, but that only works for plain
     * vanilla zip's and I don't feel like writing a special case for each of the dozen
     * self-extracting executable stubs.
     *
     * This assumes that the zip file is considered to be non-corrupt/non-truncated.
     * If it is truncated then it's not considered to be a zip and skipped.
     *
     */ 
    
    /* From appnote.iz and appnote.txt (more or less)
     *
     *   (this is why you always need to put in the last floppy if you span disks)
     *
     *   0- 3  end of central dir signature    4 bytes  (0x06054b50) P K ^E ^F
     *   4- 5  number of this disk             2 bytes
     *   6- 7  number of the disk with the
     *         start of the central directory  2 bytes
     *   8- 9  total number of entries in
     *         the central dir on this disk    2 bytes
     *  10-11  total number of entries in
     *         the central dir                 2 bytes
     *  12-15  size of the central directory   4 bytes
     *  16-19  offset of start of central
     *         directory with respect to
     *         the starting disk number        4 bytes
     *  20-21  zipfile comment length          2 bytes
     *  22-??  zipfile comment (variable size) max length 65536 bytes
     */ 
    
    /*  the signature can't be more than 22 bytes from the end */ 
  offset = size - 22;
  pos = &data[offset];
  stop = 0;
  if (((signed int) size - 65556) > 0)
    stop = size - 65556;
  
    /* not using int 0x06054b50 so that we don't have to deal with endianess issues.
       break out if we go more than 64K backwards and havn't found it, or if we hit the
       begining of the file. */ 
    while ((!(('P' == pos[0]) && ('K' == pos[1]) && (0x05 == pos[2])
              && (0x06 == pos[3]))) && (offset > stop))
    pos = &data[offset--];
  if (offset == stop)
    {
#if DEBUG_EXTRACT_ZIP
      fprintf (stderr,
                 "Did not find end of central directory structure signature. offset: %i\n",
                 offset);
      
#endif
      return 0;
    }
  
    /* offset should now point to the start of the end-of-central directory structure */ 
    /* and pos[0] should be pointing there too */ 
    /* so slurp down filecomment while here... */ 
    filecomment_length = pos[20] + (pos[21] << 8);
  if (filecomment_length + offset + 22 > size)
    {
      return 0;             /* invalid zip file format! */
    }
  filecomment = NULL;
  if (filecomment_length > 0)
    {
      filecomment = malloc (filecomment_length + 1);
      if (filecomment != NULL)
	{
	  memcpy (filecomment, &pos[22], filecomment_length);
	  filecomment[filecomment_length] = '\0';
	}
    }

#if DEBUG_EXTRACT_ZIP
  if ((0 != pos[4]) && (0 != pos[5]))
    fprintf (stderr,
	     "WARNING: This seems to be the last disk in a multi-volume"
	     " ZIP archive, and so this might not work.\n");          
#endif
      
#if DEBUG_EXTRACT_ZIP
  if ((pos[8] != pos[10]) && (pos[9] != pos[11]))
    fprintf (stderr,
	     "WARNING: May not be able to find all the files in this" 
	     " ZIP archive (no multi-volume support right now).\n");  
 entry_total = pos[10] + (pos[11] << 8);
#endif
  entry_count = 0;
  
    /* jump to start of central directory, ASSUMING that the starting disk that it's on is disk 0 */ 
    /* starting disk would otherwise be pos[6]+pos[7]<<8 */ 
    offset = pos[16] + (pos[17] << 8) + (pos[18] << 16) + (pos[19] << 24);     /* offset of cent-dir from start of disk 0 */
  
    /* stop   = pos[12] + (pos[13]<<8) + (pos[14]<<16) + (pos[15]<<24); *//* length of central dir */ 
    if (offset + 46 > size)
    {
      
        /* not a zip */ 
      if (filecomment != NULL)
        free (filecomment);
      return 0;
    }
  pos = &data[offset];         /* jump */
  
    /* we should now be at the begining of the central directory structure */ 
    
    /* from appnote.txt and appnote.iz (mostly)
     *
     *   0- 3  central file header signature   4 bytes  (0x02014b50)
     *   4- 5  version made by                 2 bytes
     *   6- 7  version needed to extract       2 bytes
     *   8- 9  general purpose bit flag        2 bytes
     *  10-11  compression method              2 bytes
     *  12-13  last mod file time              2 bytes
     *  14-15  last mod file date              2 bytes
     *  16-19  crc-32                          4 bytes
     *  20-23  compressed size                 4 bytes
     *  24-27  uncompressed size               4 bytes
     *  28-29  filename length                 2 bytes
     *  30-31  extra field length              2 bytes
     *  32-33  file comment length             2 bytes
     *  34-35  disk number start               2 bytes
     *  36-37  internal file attributes        2 bytes
     *  38-41  external file attributes        4 bytes
     *  42-45  relative offset of local header 4 bytes
     *
     *  46-??  filename (variable size)
     *   ?- ?  extra field (variable size)
     *   ?- ?  file comment (variable size)
     */ 
    if (!(('P' == pos[0]) && ('K' == pos[1]) && (0x01 == pos[2])
          && (0x02 == pos[3])))
      {      
#if DEBUG_EXTRACT_ZIP
        fprintf (stderr,
                 "Did not find central directory structure signature. offset: %i\n",
                 offset);
	
#endif
        if (filecomment != NULL)
	  free (filecomment);
	return 0;
      }
  start = NULL;
  info = NULL;
  
  do
    {                           /* while ( (0x01==pos[2])&&(0x02==pos[3]) ) */
      entry_count++;           /* check to make sure we found everything at the end */
      name_length = pos[28] + (pos[29] << 8);
      extra_length = pos[30] + (pos[31] << 8);
      comment_length = pos[32] + (pos[33] << 8);
      if (name_length + extra_length + comment_length + offset + 46 > size)
        {
          
            /* ok, invalid, abort! */ 
            break;
        }
      
#if DEBUG_EXTRACT_ZIP
        fprintf (stderr, "Found filename length %i  Comment length: %i\n",
                 name_length, comment_length);
      
#endif        
        /* yay, finally get filenames */ 
        if (start == NULL)
        {
          start = malloc (sizeof (zip_entry));
	  if (start == NULL)
	    break;
          start->next = NULL;
          info = start;
        }
      else
        {
          info->next = malloc (sizeof (zip_entry));
	  if (info->next == NULL)
	    break;
          info = info->next;
          info->next = NULL;
        }
      info->filename = malloc (name_length + 1);
      info->comment = malloc (comment_length + 1);
      
        /* (strings in zip files are not null terminated) */ 
      if (info->filename != NULL)
	{
	  memcpy (info->filename, &pos[46], name_length);
	  info->filename[name_length] = '\0';
	}
      if (info->comment != NULL)
	{
	  memcpy (info->comment, &pos[46 + name_length + extra_length],
		  comment_length);
	  info->comment[comment_length] = '\0';
	}
      offset += 46 + name_length + extra_length + comment_length;
      pos = &data[offset];      
      /* check for next header entry (0x02014b50) or (0x06054b50) if at end */ 
      if (('P' != pos[0]) && ('K' != pos[1]))
        {         
#if DEBUG_EXTRACT_ZIP
	  fprintf (stderr,
		   "Did not find next header in central directory.\n");
          
#endif
	  info = start;
          while (info != NULL)
            {
              start = info->next;
	      if (info->filename != NULL)
		free (info->filename);
	      if (info->comment != NULL)
		free (info->comment);
              free (info);
              info = start;
            }
          if (filecomment != NULL)
            free (filecomment);
          return 0;
        }
    }
  while ((0x01 == pos[2]) && (0x02 == pos[3]));
  
    /* end list */ 
    
    /* TODO: should this return an error? indicates corrupt zipfile (or
       disk missing in middle of multi-disk)? */ 
#if DEBUG_EXTRACT_ZIP
  if (entry_count != entry_total)
    fprintf (stderr,
	     "WARNING: Did not find all of the zipfile entries that we should have.\n");    
#endif
  
  ret = proc (proc_cls,
	      "zip",
	      EXTRACTOR_METATYPE_MIMETYPE,
	      EXTRACTOR_METAFORMAT_UTF8,
	      "text/plain",
	      "application/zip",
	      strlen ("application/zip")+1);
  if ( (filecomment != NULL) && (ret != 0) )
    {
      ret = proc (proc_cls,
		  "zip",
		  EXTRACTOR_METATYPE_MIMETYPE,
		  EXTRACTOR_METAFORMAT_UTF8,
		  "text/plain",
		  filecomment,
		  strlen (filecomment)+1);
    }
  if (filecomment != NULL)
    free (filecomment);

  
  /* if we've gotten to here then there is at least one zip entry (see get_zipinfo call above) */ 
  /* note: this free()'s the info list as it goes */ 
  info = start;
  while (NULL != info)
    {
      if (info->filename != NULL)
        {
          if ( (ret == 0) && (strlen (info->filename)) )
            {
	      ret = proc (proc_cls,
			  "zip",
			  EXTRACTOR_METATYPE_FILENAME,
			  EXTRACTOR_METAFORMAT_UTF8,
			  "text/plain",
			  info->filename,
			  strlen (info->filename)+1);
            }
	   free (info->filename);
        }
      if (info->comment != NULL)
	{
	  if ( (ret == 0) && (strlen (info->comment) > 0) )
	    {
	      ret = proc (proc_cls,
			  "zip",
			  EXTRACTOR_METATYPE_FILENAME,
			  EXTRACTOR_METAFORMAT_UTF8,
			  "text/plain",
			  info->comment,
			  strlen (info->comment)+1);
	    }
	  free (info->comment);
	}
      tmp = info;
      info = info->next;
      free (tmp);
    }
  return ret;
}


