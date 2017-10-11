/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2005, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.

 */
/**
 * @file plugins/html_extractor.c
 * @brief plugin to support HTML files
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include <magic.h>
#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>

/**
 * Mapping of HTML META names to LE types.
 */
static struct
{
  /**
   * HTML META name.
   */
  const char *name;

  /**
   * Corresponding LE type.
   */
  enum EXTRACTOR_MetaType type;
} tagmap[] = {
  { "author", EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "dc.author", EXTRACTOR_METATYPE_AUTHOR_NAME },
  { "title", EXTRACTOR_METATYPE_TITLE },
  { "dc.title", EXTRACTOR_METATYPE_TITLE},
  { "description", EXTRACTOR_METATYPE_DESCRIPTION },
  { "dc.description", EXTRACTOR_METATYPE_DESCRIPTION },
  { "subject", EXTRACTOR_METATYPE_SUBJECT},
  { "dc.subject", EXTRACTOR_METATYPE_SUBJECT},
  { "date", EXTRACTOR_METATYPE_UNKNOWN_DATE },
  { "dc.date", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  { "publisher", EXTRACTOR_METATYPE_PUBLISHER },
  { "dc.publisher", EXTRACTOR_METATYPE_PUBLISHER},
  { "rights", EXTRACTOR_METATYPE_RIGHTS },
  { "dc.rights", EXTRACTOR_METATYPE_RIGHTS },
  { "copyright", EXTRACTOR_METATYPE_COPYRIGHT },
  { "language", EXTRACTOR_METATYPE_LANGUAGE },
  { "keywords", EXTRACTOR_METATYPE_KEYWORDS },
  { "abstract", EXTRACTOR_METATYPE_ABSTRACT },
  { "formatter", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE },
  { "dc.creator", EXTRACTOR_METATYPE_CREATOR},
  { "dc.identifier", EXTRACTOR_METATYPE_URI },
  { "dc.format", EXTRACTOR_METATYPE_FORMAT },
  { NULL, EXTRACTOR_METATYPE_RESERVED }
};


/**
 * Global handle to MAGIC data.
 */
static magic_t magic;


/**
 * Map 'meta' tag to LE type.
 *
 * @param tag tag to map
 * @return EXTRACTOR_METATYPE_RESERVED if the type was not found
 */
static enum EXTRACTOR_MetaType
tag_to_type (const char *tag)
{
  unsigned int i;

  for (i=0; NULL != tagmap[i].name; i++)
    if (0 == strcasecmp (tag,
			 tagmap[i].name))
      return tagmap[i].type;
  return EXTRACTOR_METATYPE_RESERVED;
}


/**
 * Function called by libtidy for error reporting.
 *
 * @param doc tidy doc being processed
 * @param lvl report level
 * @param line input line
 * @param col input column
 * @param mssg message
 * @return FALSE (no output)
 */
static Bool TIDY_CALL
report_cb (TidyDoc doc,
	   TidyReportLevel lvl,
	   uint line,
	   uint col,
	   ctmbstr mssg)
{
  return 0;
}


/**
 * Input callback: get next byte of input.
 *
 * @param sourceData our 'struct EXTRACTOR_ExtractContext'
 * @return next byte of input, EndOfStream on errors and EOF
 */
static int TIDY_CALL
get_byte_cb (void *sourceData)
{
  struct EXTRACTOR_ExtractContext *ec = sourceData;
  void *data;

  if (1 !=
      ec->read (ec->cls,
		&data, 1))
    return EndOfStream;
  return *(unsigned char*) data;
}


/**
 * Input callback: unget last byte of input.
 *
 * @param sourceData our 'struct EXTRACTOR_ExtractContext'
 * @param bt byte to unget (ignored)
 */
static void TIDY_CALL
unget_byte_cb (void *sourceData, byte bt)
{
  struct EXTRACTOR_ExtractContext *ec = sourceData;

  (void) ec->seek (ec->cls, -1, SEEK_CUR);
}


/**
 * Input callback: check for EOF.
 *
 * @param sourceData our 'struct EXTRACTOR_ExtractContext'
 * @return true if we are at the EOF
 */
static Bool TIDY_CALL
eof_cb (void *sourceData)
{
  struct EXTRACTOR_ExtractContext *ec = sourceData;

  return ec->seek (ec->cls, 0, SEEK_CUR) == ec->get_size (ec->cls);
}


/**
 * Main entry method for the 'text/html' extraction plugin.
 *
 * @param ec extraction context provided to the plugin
 */
void
EXTRACTOR_html_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  TidyDoc doc;
  TidyNode head;
  TidyNode child;
  TidyNode title;
  TidyInputSource src;
  const char *name;
  TidyBuffer tbuf;
  TidyAttr attr;
  enum EXTRACTOR_MetaType type;
  ssize_t iret;
  void *data;
  const char *mime;

  if (-1 == (iret = ec->read (ec->cls,
			      &data,
			      16 * 1024)))
    return;
  if (NULL == (mime = magic_buffer (magic, data, iret)))
    return;
  if (0 != strncmp (mime,
		    "text/html",
		    strlen ("text/html")))
    return; /* not HTML */

  if (0 != ec->seek (ec->cls, 0, SEEK_SET))
    return; /* seek failed !? */

  tidyInitSource (&src, ec,
		  &get_byte_cb,
		  &unget_byte_cb,
		  &eof_cb);
  if (NULL == (doc = tidyCreate ()))
    return;
  tidySetReportFilter (doc, &report_cb);
  tidySetAppData (doc, ec);
  if (0 > tidyParseSource (doc, &src))
    {
      tidyRelease (doc);
      return;
    }
  if (1 != tidyStatus (doc))
    {
      tidyRelease (doc);
      return;
    }
  if (NULL == (head = tidyGetHead (doc)))
    {
      fprintf (stderr, "no head\n");
      tidyRelease (doc);
      return;
    }
  for (child = tidyGetChild (head); NULL != child; child = tidyGetNext (child))
    {
      switch (tidyNodeGetType(child))
	{
	case TidyNode_Root:
	  break;
	case TidyNode_DocType:
	  break;
	case TidyNode_Comment:
	  break;
	case TidyNode_ProcIns:
	  break;
	case TidyNode_Text:
	  break;
	case TidyNode_CDATA:
	  break;
	case TidyNode_Section:
	  break;
	case TidyNode_Asp:
	  break;
	case TidyNode_Jste:
	  break;
	case TidyNode_Php:
	  break;
	case TidyNode_XmlDecl:
	  break;
	case TidyNode_Start:
	case TidyNode_StartEnd:
	  name = tidyNodeGetName (child);
	  if ( (0 == strcasecmp (name, "title")) &&
	       (NULL != (title = tidyGetChild (child))) )
	    {
	      tidyBufInit (&tbuf);
	      tidyNodeGetValue (doc, title, &tbuf);
	      /* add 0-termination */
	      tidyBufPutByte (&tbuf, 0);
	      if (0 !=
		  ec->proc (ec->cls,
			    "html",
			    EXTRACTOR_METATYPE_TITLE,
			    EXTRACTOR_METAFORMAT_UTF8,
			    "text/plain",
			    (const char *) tbuf.bp,
			    tbuf.size))
		{
		  tidyBufFree (&tbuf);
		  goto CLEANUP;
		}
	      tidyBufFree (&tbuf);
	      break;
	    }
	  if (0 == strcasecmp (name, "meta"))
	    {
	      if (NULL == (attr = tidyAttrGetById (child,
						   TidyAttr_NAME)))
		break;
	      if (EXTRACTOR_METATYPE_RESERVED ==
		  (type = tag_to_type (tidyAttrValue (attr))))
		break;
	      if (NULL == (attr = tidyAttrGetById (child,
						   TidyAttr_CONTENT)))
		break;
	      name = tidyAttrValue (attr);
	      if (0 !=
		  ec->proc (ec->cls,
			    "html",
			    type,
			    EXTRACTOR_METAFORMAT_UTF8,
			    "text/plain",
			    name,
			    strlen (name) + 1))
		goto CLEANUP;
	      break;
	    }
	  break;
	case TidyNode_End:
	  break;
	default:
	  break;
	}
    }
 CLEANUP:
  tidyRelease (doc);
}



