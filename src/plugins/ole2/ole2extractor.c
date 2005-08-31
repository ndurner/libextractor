/*
     This file is part of libextractor.
     (C) 2004,2005 Vidyut Samanta and Christian Grothoff

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
#include <glib-object.h>
#include "gsf-infile-msole.h"
#include "gsf-input.h"
#include "gsf-utils.h"

#define DEBUG_OLE2 0

#if DEBUG_OLE2
#define d(code)	do { code } while (0)
#define warning printf
#else
#define d(code)
 static void warning(const char * format, ...) {}
#endif


/* using libgobject, needs init! */
void __attribute__ ((constructor)) ole_gobject_init(void) {
 g_type_init();
}

static struct EXTRACTOR_Keywords *
addKeyword(EXTRACTOR_KeywordList *oldhead,
	   const char *phrase,
	   EXTRACTOR_KeywordType type) {

   EXTRACTOR_KeywordList * keyword;
   if (strlen(phrase) == 0)
     return oldhead;
   if (0 == strcmp(phrase, "\"\""))
     return oldhead;
   if (0 == strcmp(phrase, "\" \""))
     return oldhead;
   if (0 == strcmp(phrase, " "))
     return oldhead;
   keyword = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
   keyword->next = oldhead;
   keyword->keyword = strdup(phrase);
   keyword->keywordType = type;
   return keyword;
}


static guint8 const component_guid [] = {
	0xe0, 0x85, 0x9f, 0xf2, 0xf9, 0x4f, 0x68, 0x10,
	0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9
};

