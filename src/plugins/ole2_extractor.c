/*
     This file is part of libextractor.
     (C) 2004, 2005, 2006, 2007, 2009, 2012 Vidyut Samanta and Christian Grothoff

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

     This code makes extensive use of libgsf
     -- the Gnome Structured File Library
     Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)

     Part of this code was adapted from wordleaker.
*/
/**
 * @file plugins/ole2_extractor.c
 * @brief plugin to support OLE2 (DOC, XLS, etc.) files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include "convert.h"
#include <glib-object.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-msole-utils.h>


/**
 * Set to 1 to use our own GsfInput subclass which supports seeking
 * and thus can handle very large files.  Set to 0 to use the simple
 * gsf in-memory buffer (which can only access the first ~16k) for
 * debugging.
 */
#define USE_LE_INPUT 1


/**
 * Give the given UTF8 string to LE by calling 'proc'.
 *
 * @param proc callback to invoke
 * @param proc_cls closure for proc
 * @param phrase metadata string to pass; may include spaces
 *        just double-quotes or just a space in a double quote;
 *        in those cases, nothing should be done
 * @param type meta data type to use
 * @return if 'proc' returned 1, otherwise 0
 */
static int
add_metadata (EXTRACTOR_MetaDataProcessor proc,
	      void *proc_cls,
	      const char *phrase,
	      enum EXTRACTOR_MetaType type) 
{
  char *tmp;
  int ret;

  if (0 == strlen (phrase))
    return 0;
  if (0 == strcmp (phrase, "\"\""))
    return 0;
  if (0 == strcmp (phrase, "\" \""))
    return 0;
  if (0 == strcmp (phrase, " "))
    return 0;
  if (NULL == (tmp = strdup (phrase)))
    return 0;
  
  while ( (strlen (tmp) > 0) &&
	  (isblank ((unsigned char) tmp [strlen (tmp) - 1])) )
    tmp [strlen (tmp) - 1] = '\0';
  ret = proc (proc_cls, 
	      "ole2",
	      type,
	      EXTRACTOR_METAFORMAT_UTF8,
	      "text/plain",
	      tmp,
	      strlen (tmp) + 1);
  free (tmp);
  return ret;
}


/**
 * Entry in the map from OLE meta type  strings
 * to LE types.
 */
struct Matches 
{
  /**
   * OLE description.
   */
  const char *text;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
};


