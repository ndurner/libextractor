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
#include "platform.h"
#include "convert_numeric.h"
#include "amfparser.h"

#define DEBUG 0

/* core datatypes */

static inline unsigned long readLong(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  unsigned long val;

  val = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
  ptr += 4;
  *data = ptr;
  return val;
}

static inline unsigned long readMediumInt(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  unsigned long val;

  val = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
  ptr += 3;
  *data = ptr;
  return val;
}

static inline unsigned short readInt(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  unsigned short val;

  val = (ptr[0] << 8) | ptr[1];
  ptr += 2;
  *data = ptr;
  return val;
}

static inline double readDouble(const unsigned char **data)
{
  const unsigned char *ptr = *data;
  double val;

  floatformat_to_double(&floatformat_ieee_double_big, 
                        (const void *)ptr, 
                        &val);
  ptr += 8;
  *data = ptr;
  return val;
}


/* actionscript types */

static int readASNumber(const unsigned char **data,
                               size_t *len,
                               double *retval)
{
  const unsigned char *ptr = *data;
  char *ret;
  double val;

  if (*len < 8)
    return -1;

  val = readDouble(&ptr);
  *len -= 8;

  *retval = val;
  *data = ptr;
  return 0;
}

static int readASBoolean(const unsigned char **data,
                                size_t *len,
                                int *retval)
{
  const unsigned char *ptr = *data;
  char *ret;
  int val;
  
  if (*len < 1)
    return -1;

  val = (*ptr != 0x00);
#if DEBUG
  printf("asbool: %d\n", val);
#endif
  ptr += 1;
  *len -= 1;

  *retval = val;
  *data = ptr;
  return 0;
}

static int readASDate(const unsigned char **data,
                             size_t *len,
                             double *millis,
                             short *zone)
{
  const unsigned char *ptr = *data;
  char *ret;

  if (*len < 10)
    return -1;

  *millis = readDouble(&ptr);
  *len -= 8;

  *zone = readInt(&ptr);
  len -= 2;

#if DEBUG
  printf("asdate: %f tz: %d\n", *millis, *zone);
#endif

  *data = ptr;
  return 0;
}

static int readASString(const unsigned char **data,
                               size_t *len,
                               char **retval)
{
  const unsigned char *ptr = *data;
  char *ret;
  int slen;
 
  if (*len < 2)
    return -1;

  slen = readInt(&ptr);

  if (*len < (2 + slen))
    return -1;

  ret = malloc(slen+1);
  if (ret == NULL)
    return -1;
  memcpy(ret, ptr, slen);
  ret[slen] = '\0';
#if DEBUG
  printf("asstring: %p %s\n", ret, ret);
#endif
  ptr += slen;
  *len -= (2 + slen);

  *retval = ret;
  *data = ptr;
  return 0;
}