static guint8 const document_guid [] = {
	0x02, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

static guint8 const user_guid [] = {
	0x05, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

typedef enum {
	GSF_MSOLE_META_DATA_COMPONENT,
	GSF_MSOLE_META_DATA_DOCUMENT,
	GSF_MSOLE_META_DATA_USER
} GsfMSOleMetaDataType;

typedef enum {
	LE_VT_EMPTY               = 0,
	LE_VT_NULL                = 1,
	LE_VT_I2                  = 2,
	LE_VT_I4                  = 3,
	LE_VT_R4                  = 4,
	LE_VT_R8                  = 5,
	LE_VT_CY                  = 6,
	LE_VT_DATE                = 7,
	LE_VT_BSTR                = 8,
	LE_VT_DISPATCH            = 9,
	LE_VT_ERROR               = 10,
	LE_VT_BOOL                = 11,
	LE_VT_VARIANT             = 12,
	LE_VT_UNKNOWN             = 13,
	LE_VT_DECIMAL             = 14,
	LE_VT_I1                  = 16,
	LE_VT_UI1                 = 17,
	LE_VT_UI2                 = 18,
	LE_VT_UI4                 = 19,
	LE_VT_I8                  = 20,
	LE_VT_UI8                 = 21,
	LE_VT_INT                 = 22,
	LE_VT_UINT                = 23,
	LE_VT_VOID                = 24,
	LE_VT_HRESULT             = 25,
	LE_VT_PTR                 = 26,
	LE_VT_SAFEARRAY           = 27,
	LE_VT_CARRAY              = 28,
	LE_VT_USERDEFINED         = 29,
	LE_VT_LPSTR               = 30,
	LE_VT_LPWSTR              = 31,
	LE_VT_FILETIME            = 64,
	LE_VT_BLOB                = 65,
	LE_VT_STREAM              = 66,
	LE_VT_STORAGE             = 67,
	LE_VT_STREAMED_OBJECT     = 68,
	LE_VT_STORED_OBJECT       = 69,
	LE_VT_BLOB_OBJECT         = 70,
	LE_VT_CF                  = 71,
	LE_VT_CLSID               = 72,
	LE_VT_VECTOR              = 0x1000
} GsfMSOleVariantType;

typedef struct {
	char const *name;
	guint32	    id;
	GsfMSOleVariantType prefered_type;
} GsfMSOleMetaDataPropMap;

typedef struct {
	guint32		id;
	off_t	offset;
} GsfMSOleMetaDataProp;

typedef struct {
	GsfMSOleMetaDataType type;
	off_t   offset;
	guint32	    size, num_props;
	GIConv	    iconv_handle;
	unsigned    char_size;
	GHashTable *dict;
} GsfMSOleMetaDataSection;

static GsfMSOleMetaDataPropMap const document_props[] = {
	{ "Category",		2,	LE_VT_LPSTR },
	{ "PresentationFormat",	3,	LE_VT_LPSTR },
	{ "NumBytes",		4,	LE_VT_I4 },
	{ "NumLines",		5,	LE_VT_I4 },
	{ "NumParagraphs",	6,	LE_VT_I4 },
	{ "NumSlides",		7,	LE_VT_I4 },
	{ "NumNotes",		8,	LE_VT_I4 },
	{ "NumHiddenSlides",	9,	LE_VT_I4 },
	{ "NumMMClips",		10,	LE_VT_I4 },
	{ "Scale",		11,	LE_VT_BOOL },
	{ "HeadingPairs",	12,	LE_VT_VECTOR | LE_VT_VARIANT },
	{ "DocumentParts",	13,	LE_VT_VECTOR | LE_VT_LPSTR },
	{ "Manager",		14,	LE_VT_LPSTR },
	{ "Company",		15,	LE_VT_LPSTR },
	{ "LinksDirty",		16,	LE_VT_BOOL }
};

static GsfMSOleMetaDataPropMap const component_props[] = {
	{ "Title",		2,	LE_VT_LPSTR },
	{ "Subject",		3,	LE_VT_LPSTR },
	{ "Author",		4,	LE_VT_LPSTR },
	{ "Keywords",		5,	LE_VT_LPSTR },
	{ "Comments",		6,	LE_VT_LPSTR },
	{ "Template",		7,	LE_VT_LPSTR },
	{ "LastSavedBy",	8,	LE_VT_LPSTR },
	{ "RevisionNumber",	9,	LE_VT_LPSTR },
	{ "TotalEditingTime",	10,	LE_VT_FILETIME },
	{ "LastPrinted",	11,	LE_VT_FILETIME },
	{ "CreateTime",		12,	LE_VT_FILETIME },
	{ "LastSavedTime",	13,	LE_VT_FILETIME },
	{ "NumPages",		14,	LE_VT_I4 },
	{ "NumWords",		15,	LE_VT_I4 },
	{ "NumCharacters",	16,	LE_VT_I4 },
	{ "Thumbnail",		17,	LE_VT_CF },
	{ "AppName",		18,	LE_VT_LPSTR },
	{ "Security",		19,	LE_VT_I4 }
};

static GsfMSOleMetaDataPropMap const common_props[] = {
	{ "Dictionary",		0,	0, /* magic */},
	{ "CodePage",		1,	LE_VT_UI2 },
	{ "LOCALE_SYSTEM_DEFAULT",	0x80000000,	LE_VT_UI4},
	{ "CASE_SENSITIVE",		0x80000003,	LE_VT_UI4},
};

typedef struct {
  char * text;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tmap[] = {
  { "Title", EXTRACTOR_TITLE },
  { "PresentationFormat", EXTRACTOR_FORMAT },
  { "Category", EXTRACTOR_DESCRIPTION },
  { "Manager", EXTRACTOR_CREATED_FOR },
  { "Company", EXTRACTOR_ORGANIZATION },
  { "Subject", EXTRACTOR_SUBJECT },
  { "Author", EXTRACTOR_AUTHOR },
  { "Keywords", EXTRACTOR_KEYWORDS },
  { "Comments", EXTRACTOR_COMMENT },
  { "Template", EXTRACTOR_FORMAT },
  { "NumPages", EXTRACTOR_PAGE_COUNT },
  { "AppName", EXTRACTOR_SOFTWARE },
  { "RevisionNumber", EXTRACTOR_VERSIONNUMBER },
  { "Dictionary", EXTRACTOR_LANGUAGE },
  { "NumBytes", EXTRACTOR_SIZE },
  { "CreatedTime", EXTRACTOR_CREATION_DATE },
  { "LastSavedTime" , EXTRACTOR_MODIFICATION_DATE },
  { NULL, 0 },
};


static char const *
msole_prop_id_to_gsf (GsfMSOleMetaDataSection *section, guint32 id)
{
  char const *res = NULL;
  GsfMSOleMetaDataPropMap const *map = NULL;
  unsigned i = 0;

  if (section->dict != NULL) {
    if (id & 0x1000000) {
      id &= ~0x1000000;
      d (printf ("LINKED "););
    }

    res = g_hash_table_lookup (section->dict, GINT_TO_POINTER (id));

    if (res != NULL) {
      d (printf (res););
      return res;
    }
  }

  if (section->type == GSF_MSOLE_META_DATA_COMPONENT) {
    map = component_props;
    i = G_N_ELEMENTS (component_props);
  } else if (section->type == GSF_MSOLE_META_DATA_DOCUMENT) {
    map = document_props;
    i = G_N_ELEMENTS (document_props);
  }
  while (i-- > 0)
    if (map[i].id == id) {
      d (printf (map[i].name););
      return map[i].name;
    }

  map = common_props;
  i = G_N_ELEMENTS (common_props);
  while (i-- > 0)
    if (map[i].id == id) {
      d (printf (map[i].name););
      return map[i].name;
    }

  d (printf ("_UNKNOWN_(0x%x %d)", id, id););

  return NULL;
}

static GValue *
msole_prop_parse(GsfMSOleMetaDataSection *section,
		 guint32 type,
		 guint8 const **data,
		 guint8 const *data_end)
{
  GValue *res;
  char *str;
  guint32 len;
  gsize gslen;
  gboolean const is_vector = type & LE_VT_VECTOR;

  g_return_val_if_fail (!(type & (unsigned)(~0x1fff)), NULL); /* not valid in a prop set */

  type &= 0xfff;

  if (is_vector) {
    unsigned i, n;

    g_return_val_if_fail (*data + 4 <= data_end, NULL);

    n = GSF_LE_GET_GUINT32 (*data);
    *data += 4;

    d (printf (" array with %d elem\n", n););
    for (i = 0 ; i < n ; i++) {
      GValue *v;
      d (printf ("\t[%d] ", i););
      v = msole_prop_parse (section, type, data, data_end);
      if (v) {
	/* FIXME: do something with it.  */
	if (G_IS_VALUE (v))
	  g_value_unset (v);
	g_free (v);
      }
    }
    return NULL;
  }

  res = g_new0 (GValue, 1);
  switch (type) {
  case LE_VT_EMPTY :		 d (puts ("VT_EMPTY"););
    /* value::unset == empty */
    break;

  case LE_VT_NULL :		 d (puts ("VT_NULL"););
    /* value::unset == null too :-) do we need to distinguish ? */
    break;

  case LE_VT_I2 :		 d (puts ("VT_I2"););
    g_return_val_if_fail (*data + 2 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT);
    g_value_set_int	(res, GSF_LE_GET_GINT16 (*data));
    *data += 2;
    break;

  case LE_VT_I4 :		 d (puts ("VT_I4"););
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT);
    g_value_set_int	(res, GSF_LE_GET_GINT32 (*data));
    *data += 4;
    break;

  case LE_VT_R4 :		 d (puts ("VT_R4"););
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    g_value_init (res, G_TYPE_FLOAT);
    g_value_set_float (res, GSF_LE_GET_FLOAT (*data));
    *data += 4;
    break;

  case LE_VT_R8 :		 d (puts ("VT_R8"););
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_DOUBLE);
    g_value_set_double (res, GSF_LE_GET_DOUBLE (*data));
    *data += 8;
    break;

  case LE_VT_CY :		 d (puts ("VT_CY"););
    break;

  case LE_VT_DATE :		 d (puts ("VT_DATE"););
    break;

  case LE_VT_BSTR :		 d (puts ("VT_BSTR"););
    break;

  case LE_VT_DISPATCH :	 d (puts ("VT_DISPATCH"););
    break;

  case LE_VT_BOOL :		 d (puts ("VT_BOOL"););
    g_return_val_if_fail (*data + 1 <= data_end, NULL);
    g_value_init (res, G_TYPE_BOOLEAN);
    g_value_set_boolean (res, **data ? TRUE : FALSE);
    *data += 1;
    break;

  case LE_VT_VARIANT :	 d (printf ("VT_VARIANT containing a "););
    g_free (res);
    type = GSF_LE_GET_GUINT32 (*data);
    *data += 4;
    return msole_prop_parse (section, type, data, data_end);

  case LE_VT_UI1 :		 d (puts ("VT_UI1"););
    g_return_val_if_fail (*data + 1 <= data_end, NULL);
    g_value_init (res, G_TYPE_UCHAR);
    g_value_set_uchar (res, (guchar)(**data));
    *data += 1;
    break;

  case LE_VT_UI2 :		 d (puts ("VT_UI2"););
    g_return_val_if_fail (*data + 2 <= data_end, NULL);
    g_value_init (res, G_TYPE_UINT);
    g_value_set_uint (res, GSF_LE_GET_GUINT16 (*data));
    *data += 2;
    break;

  case LE_VT_UI4 :		 d (puts ("VT_UI4"););
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    g_value_init (res, G_TYPE_UINT);
    *data += 4;
    d (printf ("%u\n", GSF_LE_GET_GUINT32 (*data)););
    break;

  case LE_VT_I8 :		 d (puts ("VT_I8"););
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT64);
    *data += 8;
    break;

  case LE_VT_UI8 :		 d (puts ("VT_UI8"););
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_UINT64);
    *data += 8;
    break;

  case LE_VT_LPSTR :		 d (puts ("VT_LPSTR"););
    /* be anal and safe */
    g_return_val_if_fail (*data + 4 <= data_end, NULL);

    len = GSF_LE_GET_GUINT32 (*data);

    g_return_val_if_fail (len < 0x10000, NULL);
    g_return_val_if_fail (*data + 4 + len*section->char_size <= data_end, NULL);

    gslen = 0;
    str = g_convert_with_iconv (*data + 4,
				len * section->char_size,
				section->iconv_handle, &gslen, NULL, NULL);
    len = (guint32)gslen;

    g_value_init (res, G_TYPE_STRING);
    g_value_set_string (res, str);
    g_free (str);
    *data += 4 + len;
    break;

  case LE_VT_LPWSTR : d (puts ("VT_LPWSTR"););
    /* be anal and safe */
    g_return_val_if_fail (*data + 4 <= data_end, NULL);

    len = GSF_LE_GET_GUINT32 (*data);

    g_return_val_if_fail (len < 0x10000, NULL);
    g_return_val_if_fail (*data + 4 + len <= data_end, NULL);

    str = g_convert (*data + 4, len*2,
		     "UTF-8", "UTF-16LE", &gslen, NULL, NULL);
    len = (guint32)gslen;

    g_value_init (res, G_TYPE_STRING);
    g_value_set_string (res, str);
    g_free (str);
    *data += 4 + len;
    break;

  case LE_VT_FILETIME :	 d (puts ("VT_FILETIME"););

    g_return_val_if_fail (*data + 8 <= data_end, NULL);

    g_value_init (res, G_TYPE_STRING);
    {
      /* ft * 100ns since Jan 1 1601 */
      guint64 ft = GSF_LE_GET_GUINT64 (*data);

      ft /= 10000000; /* convert to seconds */
#ifdef _MSC_VER
      ft -= 11644473600i64; /* move to Jan 1 1970 */
#else
      ft -= 11644473600ULL; /* move to Jan 1 1970 */
#endif

      str = g_strdup(ctime((time_t*)&ft));

      g_value_set_string (res, str);

      *data += 8;
      break;
    }
  case LE_VT_BLOB :		 d (puts ("VT_BLOB"););
    break;
  case LE_VT_STREAM :	 d (puts ("VT_STREAM"););
    break;
  case LE_VT_STORAGE :	 d (puts ("VT_STORAGE"););
    break;
  case LE_VT_STREAMED_OBJECT: d (puts ("VT_STREAMED_OBJECT"););
    break;
  case LE_VT_STORED_OBJECT :	 d (puts ("VT_STORED_OBJECT"););
    break;
  case LE_VT_BLOB_OBJECT :	 d (puts ("VT_BLOB_OBJECT"););
    break;
  case LE_VT_CF :		 d (puts ("VT_CF"););
    break;
  case LE_VT_CLSID :		 d (puts ("VT_CLSID"););
    *data += 16;
    break;

  case LE_VT_ERROR :
  case LE_VT_UNKNOWN :
  case LE_VT_DECIMAL :
  case LE_VT_I1 :
  case LE_VT_INT :
  case LE_VT_UINT :
  case LE_VT_VOID :
  case LE_VT_HRESULT :
  case LE_VT_PTR :
  case LE_VT_SAFEARRAY :
  case LE_VT_CARRAY :
  case LE_VT_USERDEFINED :
    warning ("type %d (0x%x) is not permitted in property sets",
	       type, type);
    g_free (res);
    res = NULL;
    break;

  default :
    warning ("Unknown property type %d (0x%x)", type, type);
    g_free (res);
    res = NULL;
  };

  d ( if (res != NULL && G_IS_VALUE (res)) {
    char *val = g_strdup_value_contents (res);
    d(printf ("%s\n", val););
    g_free (val);
  } else
      puts ("<unparsed>\n");
      );
  return res;
}

