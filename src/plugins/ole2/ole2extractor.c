/*
     This file is part of libextractor.
     (C) 2004, 2005, 2006 Vidyut Samanta and Christian Grothoff

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

     This code makes extensive use of libgsf
     -- the Gnome Structured File Library
     Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)

     Part of this code was borrowed from wordleaker.cpp. See also
     the README file in this directory.
*/

#include "platform.h"
#include "extractor.h"
#include "../convert.h"

#include <glib-object.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <gsf/gsf-utils.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-msole-utils.h>

#define DEBUG_OLE2 0

/* ******************************** main extraction code ************************ */

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
  keyword = malloc(sizeof(EXTRACTOR_KeywordList));
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

typedef struct {
  char * text;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tmap[] = {
  { "Title", EXTRACTOR_TITLE },
  { "PresentationFormat", EXTRACTOR_FORMAT },
  { "Category", EXTRACTOR_DESCRIPTION },
  { "Manager", EXTRACTOR_MANAGER },
  { "Company", EXTRACTOR_COMPANY },
  { "Subject", EXTRACTOR_SUBJECT },
  { "Author", EXTRACTOR_AUTHOR },
  { "Keywords", EXTRACTOR_KEYWORDS },
  { "Comments", EXTRACTOR_COMMENT },
  { "Template", EXTRACTOR_TEMPLATE },
  { "NumPages", EXTRACTOR_PAGE_COUNT },
  { "AppName", EXTRACTOR_SOFTWARE },
  { "RevisionNumber", EXTRACTOR_VERSIONNUMBER },
  { "Dictionary", EXTRACTOR_LANGUAGE },
  { "NumBytes", EXTRACTOR_SIZE },
  { "CreatedTime", EXTRACTOR_CREATION_DATE },
  { "LastSavedTime" , EXTRACTOR_MODIFICATION_DATE },
  { "gsf:company", EXTRACTOR_COMPANY },
  /*  { "gsf:security", EXTRACTOR_SECURITY }, */
  { "gsf:character-count", EXTRACTOR_CHARACTER_COUNT },
  { "gsf:page-count", EXTRACTOR_PAGE_COUNT },
  { "gsf:line-count", EXTRACTOR_LINE_COUNT },
  { "gsf:word-count", EXTRACTOR_WORD_COUNT },
  { "gsf:paragraph-count", EXTRACTOR_PARAGRAPH_COUNT },
  { "gsf:last-saved-by", EXTRACTOR_LAST_SAVED_BY },
  /* { "gsf:scale", EXTRACTOR_SCALE }, // always "false"? */
  { "gsf:manager", EXTRACTOR_MANAGER },
  { "dc:title", EXTRACTOR_TITLE },
  { "dc:creator", EXTRACTOR_CREATOR },
  { "dc:date", EXTRACTOR_DATE },
  { "dc:subject", EXTRACTOR_SUBJECT },
  { "dc:keywords", EXTRACTOR_KEYWORDS },
  { "dc:last-printed", EXTRACTOR_LAST_PRINTED },
  { "dc:description", EXTRACTOR_DESCRIPTION },
  { "meta:creation-date", EXTRACTOR_CREATION_DATE },
  /* { "meta:editing-duration", EXTRACTOR_TOTAL_EDITING_TIME }, // encoding? */
  { "meta:generator", EXTRACTOR_GENERATOR },
  { "meta:template", EXTRACTOR_TEMPLATE },
  /* { "meta:editing-cycles", EXTRACTOR_EDITING_CYCLES }, // usually "FALSE" */
  /* { "msole:codepage", EXTRACTOR_CHARACTER_SET }, */
  { NULL, 0 },
};

static void processMetadata(gpointer key,
			    gpointer value,
			    gpointer user_data) {
  struct EXTRACTOR_Keywords ** pprev = user_data;
  const char * type = key;
  const GsfDocProp * prop = value;
  const GValue * gval;
  char * contents;
  int pos;

  if ( (key == NULL) ||
       (value == NULL) )
    return;
  gval = gsf_doc_prop_get_val(prop);

  if (G_VALUE_TYPE(gval) == G_TYPE_STRING) {
    contents = strdup(g_value_get_string(gval));
  } else {
    /* convert other formats? */
    contents = g_strdup_value_contents(gval);
  }
  if (contents == NULL)
    return;
  if ( (strlen(contents) > 0) &&
       (contents[strlen(contents)-1] == '\n') )
    contents[strlen(contents)-1] = '\0';
  pos = 0;
  while (tmap[pos].text != NULL) {
    if (0 == strcmp(tmap[pos].text,
		    type))
      break;
    pos++;
  }
  if (tmap[pos].text != NULL)
    *pprev = addKeyword(*pprev,
			contents,
			tmap[pos].type);
#if DEBUG_OLE2
  else
    printf("No match for type `%s'\n",
	   type);
#endif
  free(contents);
}


static struct EXTRACTOR_Keywords *
process(GsfInput * in,
	struct EXTRACTOR_Keywords * prev) {
  GsfDocMetaData * sections;
  GError * error;

  sections = gsf_doc_meta_data_new();
  error = gsf_msole_metadata_read(in, sections);
  if (error == NULL) {
    gsf_doc_meta_data_foreach(sections,
			      &processMetadata,
			      &prev);
  }
  g_object_unref(G_OBJECT(sections));
  return prev;
}

static struct EXTRACTOR_Keywords *
processSO(GsfInput * src,
	  struct EXTRACTOR_Keywords * prev) {
  off_t size;
  char * buf;

  size = gsf_input_size(src);
  if (size < 0x374) /* == 0x375?? */
    return prev;
  buf = malloc(size);
  gsf_input_read(src, size, (unsigned char*) buf);
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

/* *************** wordleaker stuff *************** */

#define __(a) dgettext("iso-639", a)

static const char * lidToLanguage( unsigned int lid ) {
  switch ( lid ) {
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


static struct EXTRACTOR_Keywords *
history_extract(GsfInput * stream,
		unsigned int lcbSttbSavedBy,
		unsigned int fcSttbSavedBy,
		struct EXTRACTOR_Keywords * prev) {
  unsigned int where = 0;
  unsigned char * lbuffer;
  unsigned int i;
  unsigned int length;
  char * author;
  char * filename;
  char * rbuf;
  unsigned int nRev;

  // goto offset of revision
  gsf_input_seek(stream, fcSttbSavedBy, G_SEEK_SET);
  if (gsf_input_remaining(stream) < lcbSttbSavedBy)
    return prev;
  lbuffer = malloc(lcbSttbSavedBy);
  // read all the revision history
  gsf_input_read(stream, lcbSttbSavedBy, lbuffer);
  // there are n strings, so n/2 revisions (author & file)
  nRev = (lbuffer[2] + (lbuffer[3] << 8)) / 2;
  where = 6;
  for (i=0; i < nRev; i++) {	
    if (where >= lcbSttbSavedBy)
      break;
    length = lbuffer[where++];
    if ( (where + 2 * length + 2 >= lcbSttbSavedBy) ||
	 (where + 2 * length + 2 <= where) )
      break;
    author = convertToUtf8((const char*) &lbuffer[where],
			   length * 2,
			   "UTF-16BE");
    where += length * 2 + 1;
    length = lbuffer[where++];
    if ( (where + 2 * length >= lcbSttbSavedBy) ||
	 (where + 2 * length + 1 <= where) ) {
      free(author);
      break;
    }
    filename = convertToUtf8((const char*) &lbuffer[where],
			     length * 2,
			     "UTF-16BE");	
    where += length * 2 + 1;
    rbuf = malloc(strlen(author) + strlen(filename) + 512);
    snprintf(rbuf, 512 + strlen(author) + strlen(filename),
	     _("Revision #%u: Author '%s' worked on '%s'"),
	     i, author, filename);
    free(author);
    free(filename);
    prev = addKeyword(prev,
		      rbuf,
		      EXTRACTOR_REVISION_HISTORY);
    free(rbuf);
  }
  free(lbuffer);
  return prev;
}


/* ************** main method *********** */

struct EXTRACTOR_Keywords *
libextractor_ole2_extract(const char * filename,
			  const char * data,
			  size_t size,
			  struct EXTRACTOR_Keywords * prev) {
  GsfInput * input;
  GsfInfile * infile;
  GsfInput * src;
  GError * err = NULL;
  const char * name;
  const char * generator = NULL;
  int i;
  unsigned int lcb;
  unsigned int fcb;
  const unsigned char * data512;
  unsigned int lid;
  const char * lang;

  if (size < 512 + 898)
    return prev; /* can hardly be OLE2 */
  input = gsf_input_memory_new((const guint8 *) data,
			       (gsf_off_t) size,
			       FALSE);
  if (input == NULL)
    return prev;

  infile = gsf_infile_msole_new(input, &err);
  if (infile == NULL) {
    g_object_unref(G_OBJECT(input));
    return prev;
  }
  lcb = 0;
  fcb = 0;
  for (i=0;i<gsf_infile_num_children(infile);i++) {
    name = gsf_infile_name_by_index (infile, i);
    src = NULL;
    if (name == NULL)
      continue;
    if ( (0 == strcmp(name, "\005SummaryInformation"))
	 || (0 == strcmp(name, "\005DocumentSummaryInformation")) ) {
      src = gsf_infile_child_by_index (infile, i);
      if (src != NULL)
	prev = process(src,
		       prev);
    }
    if (0 == strcmp(name, "SfxDocumentInfo")) {
      src = gsf_infile_child_by_index (infile, i);
      if (src != NULL)
	prev = processSO(src,
			 prev);
    }
    if (src != NULL)
      g_object_unref(G_OBJECT(src));
  }

  data512 = (const unsigned char*) &data[512];
  lid = data512[6] + (data512[7] << 8);
  lcb = data512[726] + (data512[727] << 8) + (data512[728] << 16) + (data512[729] << 24);
  fcb = data512[722] + (data512[723] << 8) + (data512[724] << 16) + (data512[725] << 24);
  lang = lidToLanguage(lid);
  if (lang != NULL) {
    prev = addKeyword(prev,
		      lang,
		      EXTRACTOR_LANGUAGE);
  }
  if (lcb >= 6) {
    for (i=0;i<gsf_infile_num_children(infile);i++) {
      name = gsf_infile_name_by_index (infile, i);
      if (name == NULL)
	continue;
      if ( (0 == strcmp(name, "1Table")) ||
	   (0 == strcmp(name, "0Table")) ) {
	src = gsf_infile_child_by_index (infile, i);
	if (src != NULL) {
	  prev = history_extract(src,
				 lcb,
				 fcb,
				 prev);
	  g_object_unref(G_OBJECT(src));
	}
      }
    }
  }
  g_object_unref(G_OBJECT(infile));
  g_object_unref(G_OBJECT(input));

  /*
   * Hack to return an appropriate mimetype
   */
  generator = EXTRACTOR_extractLast(EXTRACTOR_GENERATOR, prev);
  if (NULL == generator) {
     /*
      * when very puzzled, just look at file magic number
      */
    if ( (8 < size)
	 && (0 == memcmp(data, "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1", 8)) )
      generator = "Microsoft Office";
  }

  if(NULL != generator) {
    const char * mimetype = "application/vnd.ms-files";

    if((0 == strncmp(generator, "Microsoft Word", 14)) ||
       (0 == strncmp(generator, "Microsoft Office Word", 21)))
      mimetype = "application/msword";
    else if((0 == strncmp(generator, "Microsoft Excel", 15)) ||
            (0 == strncmp(generator, "Microsoft Office Excel", 22)))
      mimetype = "application/vnd.ms-excel";
    else if((0 == strncmp(generator, "Microsoft PowerPoint", 20)) ||
            (0 == strncmp(generator, "Microsoft Office PowerPoint", 27)))
      mimetype = "application/vnd.ms-powerpoint";
    else if(0 == strncmp(generator, "Microsoft Project", 17))
      mimetype = "application/vnd.ms-project";
    else if(0 == strncmp(generator, "Microsoft Visio", 15))
      mimetype = "application/vnd.visio";
    else if(0 == strncmp(generator, "Microsoft Office", 16))
      mimetype = "application/vnd.ms-office";

    prev = addKeyword(prev, mimetype, EXTRACTOR_MIMETYPE);
  }

  return prev;
}

void __attribute__ ((constructor)) ole2_ltdl_init() {
#ifdef gsf_init
  gsf_init();
#endif
  // gsf_init_dynamic(NULL);
}

void __attribute__ ((destructor)) ole2_ltdl_fini() {
#ifdef gsf_init
  gsf_shutdown();
#endif
  // gsf_shutdown_dynamic(NULL);
}

/* end of ole2extractor.c */