#if OLD


/* ******************** parser helper functions ************** */

static int
tagMatch (const char *tag, const char *s, const char *e)
{
  return (((e - s) == strlen (tag)) && (0 == strncasecmp (tag, s, e - s)));
}

static int
lookFor (char c, size_t * pos, const char *data, size_t size)
{
  size_t p = *pos;

  while ((p < size) && (data[p] != c))
    {
      if (data[p] == '\0')
        return 0;
      p++;
    }
  *pos = p;
  return p < size;
}

static int
skipWhitespace (size_t * pos, const char *data, size_t size)
{
  size_t p = *pos;

  while ((p < size) && (isspace ( (unsigned char) data[p])))
    {
      if (data[p] == '\0')
        return 0;
      p++;
    }
  *pos = p;
  return p < size;
}

static int
skipLetters (size_t * pos, const char *data, size_t size)
{
  size_t p = *pos;

  while ((p < size) && (isalpha ( (unsigned char) data[p])))
    {
      if (data[p] == '\0')
        return 0;
      p++;
    }
  *pos = p;
  return p < size;
}

static int
lookForMultiple (const char *c, size_t * pos, const char *data, size_t size)
{
  size_t p = *pos;

  while ((p < size) && (strchr (c, data[p]) == NULL))
    {
      if (data[p] == '\0')
        return 0;
      p++;
    }
  *pos = p;
  return p < size;
}