static GValue *
msole_prop_read (struct GsfInput *in,
		 GsfMSOleMetaDataSection *section,
		 GsfMSOleMetaDataProp    *props,
		 unsigned i)
{
  guint32 type;
  guint8 const *data;
  /* TODO : why size-4 ? I must be missing something */
  off_t size = ((i+1) >= section->num_props)
    ? section->size-4 : props[i+1].offset;
  char const *prop_name;

  g_return_val_if_fail (i < section->num_props, NULL);
  g_return_val_if_fail (size >= props[i].offset + 4, NULL);

  size -= props[i].offset; /* includes the type id */
  if (gsf_input_seek (in, section->offset+props[i].offset, SEEK_SET) ||
      NULL == (data = gsf_input_read (in, size, NULL))) {
    warning ("failed to read prop #%d", i);
    return NULL;
  }

  type = GSF_LE_GET_GUINT32 (data);
  data += 4;

  /* dictionary is magic */
  if (props[i].id == 0) {
    guint32 len, id, i, n;
    gsize gslen;
    char *name;
    guint8 const *start = data;

    g_return_val_if_fail (section->dict == NULL, NULL);

    section->dict = g_hash_table_new_full (
					   g_direct_hash, g_direct_equal,
					   NULL, g_free);

    n = type;
    for (i = 0 ; i < n ; i++) {
      id = GSF_LE_GET_GUINT32 (data);
      len = GSF_LE_GET_GUINT32 (data + 4);

      g_return_val_if_fail (len < 0x10000, NULL);

      gslen = 0;
      name = g_convert_with_iconv (data + 8,
				   len * section->char_size,
				   section->iconv_handle, &gslen, NULL, NULL);

      len = (guint32)gslen;
      data += 8 + len;

      d (printf ("\t%u == %s\n", id, name););
      g_hash_table_replace (section->dict,
			    GINT_TO_POINTER (id), name);

      /* MS documentation blows goats !
       * The docs claim there are padding bytes in the dictionary.
       * Their examples show padding bytes.
       * In reality non-unicode strings do not see to have padding.
       */
      if (section->char_size != 1 && (data - start) % 4)
	data += 4 - ((data - start) % 4);
    }

    return NULL;
  }

  d (printf ("%u) ", i););
  prop_name = msole_prop_id_to_gsf (section, props[i].id);

  d (printf (" @ %x %x = ", (unsigned)props[i].offset, (unsigned)size););
  return msole_prop_parse (section, type, &data, data + size);
}