int parse_amf(const unsigned char **data, 
              size_t *len,
              AMFParserHandler *handler) 
{
  const unsigned char *ptr = *data;
  unsigned char astype;
  int ret;

  ret = 0;
  astype = *ptr++;
  (*(handler->as_begin_callback))(astype, handler->userdata);
  switch (astype) {
    case ASTYPE_NUMBER:
    {
      double val;
      ret = readASNumber(&ptr, len, &val);
      if (ret == 0)
        (*(handler->as_end_callback))(astype, 
                                      &val, 
                                      handler->userdata);
      break;
    }
    case ASTYPE_BOOLEAN:
    {
      int val;
      ret = readASBoolean(&ptr, len, &val);
      if (ret == 0)
        (*(handler->as_end_callback))(astype, 
                                      &val, 
                                      handler->userdata);
      break;
    }
    case ASTYPE_STRING:
    {
      char *val;
      ret = readASString(&ptr, len, &val);
      if (ret == 0) {
        (*(handler->as_end_callback))(astype, 
                                      val, 
                                      handler->userdata);
        free(val);
      }
      break;
    }
    case ASTYPE_DATE:
    {
      void *tmp[2];
      double millis;
      short tz;
      ret = readASDate(&ptr, len, &millis, &tz);
      tmp[0] = &millis;
      tmp[1] = &tz;
      if (ret == 0)
        (*(handler->as_end_callback))(astype, 
                                      &tmp, 
                                      handler->userdata);
      break;
    }
    case ASTYPE_NULL:
    case ASTYPE_UNDEFINED:
    case ASTYPE_UNSUPPORTED:
      ret = 0;
      (*(handler->as_end_callback))(astype, NULL, handler->userdata);
      break;
    case ASTYPE_ENDOFOBJECT:
      ret = 0;
      (*(handler->as_end_callback))(astype, NULL, handler->userdata);
#if DEBUG
      printf("asendofboject\n");
#endif
      break;
    case ASTYPE_ARRAY:
    {
      long i, alen;
#if DEBUG
      printf("asarray:\n");
#endif
      if (*len < 4) {
        ret = -1;
        break;
      }
      alen = readLong(&ptr);
      *len -= 4;
#if DEBUG
      printf(" len: %ld\n", alen);
#endif
      for (i = 0; i < alen; i++) {
        ret = parse_amf(&ptr, len, handler);
        if (ret == -1)
	  break;
      }
      (*(handler->as_end_callback))(ASTYPE_ARRAY, 
                                    NULL, 
                                    handler->userdata);
#if DEBUG
      printf("asarray: END\n");
#endif
      break;
    }  
    case ASTYPE_OBJECT:
    {
      char *key;
      unsigned char type;
#if DEBUG
      printf("asobject:\n");
#endif
      ret = readASString(&ptr, len, &key);
      if (ret == -1)
        break;
      (*(handler->as_key_callback))(key, 
                                    handler->userdata);
      free(key);
      type = *ptr;
      while (type != ASTYPE_ENDOFOBJECT) {
        ret = parse_amf(&ptr, len, handler);
        if (ret == -1)
          break;
        ret = readASString(&ptr, len, &key);
        if (ret == -1)
          break;
        (*(handler->as_key_callback))(key, 
                                      handler->userdata);
        free(key);
        type = *ptr;      
      }
      if (ret == 0)
        (*(handler->as_end_callback))(ASTYPE_OBJECT, 
                                      NULL, 
                                      handler->userdata);
#if DEBUG
      printf("asobject END:\n");
#endif
      break;
    }  
    case ASTYPE_MIXEDARRAY:
    {
      char *key;
      unsigned char type;
      long max_index;
#if DEBUG
      printf("asmixedarray:\n");
#endif
      if (*len < 4) {
        ret = -1;
        break;
      }
      max_index = readLong(&ptr);
      *len -= 4;
#if DEBUG
      printf(" max index: %ld\n", max_index);
#endif
      ret = readASString(&ptr, len, &key);
      if (ret == -1)
        break;
      (*(handler->as_key_callback))(key, 
                                    handler->userdata);
      free(key);
      type = *ptr;
      while (type != ASTYPE_ENDOFOBJECT) {
        ret = parse_amf(&ptr, len, handler);
        if (ret == -1)
          break;
        ret = readASString(&ptr, len, &key);
        if (ret == -1)
          break;
        (*(handler->as_key_callback))(key, 
                                      handler->userdata);
        free(key);
        type = *ptr;      
      }
      if (ret == 0)
        (*(handler->as_end_callback))(astype, 
                                      NULL, 
                                      handler->userdata);
#if DEBUG
      printf("asmixedarray: END\n");
#endif
      break;
    }  
    default:
      ret = -1;
      (*(handler->as_end_callback))(astype, 
                                    NULL, 
                                    handler->userdata);
#if DEBUG
      printf("asunknown %x\n", astype);
#endif
      break;
  }

  *data = ptr;
  return ret;
}

