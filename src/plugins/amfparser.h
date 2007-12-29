/*
     This file is part of libextractor.
     Copyright (C) 2007 Heikki Lindholm

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

/*
 * see http://osflash.org/documentation/amf
 */

#ifndef AMFPARSER_H
#define AMFPARSER_H

/* Actionscript types */
#define ASTYPE_NUMBER       0x00
#define ASTYPE_BOOLEAN      0x01
#define ASTYPE_STRING       0x02
#define ASTYPE_OBJECT       0x03
#define ASTYPE_MOVIECLIP    0x04
#define ASTYPE_NULL         0x05
#define ASTYPE_UNDEFINED    0x06
#define ASTYPE_REFERENCE    0x07
#define ASTYPE_MIXEDARRAY   0x08
#define ASTYPE_ENDOFOBJECT  0x09
#define ASTYPE_ARRAY        0x0a
#define ASTYPE_DATE         0x0b
#define ASTYPE_LONGSTRING   0x0c
#define ASTYPE_UNSUPPORTED  0x0d
#define ASTYPE_RECORDSET    0x0e
#define ASTYPE_XML          0x0f
#define ASTYPE_TYPEDOBJECT  0x10
#define ASTYPE_AMF3DATA     0x11

typedef struct {
  void * userdata;
  void (*as_begin_callback)(unsigned char type, void * userdata);
  void (*as_key_callback)(char * key, void * userdata);
  void (*as_end_callback)(unsigned char type, void * value, void * userdata);
} AMFParserHandler;

extern int parse_amf(const unsigned char **data, 
                     size_t *len,
                     AMFParserHandler *handler);

#endif /* AMFPARSER_H */