static int
msole_prop_cmp (gconstpointer a, gconstpointer b)
{
  GsfMSOleMetaDataProp const *prop_a = a ;
  GsfMSOleMetaDataProp const *prop_b = b ;
  return prop_a->offset - prop_b->offset;
}

/**
 * gsf_msole_iconv_open_codepage_for_import :
 * @to:
 * @codepage :
 *
 * Returns an iconv converter for @codepage -> utf8.
 **/
static GIConv
gsf_msole_iconv_open_codepage_for_import(char const *to,
					 int codepage)
{
  GIConv iconv_handle;

  g_return_val_if_fail (to != NULL, (GIConv)(-1));
  /* sometimes it is stored as signed short */
  if (codepage == 65001 || codepage == -535) {
    iconv_handle = g_iconv_open (to, "UTF-8");
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  } else if (codepage != 1200 && codepage != 1201) {
    char* src_charset = g_strdup_printf ("CP%d", codepage);
    iconv_handle = g_iconv_open (to, src_charset);
    g_free (src_charset);
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  } else {
    char const *from = (codepage == 1200) ? "UTF-16LE" : "UTF-16BE";
    iconv_handle = g_iconv_open (to, from);
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  }

  /* Try aliases.  */
  if (codepage == 10000) {
    /* gnu iconv.  */
    iconv_handle = g_iconv_open (to, "MACROMAN");
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;

    /* glibc.  */
    iconv_handle = g_iconv_open (to, "MACINTOSH");
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  }

  warning ("Unable to open an iconv handle from codepage %d -> %s",
	     codepage, to);
  return (GIConv)(-1);
}