static struct Matches tmap[] = {
  { "Title", EXTRACTOR_METATYPE_TITLE },
  { "PresentationFormat", EXTRACTOR_METATYPE_FORMAT },
  { "Category", EXTRACTOR_METATYPE_SECTION },
  { "Manager", EXTRACTOR_METATYPE_MANAGER },
  { "Company", EXTRACTOR_METATYPE_COMPANY },
  { "Subject", EXTRACTOR_METATYPE_SUBJECT },
  { "Author", EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "Keywords", EXTRACTOR_METATYPE_KEYWORDS },
  { "Comments", EXTRACTOR_METATYPE_COMMENT },
  { "Template", EXTRACTOR_METATYPE_TEMPLATE },
  { "NumPages", EXTRACTOR_METATYPE_PAGE_COUNT },
  { "AppName", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE },
  { "RevisionNumber", EXTRACTOR_METATYPE_REVISION_NUMBER },
  { "NumBytes", EXTRACTOR_METATYPE_EMBEDDED_FILE_SIZE },
  { "CreatedTime", EXTRACTOR_METATYPE_CREATION_DATE },
  { "LastSavedTime" , EXTRACTOR_METATYPE_MODIFICATION_DATE },
  { "gsf:company", EXTRACTOR_METATYPE_COMPANY },
  { "gsf:character-count", EXTRACTOR_METATYPE_CHARACTER_COUNT },
  { "gsf:page-count", EXTRACTOR_METATYPE_PAGE_COUNT },
  { "gsf:line-count", EXTRACTOR_METATYPE_LINE_COUNT },
  { "gsf:word-count", EXTRACTOR_METATYPE_WORD_COUNT },
  { "gsf:paragraph-count", EXTRACTOR_METATYPE_PARAGRAPH_COUNT },
  { "gsf:last-saved-by", EXTRACTOR_METATYPE_LAST_SAVED_BY },
  { "gsf:manager", EXTRACTOR_METATYPE_MANAGER },
  { "dc:title", EXTRACTOR_METATYPE_TITLE },
  { "dc:creator", EXTRACTOR_METATYPE_CREATOR },
  { "dc:date", EXTRACTOR_METATYPE_UNKNOWN_DATE },
  { "dc:subject", EXTRACTOR_METATYPE_SUBJECT },
  { "dc:keywords", EXTRACTOR_METATYPE_KEYWORDS },
  { "dc:last-printed", EXTRACTOR_METATYPE_LAST_PRINTED },
  { "dc:description", EXTRACTOR_METATYPE_DESCRIPTION },
  { "meta:creation-date", EXTRACTOR_METATYPE_CREATION_DATE },
  { "meta:generator", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  { "meta:template", EXTRACTOR_METATYPE_TEMPLATE },
  { "meta:editing-cycles", EXTRACTOR_METATYPE_EDITING_CYCLES }, 
  /* { "Dictionary", EXTRACTOR_METATYPE_LANGUAGE },  */
  /* { "gsf:security", EXTRACTOR_SECURITY }, */
  /* { "gsf:scale", EXTRACTOR_SCALE }, // always "false"? */
  /* { "meta:editing-duration", EXTRACTOR_METATYPE_TOTAL_EDITING_TIME }, // encoding? */
  /* { "msole:codepage", EXTRACTOR_CHARACTER_SET }, */
  { NULL, 0 }
};


/**
 * Closure for 'process_metadata'.
 */
struct ProcContext
{
  /**
   * Function to call for meta data that was found.
   */
  EXTRACTOR_MetaDataProcessor proc;

  /**
   * Closure for 'proc'.
   */
  void *proc_cls;

