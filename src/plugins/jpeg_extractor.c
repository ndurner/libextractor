/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
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

#include "platform.h"
#include "extractor.h"


#define M_SOI   0xD8            /* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9            /* End Of Image (end of datastream) */
#define M_SOS   0xDA            /* Start Of Scan (begins compressed data) */
#define M_APP12	0xEC
#define M_COM   0xFE            /* COMment */
#define M_APP0  0xE0

/**
 * Get the next character in the sequence and advance
 * the pointer *data to the next location in the sequence.
 * If we're at the end, return -1.
 */
#define NEXTC(data,end) ((*(data)<(end))?*((*(data))++):-1)

/* The macro does:
unsigned int NEXTC(unsigned char ** data, char *  end) {
  if (*data < end) {
    char result = **data;
    (*data)++;
    return result;
  } else
    return -1;
}
*/

/**
 * Read length, convert to unsigned int.
 * All 2-byte quantities in JPEG markers are MSB first
 * @return -1 on error
 */
static int
readLength (const unsigned char **data, const unsigned char *end)
{
  int c1;
  int c2;

  c1 = NEXTC (data, end);
  if (c1 == -1)
    return -1;
  c2 = NEXTC (data, end);
  if (c2 == -1)
    return -1;
  return ((((unsigned int) c1) << 8) + ((unsigned int) c2)) - 2;
}

/**
 * @return the next marker or -1 on error.
 */
static int
next_marker (const unsigned char **data, const unsigned char *end)
{
  int c;
  c = NEXTC (data, end);
  while ((c != 0xFF) && (c != -1))
    c = NEXTC (data, end);
  do
    {
      c = NEXTC (data, end);
    }
  while (c == 0xFF);
  return c;
}

static void
skip_variable (const unsigned char **data, const unsigned char *end)
{
  int length;

  length = readLength (data, end);
  if (length < 0)
    {
      (*data) = end;            /* skip to the end */
      return;
    }
  /* Skip over length bytes */
  (*data) += length;
}

static char *
process_COM (const unsigned char **data, const unsigned char *end)
{
  unsigned int length;
  int ch;
  int pos;
  char *comment;

  length = readLength (data, end);
  if (length <= 0)
    return NULL;
  comment = malloc (length + 1);
  if (comment == NULL)
    return NULL;
  pos = 0;
  while (length > 0)
    {
      ch = NEXTC (data, end);
      if ((ch == '\r') || (ch == '\n'))
        comment[pos++] = '\n';
      else if (isprint ((unsigned char) ch))
        comment[pos++] = ch;
      length--;
    }
  comment[pos] = '\0';
  return comment;
}


int 
EXTRACTOR_jpeg_extract (const unsigned char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  int c1;
  int c2;
  int marker;
  const unsigned char *end;
  char *tmp;
  char val[128];

  if (size < 0x12)
    return 0;
  end = &data[size];
  c1 = NEXTC (&data, end);
  c2 = NEXTC (&data, end);
  if ((c1 != 0xFF) || (c2 != M_SOI))
    return 0;              /* not a JPEG */
  if (0 != proc (proc_cls, 
		 "jpeg",
		 EXTRACTOR_METATYPE_MIMETYPE,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 "image/jpeg",
		 strlen ("image/jpeg")+1))
    return 1;
  while (1)
    {
      marker = next_marker (&data, end);
      switch (marker)
        {
        case -1:               /* end of file */
        case M_SOS:
        case M_EOI:
          goto RETURN;
        case M_APP0:
          {
            int len = readLength (&data, end);
            if (len < 0x8)
              goto RETURN;
            if (0 == strncmp ((char *) data, "JFIF", 4))
              {
                switch (data[0x4])
                  {
                  case 1:      /* dots per inch */
                    snprintf (val, 
			      sizeof (val),
                              _("%ux%u dots per inch"),
                              (data[0x8] << 8) + data[0x9],
                              (data[0xA] << 8) + data[0xB]);
		    if (0 != proc (proc_cls, 
				   "jpeg",
				   EXTRACTOR_METATYPE_IMAGE_RESOLUTION,
				   EXTRACTOR_METAFORMAT_UTF8,
				   "text/plain",
				   val,
				   strlen (val)+1))
		      return 1;
                    break;
                  case 2:      /* dots per cm */
                    snprintf (val, 
			      sizeof (val),
                              _("%ux%u dots per cm"),
                              (data[0x8] << 8) + data[0x9],
                              (data[0xA] << 8) + data[0xB]);
		    if (0 != proc (proc_cls, 
				   "jpeg",
				   EXTRACTOR_METATYPE_IMAGE_RESOLUTION,
				   EXTRACTOR_METAFORMAT_UTF8,
				   "text/plain",
				   val,
				   strlen (val)+1))
		      return 1;
                    break;
                  case 0:      /* no unit given */
                    snprintf (val, 
			      sizeof (val),
                              _("%ux%u dots per inch?"),
                              (data[0x8] << 8) + data[0x9],
                              (data[0xA] << 8) + data[0xB]);
		    if (0 != proc (proc_cls, 
				   "jpeg",
				   EXTRACTOR_METATYPE_IMAGE_RESOLUTION,
				   EXTRACTOR_METAFORMAT_UTF8,
				   "text/plain",
				   val,
				   strlen (val)+1))
		      return 1;
                    break;
                  default:     /* unknown unit */
                    break;
                  }
              }
            data = &data[len];
            break;
          }
        case 0xC0:
          {
            int len = readLength (&data, end);
            if (len < 0x9)
              goto RETURN;
            snprintf (val, 
		      sizeof (val),
                      "%ux%u",
                      (data[0x3] << 8) + data[0x4],
                      (data[0x1] << 8) + data[0x2]);
	    if (0 != proc (proc_cls, 
			   "jpeg",
			   EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
			   EXTRACTOR_METAFORMAT_UTF8,
			   "text/plain",
			   val,
			   strlen (val)+1))
	      return 1;
            data = &data[len];
            break;
          }
        case M_COM:
        case M_APP12:
          tmp = process_COM (&data, end);
	  if (NULL == tmp)
	    break;
	  if (0 != proc (proc_cls, 
			 "jpeg",
			 EXTRACTOR_METATYPE_COMMENT,
			 EXTRACTOR_METAFORMAT_UTF8,
			 "text/plain",
			 tmp,
			 strlen (tmp)+1))
	    {
	      free (tmp);
	      return 1;
	    }
	  free (tmp);
          break;
        default:
          skip_variable (&data, end);
          break;
        }
    }
RETURN:
  return 0;
}