/**
 * gsf_msole_iconv_open_for_import :
 * @codepage :
 *
 * Returns an iconv converter for single byte encodings @codepage -> utf8.
 * 	Attempt to handle the semantics of a specification for multibyte encodings
 * 	since this is only supposed to be used for single bytes.
 **/
static GIConv
gsf_msole_iconv_open_for_import (int codepage)
{
  return gsf_msole_iconv_open_codepage_for_import ("UTF-8", codepage);
}





static struct EXTRACTOR_Keywords * process(struct GsfInput * in,
					   struct EXTRACTOR_Keywords * prev) {
  guint8 const *data = gsf_input_read (in, 28, NULL);
  guint16 version;
  guint32 os, num_sections;
  unsigned i, j;
  GsfMSOleMetaDataSection *sections;
  GsfMSOleMetaDataProp *props;

  if (NULL == data)
    return prev;

  /* NOTE : high word is the os, low word is the os version
   * 0 = win16
   * 1 = mac
   * 2 = win32
   */
  os = GSF_LE_GET_GUINT16 (data + 6);

  version = GSF_LE_GET_GUINT16 (data + 2);

  num_sections = GSF_LE_GET_GUINT32 (data + 24);
  if (GSF_LE_GET_GUINT16 (data + 0) != 0xfffe
      || (version != 0 && version != 1)
      || os > 2
      || num_sections > 100) { /* arbitrary sanity check */
    return prev;
  }

  /* extract the section info */
  sections = (GsfMSOleMetaDataSection *)g_alloca (sizeof (GsfMSOleMetaDataSection)* num_sections);
  for (i = 0 ; i < num_sections ; i++) {
    data = gsf_input_read (in, 20, NULL);
    if (NULL == data) {
      return prev;
    }
    if (!memcmp (data, component_guid, sizeof (component_guid)))
      sections [i].type = GSF_MSOLE_META_DATA_COMPONENT;
    else if (!memcmp (data, document_guid, sizeof (document_guid)))
      sections [i].type = GSF_MSOLE_META_DATA_DOCUMENT;
    else if (!memcmp (data, user_guid, sizeof (user_guid)))
      sections [i].type = GSF_MSOLE_META_DATA_USER;
    else {
      sections [i].type = GSF_MSOLE_META_DATA_USER;
      warning ("Unknown property section type, treating it as USER");
    }

    sections [i].offset = GSF_LE_GET_GUINT32 (data + 16);
#ifndef NO_DEBUG_OLE_PROPS
    d(printf ("0x%x\n", (guint32)sections [i].offset););
#endif
  }
  for (i = 0 ; i < num_sections ; i++) {
    if (gsf_input_seek (in, sections[i].offset, SEEK_SET) ||
	NULL == (data = gsf_input_read (in, 8, NULL))) {
      return prev;
    }

    sections[i].iconv_handle = (GIConv)-1;
    sections[i].char_size    = 1;
    sections[i].dict      = NULL;
    sections[i].size      = GSF_LE_GET_GUINT32 (data); /* includes header */
    sections[i].num_props = GSF_LE_GET_GUINT32 (data + 4);
    if (sections[i].num_props <= 0)
      continue;
    props = g_new (GsfMSOleMetaDataProp, sections[i].num_props);
    for (j = 0; j < sections[i].num_props; j++) {
      if (NULL == (data = gsf_input_read (in, 8, NULL))) {
	g_free (props);
	return prev;
      }

      props [j].id = GSF_LE_GET_GUINT32 (data);
      props [j].offset  = GSF_LE_GET_GUINT32 (data + 4);
    }

    /* order prop info by offset to facilitate bounds checking */
    qsort (props, sections[i].num_props,
	   sizeof (GsfMSOleMetaDataProp),
	   msole_prop_cmp);

    sections[i].iconv_handle = (GIConv)-1;
    sections[i].char_size = 1;
    for (j = 0; j < sections[i].num_props; j++) /* first codepage */
      if (props[j].id == 1) {
	GValue *v = msole_prop_read (in, sections+i, props, j);
	if (v != NULL) {
	  if (G_IS_VALUE (v)) {
	    if (G_VALUE_HOLDS_INT (v)) {
	      int codepage = g_value_get_int (v);
	      sections[i].iconv_handle = gsf_msole_iconv_open_for_import (codepage);
	      if (codepage == 1200 || codepage == 1201)
		sections[i].char_size = 2;
	    }
	    g_value_unset (v);
	  }
	  g_free (v) ;
	}
      }
    if (sections[i].iconv_handle == (GIConv)-1)
      sections[i].iconv_handle = gsf_msole_iconv_open_for_import (1252);

    for (j = 0; j < sections[i].num_props; j++) /* then dictionary */
      if (props[j].id == 0) {
	GValue *v = msole_prop_read (in, sections+i, props, j);
	if (v) {
	  if (G_VALUE_TYPE(v) == G_TYPE_STRING) {
	    gchar * contents = g_strdup_value_contents(v);
	    free(contents);
	  } else {	
	
	    /* FIXME: do something with non-strings...  */
	  }
	  if (G_IS_VALUE (v))
	    g_value_unset (v);
	  g_free (v);
	}
      }
    for (j = 0; j < sections[i].num_props; j++) /* the rest */
      if (props[j].id > 1) {	
	GValue *v = msole_prop_read (in, sections+i, props, j);
	if (v && G_IS_VALUE(v)) {
	  gchar * contents = NULL;
	  int pc;
	  int ipc;
	
	  if (G_VALUE_TYPE(v) == G_TYPE_STRING) {
	    contents = strdup(g_value_get_string(v));
	  } else {
	    /* convert other formats? */
	    contents = g_strdup_value_contents(v);
	  }	
	  pc = 0;
	  if (contents != NULL) {
	    for (ipc=strlen(contents)-1;ipc>=0;ipc--)
	      if ( (isprint(contents[ipc])) &&
		   (! isspace(contents[ipc])) )
		pc++;
	    if ( (strlen(contents) > 0) &&
		 (contents[strlen(contents)-1] == '\n') )
		 contents[strlen(contents)-1] = '\0';
	  }
	  if (pc > 0) {
	    int pos = 0;
	    const char * prop
	      = msole_prop_id_to_gsf(sections+i, props[j].id);
	    if (prop != NULL) {
	      while (tmap[pos].text != NULL) {
		if (0 == strcmp(tmap[pos].text,
				prop))
		  break;
		pos++;
	      }
	      if (tmap[pos].text != NULL)
		prev = addKeyword(prev,
				  contents,
				  tmap[pos].type);
	    }
	  }
	  if (contents != NULL)
	    free(contents);	
	}
	if (v) {
	  if (G_IS_VALUE (v))
	    g_value_unset (v);
	  g_free (v);
	}
      }

    gsf_iconv_close (sections[i].iconv_handle);
    g_free (props);
    if (sections[i].dict != NULL)
      g_hash_table_destroy (sections[i].dict);
  }
  switch (os) {
  case 0:
    prev = addKeyword(prev,
		      "Win16",
		      EXTRACTOR_OS);
    break;
  case 1:
    prev = addKeyword(prev,
		      "MacOS",
		      EXTRACTOR_OS);
    break;
  case 2:
    prev = addKeyword(prev,
		      "Win32",
		      EXTRACTOR_OS);
    break;
  }
  return prev;
}

