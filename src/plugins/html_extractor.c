/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2009 Vidyut Samanta and Christian Grothoff

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
#include <string.h>
#include "convert.h"

static struct
{
  const char *name;
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

static const char *relevantTags[] = {
  "title",
  "meta",
  NULL,
};

typedef struct TI
{
  struct TI *next;
  const char *tagStart;
  const char *tagEnd;
  const char *dataStart;
  const char *dataEnd;
} TagInfo;




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
findInTags (TagInfo * t,
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
  TagInfo *tags;
  TagInfo *t;
  TagInfo tag;
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
              t = malloc (sizeof (TagInfo));
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