static void
findEntry (const char *key,
           const char *start,
           const char *end, const char **mstart, const char **mend)
{
  size_t len;

  *mstart = NULL;
  *mend = NULL;
  len = strlen (key);
  while (start < end - len - 1)
    {
      start++;
      if (start[len] != '=')
        continue;
      if (0 == strncasecmp (start, key, len))
        {
          start += len + 1;
          *mstart = start;
          if ((*start == '\"') || (*start == '\''))
            {
              start++;
              while ((start < end) && (*start != **mstart))
                start++;
              (*mstart)++;      /* skip quote */
            }
          else
            {
              while ((start < end) && (!isspace ( (unsigned char) *start)))
                start++;
            }
          *mend = start;
          return;
        }
    }
}

/**
 * Search all tags that correspond to "tagname".  Example:
 * If the tag is <meta name="foo" desc="bar">, and
 * tagname == "meta", keyname="name", keyvalue="foo",
 * and searchname="desc", then this function returns a
 * copy (!) of "bar".  Easy enough?
 *
 * @return NULL if nothing is found
 */
static char *
findInTags (struct TagInfo * t,
            const char *tagname,
            const char *keyname, const char *keyvalue, const char *searchname)
{
  const char *pstart;
  const char *pend;

  while (t != NULL)
    {
      if (tagMatch (tagname, t->tagStart, t->tagEnd))
        {
          findEntry (keyname, t->tagEnd, t->dataStart, &pstart, &pend);
          if ((pstart != NULL) && (tagMatch (keyvalue, pstart, pend)))
            {
              findEntry (searchname, t->tagEnd, t->dataStart, &pstart, &pend);
              if (pstart != NULL)
                {
                  char *ret = malloc (pend - pstart + 1);
		  if (ret == NULL)
		    return NULL;
                  memcpy (ret, pstart, pend - pstart);
                  ret[pend - pstart] = '\0';
                  return ret;
                }
            }
        }
      t = t->next;
    }
  return NULL;
}