static struct EXTRACTOR_Keywords * processSO(struct GsfInput * src,
					     struct EXTRACTOR_Keywords * prev) {
  off_t size;
  char * buf;

  size = gsf_input_size(src);
  if (size < 0x374) /* == 0x375?? */
    return prev;
  buf = malloc(size);
  gsf_input_read(src, size, buf);
  if ( (buf[0] != 0x0F) ||
       (buf[1] != 0x0) ||
       (0 != strncmp(&buf[2],
		     "SfxDocumentInfo",
		     strlen("SfxDocumentInfo"))) ||
       (buf[0x11] != 0x0B) ||
       (buf[0x13] != 0x00) || /* pw protected! */
       (buf[0x12] != 0x00) ) {
    free(buf);
    return prev;
  }
  buf[0xd3] = '\0';
  if (buf[0x94] + buf[0x93] > 0)
    prev = addKeyword(prev,
		      &buf[0x95],
		      EXTRACTOR_TITLE);
  buf[0x114] = '\0';
  if (buf[0xd5] + buf[0xd4] > 0)
    prev = addKeyword(prev,
		      &buf[0xd6],
		      EXTRACTOR_SUBJECT);
  buf[0x215] = '\0';
  if (buf[0x115] + buf[0x116] > 0)
    prev = addKeyword(prev,
		      &buf[0x117],
		      EXTRACTOR_COMMENT);
  buf[0x296] = '\0';
  if (buf[0x216] + buf[0x217] > 0)
    prev = addKeyword(prev,
		      &buf[0x218],
		      EXTRACTOR_KEYWORDS);
  /* fixme: do timestamps,
     mime-type, user-defined info's */

  free(buf);
  return prev;
}