  /**
   * Return value; 0 to continue to extract, 1 if we are done
   */
  int ret;
};


/**
 * Function invoked by 'gst_msole_metadata_read' with
 * metadata found in the document.
 *
 * @param key 'const char *' describing the meta data
 * @param value the UTF8 representation of the meta data
 * @param user_data our 'struct ProcContext' (closure)
 */
static void 
process_metadata (gpointer key,
		  gpointer value,
		  gpointer user_data) 
{
  const char *type = key;
  const GsfDocProp *prop = value;
  struct ProcContext *pc = user_data;
  const GValue *gval;
  char *contents;
  int pos;

  if ( (NULL == key) ||
       (NULL == value) )
    return;
  if (0 != pc->ret)
    return;
  gval = gsf_doc_prop_get_val (prop);

  if (G_VALUE_TYPE(gval) == G_TYPE_STRING) 
    {
      contents = strdup (g_value_get_string (gval));
    }
  else
    {
      /* convert other formats? */
      contents = g_strdup_value_contents (gval);
    }
  if (NULL == contents)
    return;
  if (0 == strcmp (type, "meta:generator"))
    {
      const char *mimetype = "application/vnd.ms-files";
      if ( (0 == strncmp (value, "Microsoft Word", 14)) ||
	   (0 == strncmp (value, "Microsoft Office Word", 21)))
	mimetype = "application/msword";
      else if ( (0 == strncmp(value, "Microsoft Excel", 15)) ||
		(0 == strncmp(value, "Microsoft Office Excel", 22)) )
	mimetype = "application/vnd.ms-excel";
      else if ( (0 == strncmp(value, "Microsoft PowerPoint", 20)) ||
		(0 == strncmp(value, "Microsoft Office PowerPoint", 27)) )
	mimetype = "application/vnd.ms-powerpoint";
      else if (0 == strncmp(value, "Microsoft Project", 17))
	mimetype = "application/vnd.ms-project";
      else if (0 == strncmp(value, "Microsoft Visio", 15))
	mimetype = "application/vnd.visio";
      else if (0 == strncmp(value, "Microsoft Office", 16))
	mimetype = "application/vnd.ms-office";
      if (0 != add_metadata (pc->proc,
			     pc->proc_cls, 
			     mimetype, 
			     EXTRACTOR_METATYPE_MIMETYPE))
	{
	  free (contents);
	  pc->ret = 1;
	  return;
	}
    }
  for (pos = 0; NULL != tmap[pos].text; pos++)
    if (0 == strcmp (tmap[pos].text,
		     type))
      break;
  if ( (NULL != tmap[pos].text) &&
       (0 != add_metadata (pc->proc, pc->proc_cls,
			   contents,
			   tmap[pos].type)) )
    {
      free (contents);
      pc->ret = 1;
      return;
    }
  free(contents);
}


/**
 * Function called on (Document)SummaryInformation OLE
 * streams.
 * 
 * @param in the input OLE stream
 * @param proc function to call on meta data found
 * @param proc_cls closure for proc
 * @return 0 to continue to extract, 1 if we are done
 */
static int
process (GsfInput *in,
	 EXTRACTOR_MetaDataProcessor proc,
	 void *proc_cls)
{
  struct ProcContext pc;
  GsfDocMetaData *sections;

  pc.proc = proc;
  pc.proc_cls = proc_cls;
  pc.ret = 0;
  sections = gsf_doc_meta_data_new ();
  if (NULL == gsf_msole_metadata_read (in, sections))
    {
      gsf_doc_meta_data_foreach (sections,
				 &process_metadata,
				 &pc);
    }
  g_object_unref (G_OBJECT (sections));
  return pc.ret;
}


/**
 * Function called on SfxDocumentInfo OLE
 * streams.
 * 
 * @param in the input OLE stream
 * @param proc function to call on meta data found
 * @param proc_cls closure for proc
 * @return 0 to continue to extract, 1 if we are done
 */
static int
process_star_office (GsfInput *src,
		     EXTRACTOR_MetaDataProcessor proc,
		     void *proc_cls) 
{
  off_t size = gsf_input_size (src);

  if ( (size < 0x374) || 
       (size > 4*1024*1024) )  /* == 0x375?? */
    return 0;
  {
    char buf[size];

    gsf_input_read (src, size, (unsigned char*) buf);
    if ( (buf[0] != 0x0F) ||
	 (buf[1] != 0x0) ||
	 (0 != strncmp (&buf[2],
			"SfxDocumentInfo",
			strlen ("SfxDocumentInfo"))) ||
	 (buf[0x11] != 0x0B) ||
	 (buf[0x13] != 0x00) || /* pw protected! */
	 (buf[0x12] != 0x00) ) 
      return 0;
    buf[0xd3] = '\0';
    if ( (buf[0x94] + buf[0x93] > 0) &&
	 (0 != add_metadata (proc, proc_cls,
			     &buf[0x95],
			     EXTRACTOR_METATYPE_TITLE)) )
      return 1;
    buf[0x114] = '\0';
    if ( (buf[0xd5] + buf[0xd4] > 0) &&
	 (0 != add_metadata (proc, proc_cls,
			     &buf[0xd6],
			     EXTRACTOR_METATYPE_SUBJECT)) )
      return 1;
    buf[0x215] = '\0';
    if ( (buf[0x115] + buf[0x116] > 0) &&
	 (0 != add_metadata (proc, proc_cls,
			     &buf[0x117],
			     EXTRACTOR_METATYPE_COMMENT)) )
      return 1;
    buf[0x296] = '\0';
    if ( (buf[0x216] + buf[0x217] > 0) &&
	 (0 != add_metadata(proc, proc_cls,
			    &buf[0x218],
			    EXTRACTOR_METATYPE_KEYWORDS)) )
      return 1;
    /* fixme: do timestamps,
       mime-type, user-defined info's */
  }
  return 0;
}


/**
 * We use "__" to translate using iso-639.
 * 
 * @param a string to translate
 * @return translated string
 */
#define __(a) dgettext("iso-639", a)


/**
 * Get the language string for the given language ID (lid)
 * value.
 * 
 * @param lid language id value
 * @return language string corresponding to the lid
 */
static const char * 
lid_to_language (unsigned int lid)
{
  switch (lid)
    {
    case 0x0400:
      return _("No Proofing");
    case 0x0401:
      return __("Arabic");
    case 0x0402:
      return __("Bulgarian");
    case 0x0403:
      return __("Catalan");
    case 0x0404:
      return _("Traditional Chinese");
    case 0x0804:
      return _("Simplified Chinese");
    case 0x0405:
      return __("Chechen");
    case 0x0406:
      return __("Danish");
    case 0x0407:
      return __("German");
    case 0x0807:
      return _("Swiss German");
    case 0x0408:
      return __("Greek");
    case 0x0409:
      return _("U.S. English");
    case 0x0809:
      return _("U.K. English");
    case 0x0c09:
      return _("Australian English");
    case 0x040a:
      return _("Castilian Spanish");
    case 0x080a:
      return _("Mexican Spanish");
    case 0x040b:
      return __("Finnish");
    case 0x040c:
      return __("French");
    case 0x080c:
      return _("Belgian French");
    case 0x0c0c:
      return _("Canadian French");
    case 0x100c:
      return _("Swiss French");
    case 0x040d:
      return __("Hebrew");
    case 0x040e:
      return __("Hungarian");
    case 0x040f:
      return __("Icelandic");
    case 0x0410:
      return __("Italian");
    case 0x0810:
      return _("Swiss Italian");
    case 0x0411:
      return __("Japanese");
    case 0x0412:
      return __("Korean");
    case 0x0413:
      return __("Dutch");
    case 0x0813:
      return _("Belgian Dutch");
    case 0x0414:
      return _("Norwegian Bokmal");
    case 0x0814:
      return __("Norwegian Nynorsk");
    case 0x0415:
      return __("Polish");
    case 0x0416:
      return __("Brazilian Portuguese");
    case 0x0816:
      return __("Portuguese");
    case 0x0417:
      return _("Rhaeto-Romanic");
    case 0x0418:
      return __("Romanian");
    case 0x0419:
      return __("Russian");
    case 0x041a:
      return _("Croato-Serbian (Latin)");
    case 0x081a:
      return _("Serbo-Croatian (Cyrillic)");
    case 0x041b:
      return __("Slovak");
    case 0x041c:
      return __("Albanian");
    case 0x041d:
      return __("Swedish");
    case 0x041e:
      return __("Thai");
    case 0x041f:
      return __("Turkish");
    case 0x0420:
      return __("Urdu");
    case 0x0421:
      return __("Bahasa");
    case 0x0422:
      return __("Ukrainian");
    case 0x0423:
      return __("Byelorussian");
    case 0x0424:
      return __("Slovenian");
    case 0x0425:
      return __("Estonian");
    case 0x0426:
      return __("Latvian");
    case 0x0427:
      return __("Lithuanian");
    case 0x0429:
      return _("Farsi");
    case 0x042D:
      return __("Basque");
    case 0x042F:
      return __("Macedonian");
    case 0x0436:
      return __("Afrikaans");
    case 0x043E:
      return __("Malayalam");
    default:
      return NULL;
    }
}


/**
 * Extract editing history from XTable stream.
 *
 * @param stream OLE stream to process
 * @param lcSttbSavedBy length of the revision history in bytes
 * @param fcSttbSavedBy offset of the revision history in the stream
 * @param proc function to call on meta data found
 * @param proc_cls closure for proc
 * @return 0 to continue to extract, 1 if we are done
 */
static int
history_extract (GsfInput *stream,
		 unsigned int lcbSttbSavedBy,
		 unsigned int fcSttbSavedBy,
		 EXTRACTOR_MetaDataProcessor proc,
		 void *proc_cls)
{
  unsigned int where;
  unsigned char *lbuffer;
  unsigned int i;
  unsigned int length;
  char *author;
  char *filename;
  char *rbuf;
  unsigned int nRev;
  int ret;

  /* goto offset of revision information */
  gsf_input_seek (stream, fcSttbSavedBy, G_SEEK_SET);
  if (gsf_input_remaining (stream) < lcbSttbSavedBy)
    return 0;
  if (NULL == (lbuffer = malloc (lcbSttbSavedBy)))
    return 0;
  /* read all the revision history */
  gsf_input_read (stream, lcbSttbSavedBy, lbuffer);
  /* there are n strings, so n/2 revisions (author & file) */
  nRev = (lbuffer[2] + (lbuffer[3] << 8)) / 2;
  where = 6;
  ret = 0;
  for (i=0; i < nRev; i++) 
    {
      if (where >= lcbSttbSavedBy)
	break;
      length = lbuffer[where++];
      if ( (where + 2 * length + 2 >= lcbSttbSavedBy) ||
	   (where + 2 * length + 2 <= where) )
	break;
      author = EXTRACTOR_common_convert_to_utf8 ((const char*) &lbuffer[where],
						 length * 2,
						 "UTF-16BE");
      where += length * 2 + 1;
      length = lbuffer[where++];
      if ( (where + 2 * length >= lcbSttbSavedBy) ||
	   (where + 2 * length + 1 <= where) ) 
	{
	  if (NULL != author)
	    free(author);
	  break;
	}
      filename = EXTRACTOR_common_convert_to_utf8 ((const char*) &lbuffer[where],
						   length * 2,
						   "UTF-16BE");
      where += length * 2 + 1;
      if ( (NULL != author) &&
	   (NULL != filename) )
	{
	  if (NULL != (rbuf = malloc (strlen (author) + strlen (filename) + 512)))
	    {
	      snprintf (rbuf, 
			512 + strlen (author) + strlen (filename),
			_("Revision #%u: Author `%s' worked on `%s'"),
			i,
			author,
			filename);
	      ret = add_metadata (proc, proc_cls,
				  rbuf,
				  EXTRACTOR_METATYPE_REVISION_HISTORY);    
	      free (rbuf);
	    }
	}
      if (NULL != author)
	free (author);
      if (NULL != filename)
	free (filename);
      if (0 != ret)
	break;
    }
  free (lbuffer);
  return ret;
}


/* *************************** custom GSF input method ***************** */

#define LE_TYPE_INPUT                  (le_input_get_type ())
#define LE_INPUT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), LE_TYPE_INPUT, LeInput))
#define LE_INPUT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), LE_TYPE_INPUT, LeInputClass))
#define IS_LE_INPUT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LE_TYPE_INPUT))
#define IS_LE_INPUT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), LE_TYPE_INPUT))
#define LE_INPUT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), LE_TYPE_INPUT, LeInputClass))

/**
 * Internal state of an "LeInput" object.
 */
typedef struct _LeInputPrivate 
{
  /**
   * Our extraction context.
   */
  struct EXTRACTOR_ExtractContext *ec;
} LeInputPrivate;


/**
 * Overall state of an "LeInput" object.
 */
typedef struct _LeInput 
{
  /**
   * Inherited state from parent (GsfInput).
   */
  GsfInput input;
  
  /*< private > */
  /**
   * Private state of the LeInput.
   */
  LeInputPrivate *priv;
} LeInput;


/**
 * LeInput's class state.
 */
typedef struct _LeInputClass
{
  /**
   * GsfInput is our parent class.
   */
  GsfInputClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
} LeInputClass;


/**
 * Constructor for LeInput objects. 
 *
 * @param ec extraction context to use
 * @return the LeInput, NULL on error
 */
GsfInput *
le_input_new (struct EXTRACTOR_ExtractContext *ec);


/**
 * Class initializer for the "LeInput" class.
 *
 * @param class class object to initialize
 */
static void
le_input_class_init (LeInputClass *class);


/**
 * Initialize internal state of fresh input object.
 *
 * @param input object to initialize
 */
static void
le_input_init (LeInput *input);


/**
 * Macro to create LeInput type definition and register the class.
 */
GSF_CLASS (LeInput, le_input, le_input_class_init, le_input_init, GSF_INPUT_TYPE)


/**
 * Duplicate input, leaving the new one at the same offset.
 *
 * @param input the input to duplicate
 * @param err location for error reporting, can be NULL
 * @return NULL on error (always)
 */
static GsfInput *
le_input_dup (GsfInput *input,
	      GError **err)
{
  if (NULL != err)
    *err = g_error_new (gsf_input_error_id (), 0,
			"dup not supported on LeInput");
  return NULL;
}


/**
 * Read at least num_bytes. Does not change the current position if
 * there is an error. Will only read if the entire amount can be
 * read. Invalidates the buffer associated with previous calls to
 * gsf_input_read.
 *
 * @param input
 * @param num_bytes
 * @param optional_buffer
 * @return buffer where num_bytes data are available, or NULL on error
 */
static const guint8 *
le_input_read (GsfInput *input,
	       size_t num_bytes,
	       guint8 *optional_buffer)
{
  LeInput *li = LE_INPUT (input);
  struct EXTRACTOR_ExtractContext *ec;
  void *buf;
  uint64_t old_off;
  ssize_t ret;
  
  ec = li->priv->ec;
  old_off = ec->seek (ec->cls, 0, SEEK_CUR);
  if (num_bytes 
      != (ret = ec->read (ec->cls,
			  &buf,
			  num_bytes)))
    {
      /* we don't support partial reads; 
	 most other GsfInput implementations in this case
	 allocate some huge temporary buffer just to avoid
	 the partial read; we might need to do that as well!? */
      ec->seek (ec->cls, SEEK_SET, old_off);
      return NULL;
    }
  if (NULL != optional_buffer)
    {
      memcpy (optional_buffer, buf, num_bytes);
      return optional_buffer;
    }
  return buf;
}


/**
 * Move the current location in an input stream
 *
 * @param input stream to seek
 * @param offset target offset
 * @param whence determines to what the offset is relative to
 * @return TRUE on error
 */
static gboolean
le_input_seek (GsfInput *input,
	       gsf_off_t offset,
	       GSeekType whence)
{
  LeInput *li = LE_INPUT (input);
  struct EXTRACTOR_ExtractContext *ec;
  int w;
  int64_t ret;

  ec = li->priv->ec;
  switch (whence)
    {
    case G_SEEK_SET:
      w = SEEK_SET;
      break;
    case G_SEEK_CUR:
      w = SEEK_CUR;
      break;
    case G_SEEK_END:
      w = SEEK_END;
      break;
    default:
      return TRUE;
    }
  if (-1 == 
      (ret = ec->seek (ec->cls,
		       offset,
		       w)))
    return TRUE;
  return FALSE;
}


/**
 * Class initializer for the "LeInput" class.
 *
 * @param class class object to initialize
 */
static void
le_input_class_init (LeInputClass *class)
{
  GsfInputClass *input_class;

  input_class = (GsfInputClass *) class;
  input_class->Dup = le_input_dup;
  input_class->Read = le_input_read;
  input_class->Seek = le_input_seek;
  g_type_class_add_private (class, sizeof (LeInputPrivate));
}


/**
 * Initialize internal state of fresh input object.
 *
 * @param input object to initialize
 */
static void
le_input_init (LeInput *input)
{
  LeInputPrivate *priv;

  input->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (input, LE_TYPE_INPUT,
				 LeInputPrivate);
  priv = input->priv;
  priv->ec = NULL;
}


/**
 * Creates a new LeInput object.
 *
 * @param ec extractor context to wrap
 * @return NULL on error
 */
GsfInput *
le_input_new (struct EXTRACTOR_ExtractContext *ec)
{
  LeInput *input;

  input = g_object_new (LE_TYPE_INPUT, NULL);
  gsf_input_set_size (GSF_INPUT (input),
		      ec->get_size (ec->cls));
  gsf_input_seek_emulate (GSF_INPUT (input),
			  0);
  input->input.name = NULL;
  input->input.container = NULL;
  input->priv->ec = ec;

  return GSF_INPUT (input);
}




/* *********************** end of custom GSF input method ************* */


/**
 * Main entry method for the OLE2 extraction plugin.  
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_ole2_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  GsfInput *input;
  GsfInfile *infile;
  GsfInput *src;
  const char *name;
  unsigned int i;
  unsigned int lcb;
  unsigned int fcb;
  const unsigned char *data512;
  unsigned int lid;
  const char *lang;
  int ret;
  void *data;
  uint64_t fsize;
  ssize_t data_size;

  fsize = ec->get_size (ec->cls);
  if (fsize < 512 + 898)
    {
      /* File too small for OLE2 */
      return; /* can hardly be OLE2 */
    }
  if (512 + 898 > (data_size = ec->read (ec->cls, &data, fsize)))
    {
      /* Failed to read minimum file size to buffer */
      return;
    }
  data512 = (const unsigned char*) data + 512;
  lid = data512[6] + (data512[7] << 8);
  if ( (NULL != (lang = lid_to_language (lid))) &&
       (0 != (ret = add_metadata (ec->proc, ec->cls,
				  lang,
				  EXTRACTOR_METATYPE_LANGUAGE))) )
    return;
  lcb = data512[726] + (data512[727] << 8) + (data512[728] << 16) + (data512[729] << 24);
  fcb = data512[722] + (data512[723] << 8) + (data512[724] << 16) + (data512[725] << 24);
  if (0 != ec->seek (ec->cls, 0, SEEK_SET))
    {
      /* seek failed!? */
      return;
    }