/* mimetype = text/html */
int
EXTRACTOR_html_extract (const char *data,
			size_t size,
			EXTRACTOR_MetaDataProcessor proc,
			void *proc_cls,
			const char *options)
{
  size_t xsize;
  struct TagInfo *tags;
  struct TagInfo *t;
  struct TagInfo tag;
  size_t pos;
  size_t tpos;
  int i;
  char *charset;
  char *tmp;
  char *xtmp;
  int ret;

  ret = 0;
  if (size == 0)
    return 0;
  /* only scan first 32k */
  if (size > 1024 * 32)
    xsize = 1024 * 32;
  else
    xsize = size;
  tags = NULL;
  tag.next = NULL;
  pos = 0;
  while (pos < xsize)
    {
      if (!lookFor ('<', &pos, data, size))
        break;
      tag.tagStart = &data[++pos];
      if (!skipLetters (&pos, data, size))
        break;
      tag.tagEnd = &data[pos];
      if (!skipWhitespace (&pos, data, size))
        break;
    STEP3:
      if (!lookForMultiple (">\"\'", &pos, data, size))
        break;
      if (data[pos] != '>')
        {
          /* find end-quote, ignore escaped quotes (\') */
          do
            {
              tpos = pos;
              pos++;
              if (!lookFor (data[tpos], &pos, data, size))
                break;
            }
          while (data[pos - 1] == '\\');
          pos++;
          goto STEP3;
        }
      pos++;
      if (!skipWhitespace (&pos, data, size))
        break;
      tag.dataStart = &data[pos];
      if (!lookFor ('<', &pos, data, size))
        break;
      tag.dataEnd = &data[pos];
      i = 0;
      while (relevantTags[i] != NULL)
        {
          if ((strlen (relevantTags[i]) == tag.tagEnd - tag.tagStart) &&
              (0 == strncasecmp (relevantTags[i],
                                 tag.tagStart, tag.tagEnd - tag.tagStart)))
            {
              t = malloc (sizeof (struct TagInfo));
	      if (t == NULL)
		return 0;
              *t = tag;
              t->next = tags;
              tags = t;
              break;
            }
          i++;
        }
      /* abort early if we hit the body tag */
      if (tagMatch ("body", tag.tagStart, tag.tagEnd))
        break;
    }

  /* fast exit */
  if (tags == NULL)
    return 0;

  charset = NULL;
  /* first, try to determine mime type and/or character set */
  tmp = findInTags (tags, "meta", "http-equiv", "content-type", "content");
  if (tmp != NULL)
    {
      /* ideally, tmp == "test/html; charset=ISO-XXXX-Y" or something like that;
         if text/html is present, we take that as the mime-type; if charset=
         is present, we try to use that for character set conversion. */
      if (0 == strncasecmp (tmp, "text/html", strlen ("text/html")))
        ret = proc (proc_cls,
		    "html",
		    EXTRACTOR_METATYPE_MIMETYPE,
		    EXTRACTOR_METAFORMAT_UTF8,
		    "text/plain",
		    "text/html",
		    strlen ("text/html")+1);
      charset = strcasestr (tmp, "charset=");
      if (charset != NULL)
        charset = strdup (&charset[strlen ("charset=")]);
      free (tmp);
    }
  i = 0;
  while (tagmap[i].name != NULL)
    {
      tmp = findInTags (tags, "meta", "name", tagmap[i].name, "content");
      if ( (tmp != NULL) &&
	   (ret == 0) )
        {
	  if (charset == NULL)
	    {
	      ret = proc (proc_cls,
			  "html",
			  tagmap[i].type,
			  EXTRACTOR_METAFORMAT_C_STRING,
			  "text/plain",
			  tmp,
			  strlen (tmp) + 1);
	    }
	  else
	    {
	      xtmp = EXTRACTOR_common_convert_to_utf8 (tmp,
						       strlen (tmp),
						       charset);
	      if (xtmp != NULL)
		{
		  ret = proc (proc_cls,
			      "html",
			      tagmap[i].type,
			      EXTRACTOR_METAFORMAT_UTF8,
			      "text/plain",
			      xtmp,
			      strlen (xtmp) + 1);
		  free (xtmp);
		}
	    }
        }
      if (tmp != NULL)
	free (tmp);
      i++;
    }
  while (tags != NULL)
    {
      t = tags;
      if ( (tagMatch ("title", t->tagStart, t->tagEnd)) &&
	   (ret == 0) )
	{
	  if (charset == NULL)
	    {
	      xtmp = malloc (t->dataEnd - t->dataStart + 1);
	      if (xtmp != NULL)
		{
		  memcpy (xtmp, t->dataStart, t->dataEnd - t->dataStart);
		  xtmp[t->dataEnd - t->dataStart] = '\0';
		  ret = proc (proc_cls,
			      "html",
			      EXTRACTOR_METATYPE_TITLE,
			      EXTRACTOR_METAFORMAT_C_STRING,
			      "text/plain",
			      xtmp,
			      strlen (xtmp) + 1);
		  free (xtmp);
		}
	    }
	  else
	    {
	      xtmp = EXTRACTOR_common_convert_to_utf8 (t->dataStart,
						       t->dataEnd - t->dataStart,
						       charset);
	      if (xtmp != NULL)
		{
		  ret = proc (proc_cls,
			      "html",
			      EXTRACTOR_METATYPE_TITLE,
			      EXTRACTOR_METAFORMAT_UTF8,
			      "text/plain",
			      xtmp,
			      strlen (xtmp) + 1);
		  free (xtmp);
		}
	    }
	}
      tags = t->next;
      free (t);
    }
  if (charset != NULL)
    free (charset);
  return ret;
}
#endif


/**
 * Initialize glib and load magic file.
 */
void __attribute__ ((constructor))
html_gobject_init ()
{
  magic = magic_open (MAGIC_MIME_TYPE);
  if (0 != magic_load (magic, NULL))
    {
      /* FIXME: how to deal with errors? */
    }
}


/**
 * Destructor for the library, cleans up.
 */
void __attribute__ ((destructor))
html_ltdl_fini ()
{
  if (NULL != magic)
    {
      magic_close (magic);
      magic = NULL;
    }
}

/* end of html_extractor.c */