struct EXTRACTOR_Keywords *
libextractor_ole2_extract(const char * filename,
			  char * date,
			  size_t size,
			  struct EXTRACTOR_Keywords * prev) {
  struct GsfInput   *input;
  struct GsfInfileMSOle * infile;
  struct GsfInput * src;
  const char * name;
  int i;

  input = gsf_input_new((const unsigned char*) date,
			(off_t) size,
			0);
  if (input == NULL)
    return prev;

  infile = gsf_infile_msole_new(input);
  if (infile == NULL)
    return prev;

  for (i=0;i<gsf_infile_msole_num_children(infile);i++) {
    name = gsf_infile_msole_name_by_index (infile, i);
    src = NULL;
    if (name == NULL)
      continue;
    if ( (0 == strcmp(name, "\005SummaryInformation"))
	 || (0 == strcmp(name, "\005DocumentSummaryInformation")) ) {
      src = gsf_infile_msole_child_by_index (infile, i);
      if (src != NULL)
	prev = process(src,
		       prev);
    }
    if (0 == strcmp(name, "SfxDocumentInfo")) {
      src = gsf_infile_msole_child_by_index (infile, i);
      if (src != NULL)
	prev = processSO(src,
			 prev);
    }
    if (src != NULL)
      gsf_input_finalize(src);
  }
  gsf_infile_msole_finalize(infile);
  return prev;
}

/* end of ole2extractor.c */