#if USE_LE_INPUT
  if (NULL == (input = le_input_new (ec)))
    {
      fprintf (stderr, "le_input_new failed\n");
      return;
    }
#else
  input = gsf_input_memory_new ((const guint8 *) data,
				data_size,
				FALSE);
#endif
  if (NULL == (infile = gsf_infile_msole_new (input, NULL)))
    {
      g_object_unref (G_OBJECT (input));
      return;
    }
  ret = 0;
  for (i=0;i<gsf_infile_num_children (infile);i++) 
    {
      if (0 != ret)
	break;
      if (NULL == (name = gsf_infile_name_by_index (infile, i)))
	continue;
      src = NULL;
      if ( ( (0 == strcmp (name, "\005SummaryInformation")) ||
	     (0 == strcmp (name, "\005DocumentSummaryInformation")) ) &&
	   (NULL != (src = gsf_infile_child_by_index (infile, i))) )
	ret = process (src,
		       ec->proc, 
		       ec->cls);
      if ( (0 == strcmp (name, "SfxDocumentInfo")) &&
	   (NULL != (src = gsf_infile_child_by_index (infile, i))) )
	ret = process_star_office (src,
				   ec->proc,
				   ec->cls);
      if (NULL != src)
	g_object_unref (G_OBJECT (src));
    }
  if (0 != ret)
    goto CLEANUP;

  if (lcb < 6)
    goto CLEANUP;
  for (i=0;i<gsf_infile_num_children (infile);i++) 
    {
      if (ret != 0)
	break;
      if (NULL == (name = gsf_infile_name_by_index (infile, i)))
	continue;
      if ( ( (0 == strcmp (name, "1Table")) ||
	     (0 == strcmp (name, "0Table")) ) &&
	   (NULL != (src = gsf_infile_child_by_index (infile, i))) )
	{
	  ret = history_extract (src,
				 lcb,
				 fcb,
				 ec->proc, ec->cls);
	  g_object_unref (G_OBJECT (src));
	}    
    }
 CLEANUP:
  g_object_unref (G_OBJECT (infile));
  g_object_unref (G_OBJECT (input));
}


/**
 * Custom log function we give to GSF to disable logging.
 *
 * @param log_domain unused
 * @param log_level unused
 * @param message unused
 * @param user_data unused
 */
static void 
nolog (const gchar *log_domain,
       GLogLevelFlags log_level,
       const gchar *message,
       gpointer user_data) 
{
  /* do nothing */
}


/**
 * OLE2 plugin constructor. Initializes glib and gsf, in particular
 * gsf logging is disabled.
 */
void __attribute__ ((constructor)) 
ole2_ltdl_init() 
{
  g_type_init();
#ifdef HAVE_GSF_INIT
  gsf_init();
#endif
  /* disable logging -- thanks, Jody! */
  g_log_set_handler ("libgsf:msole",
		     G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING,  
		     &nolog, NULL);
}


/**
 * OLE2 plugin destructor.  Shutdown of gsf.
 */
void __attribute__ ((destructor))
ole2_ltdl_fini() 
{
#ifdef HAVE_GSF_INIT
  gsf_shutdown();
#endif
}


/* end of ole2_extractor.c */
