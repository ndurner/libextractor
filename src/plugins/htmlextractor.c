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

     Portions of this code were adapted from libhtmlparse by
     Mooneer Salem (mooneer@translator.cs).  The main changes
     to libhtmlparse were the removal of globals to make the
     code reentrant.
 */

#include "platform.h"
#include "extractor.h"
#include <string.h>

/* struct holding the arguments of tags */
struct ArgvTable {
  char *arg, *val;
};


/**
 * libhtmlparse has the callbacks defined as globals,
 * which is bad for making libextractor re-entrant.
 * We now put them all in one big table that is passed
 * around inside the parser.
 *
 *                        The CallBacks
 * You may call one ore several or even all callbacks. Except of the
 * XHTMLCallBack, all CallBacks will work as expected and described
 *
 * XHTMLCallBack:
 * The XHTMLCallBack is a special case, because you can decide, if the
 * XHTML specific tags should be handeled as a start- AND endtag, or
 * as an XHTML tag. If you call nothing, except start and endtag, the
 * behaviour is, that you'll get a start AND an endtag called back.
 * If you call XHTMLCallBack, it will only give you the XHTML call back.
 *
 * If you are in doubt or simply confused now, call XHTMLCallBack()
 */
typedef struct PC_ {
/* handle comments and javascript */
  int (*commentCallBack) (char *comment, struct PC_ * pc);
  int (*commentStartCallBack) (struct PC_ * pc);
  int (*commentEndCallBack) (struct PC_ * pc);

  /* Declaration e.g. <!DOCTYPE HTML ... */
  int (*declCallBack) (char *tag, /*@null@*/ struct ArgvTable *args, int numargs, struct PC_ * pc);

  /* Start tag e.g. <html>, with arguments, args may be NULL, numargs may be 0 */
  int (*startCallBack) (char *tag, /*@null@*/ struct ArgvTable *args, int numargs, struct PC_ * pc);

  /* End tag e.g. </html>*/
  int (*endCallBack) (char *tag, struct PC_ * pc);

  /* handle plain text */
  int (*textCallBack) (char *text, struct PC_ * pc);
  int (*textStartCallBack) (struct PC_ * pc);
  int (*textEndCallBack) (struct PC_ * pc);

  /* PHP inserts. BUG(?): if someone prints another PHP function from this PHP function
     our lib will get confused. */
  int (*phpCallBack) (char *text, struct PC_ * pc);

  /* empty tags like <hr/>, <br/>, with arguments, args may be NULL, numargs may be 0 */
  int (*XHTMLCallBack) (char *tag, /*@null@*/ struct ArgvTable *args, int numargs, struct PC_ * pc);

  /* XML tags <?xml>, with arguments, args may be NULL, numargs may be 0 */
  int (*xmlCallBack) (char *tag, /*@null@*/ struct ArgvTable *args, int numargs, struct PC_ * pc);

  /* entities like &auml;,&#228; text will inherit all chars between '&' and ';' */
  int (*entityCallBack) (char *text, struct PC_ * pc);

  /* and we also put some formaly static variables in this */

  /* needed to pass text in <script> tags verbatim */
  unsigned int lhtml_script_passthru;

  const char * end;

  int numArgs;

  int numArgsStatus;

  /**
   * 0: ignore, 1: add keyword
   */
  int nextTextAction;

  /**
   * If nextTextAction == 1, this gives the type of the
   * keyword.
   */
  EXTRACTOR_KeywordType nextKeywordType;

  /**
   * Result of the current pass.
   */
  struct EXTRACTOR_Keywords * result;

} ParserContext;


/**********************************************************************/


/* argument caching (e.g width="80%") */
static struct ArgvTable *addArgToTable(struct ArgvTable *args, char *arg, char *val,
				       struct PC_ * pc) {
  pc->numArgs++;
  if (args == NULL) {
    args = (struct ArgvTable*) calloc(1, 
				      sizeof(struct ArgvTable)*(pc->numArgs+1));
  } else {
    args = (struct ArgvTable*) realloc(args, 
				       sizeof(struct ArgvTable)*(pc->numArgs+1));
  }
  if (args == NULL) {
    fprintf(stderr,
	    _("Fatal: could not allocate (%s at %s:%d).\n"),
	    strerror(errno),
	    __FILE__, __LINE__);
    exit(EXIT_FAILURE);
  }
  args[pc->numArgs-1].arg = arg;
  args[pc->numArgs-1].val = val;
  return args;
}

/* clean up memory */
static void freeArgs (struct ArgvTable *args,
		      struct PC_ * pc) {
  int i;

  if (args != NULL) {
    for(i=0; i<pc->numArgs; i++) {
      free(args[i].arg);
      free(args[i].val);
    }
    free(args);
    args=NULL;
    pc->numArgs=0;
  }
}

/* prototype */
static const char *parseForEntities(const char *, struct PC_ * pc);


static const char *parseText(const char *html, struct PC_ * pc) {
  char *tmp;
  const char *tmp2;
  int ret=0;

  while( (html < pc->end) && isspace((int) *html)) html++;

  if (html >= pc->end) 
    return html;
  if (*html == '<') return html;

  tmp2 = html;
  while ( (html < pc->end) && (*html != '<') ) html++;

  tmp = (char *)calloc(1, (size_t)(html-tmp2+1));
  if (!tmp) return pc->end;

  memcpy(tmp, tmp2, (size_t)(html-tmp2));

  if (strlen(tmp) > 0) {
    if (pc->textStartCallBack) {
      ret = pc->textStartCallBack(pc);
      if (ret != 0) {
	free(tmp);
	return pc->end;
      }
    }
    if (pc->textCallBack) {
      if (pc->entityCallBack){ /* that is textCallBack(text)
			      with entityCallBack(entity) as an extrabonus */
	/*printf("entity is here\n");*/
	parseForEntities(tmp, pc);
      } else{
	ret = pc->textCallBack(tmp, pc);
	if (ret != 0) {
	  free(tmp);
	  return pc->end;
	}
      }
    }
    if (pc->textEndCallBack) {
      ret = pc->textEndCallBack(pc);
      if (ret != 0) {
	free(tmp);
	return pc->end;
      }
    }
  }
  free(tmp);
  if (html < pc->end-1)
    if (*(html+1) == '>') html += 2;
  return html;
}

static const char *parseComment (const char *html, struct PC_ * pc) {
  char *tmp;
  const char *tmp2;
  int ret=0;

  while ( (html < pc->end) &&
	  ( (*html == '-') || isspace((int)*html)) ) html++;

  tmp2 = html;
  while ( (html+2 < pc->end) && 
	  !(*html == '-' && *(html+1) == '-' && *(html+2) == '>')) html++;

  tmp = (char *)calloc(1, (size_t)(html-tmp2+1));
  if (!tmp) return pc->end;

  memcpy(tmp, tmp2, (size_t)(html-tmp2));

  if (html+3 < pc->end) {
    html += 3;
  } else {
    free(tmp);
    return pc->end;
  }

  if (pc->commentStartCallBack) {
    ret = pc->commentStartCallBack(pc);
    if (ret != 0) {
      free(tmp);
      return pc->end;
    }
  }
  if (pc->commentCallBack) {
    ret = pc->commentCallBack(tmp, pc);
    if (ret != 0) {
      free(tmp);
      return pc->end;
    }
  }
  if (pc->commentEndCallBack) {
    ret = pc->commentEndCallBack(pc);
    if (ret != 0) {
      free(tmp);
      return pc->end;
    }
  }
  free(tmp);
  return html;
}

static const char *parseEndTag(const char *html, struct PC_ * pc) {
  char *tmp;
  const char *tmp2;
  int ret=0;

  if (html >= pc->end)
    return html;

  html++;
  tmp2 = html;
  while(html < pc->end && *html != '>') html++;

  tmp =(char *) calloc(1, (size_t)(html-tmp2+1));
  if (!tmp) return pc->end;

  memcpy(tmp, tmp2, (size_t)(html-tmp2));

  if (pc->endCallBack) {
    ret = pc->endCallBack(tmp,pc);
    if (ret != 0) {
      free(tmp);
      return pc->end;
    }
  }
  if ( (html < pc->end) && (*html == '>') ) html++;
  free(tmp);
  return html;
}

static const char *parsePHP(const char *html, struct PC_ * pc) {
  const char *tmp;
  char *tmp2;
  int ret=0;

  html += 4;
  while(html < pc->end && isspace((int)*html)) html++;

  tmp = html;

  while ( (html+1 < pc->end) && !(*html == '?' && *(html+1) == '>')) html++;
  tmp2 = (char *)calloc(1, (size_t)(html-tmp+1));
  if (!tmp2) return pc->end;

  memcpy(tmp2, tmp, (size_t)(html-tmp));

  if (pc->phpCallBack) {
    ret = pc->phpCallBack(tmp2, pc);
    if (ret != 0) {
      free(tmp2);
      return pc->end;
    }
  }
  free(tmp2);
  html += 2;
  return html;
}

/* parse the XML tag itself */
static const char *parseXMLtag(const char *html, struct PC_ * pc) {
  char *tag, *name, *value;
  const char *tmp;
  int ret;
  struct ArgvTable *tmp2 = NULL;

  pc->numArgs = 0;
  tmp = html;
  while (html < pc->end && !isspace((int)*html) && *html != '>') html++;

  /* you may want to upper/lower tags, so I leave the tag itself untouched */
  tag = (char *)calloc(1, (size_t)(html-tmp+1));
  if (!tag) {
    return pc->end;
  }
  memcpy(tag, tmp, (size_t)(html-tmp)); 
  if (html >= pc->end) {
    free(tag);
    return html;
  }
  if (*html == '>') {
    if (pc->xmlCallBack != NULL) {
      ret = pc->xmlCallBack(tag, NULL, 0, pc);
      free(tag);
      if (*html == '>') html++;
      return ((ret != 0) ? pc->end : html);
    }
  }
  while((html < pc->end) && isspace((int)*html)) html++;

  while( (html < pc->end) && *html != '>' ) {
    while ( (html < pc->end) && (isspace((int)*html)) ) html++;
    if (html >= pc->end) 
      return pc->end;
    if (*html == '>') break;

    tmp = html;
    while( (html < pc->end) && !isspace((int)*html) && *html != '=' && *html != '>') html++;
    name = (char *)calloc(1, (size_t)(html-tmp+1));
    if (!name) {
      free(tag);
      tag = NULL;
      return pc->end;
    }
    memcpy(name, tmp, (size_t)(html-tmp));
    if (isspace((int)*html)) {
      tmp2 = addArgToTable(tmp2, name, NULL, pc);
      while(html < pc->end && isspace((int)*html) && *html != '>') html++;
    }
    if (html >= pc->end) {
      free(tag);
      return html;
    }
    if (*html == '>') {
      tmp2 = addArgToTable(tmp2, name, NULL, pc);
      html++;
      break;
    }
    if (*html == '=') html++;
    if (html >= pc->end) {
      free(tag);
      return html;
    }
    if (*html != '"' && *html != '\'') {
      tmp = html;
      while(html < pc->end && *html != '>' && !isspace((int)*html)) html++;
      value = (char *)calloc(1, (size_t)(html-tmp+1));
      if (!value) {
	free(name);
	name = NULL;
	free(tag);
	tag = NULL;
	
	if (tmp2 != NULL) {
	  freeArgs(tmp2, pc);
	  tmp2 = NULL;
	}
	return pc->end;
      }
      memcpy(value, tmp, (size_t)(html-tmp));
      tmp2 = addArgToTable(tmp2, name, value, pc);
    } else if (*html == '"') {
      html++;
      if (html >= pc->end) {
	free(tag);
	return html;
      }
      tmp = html;
      while(html < pc->end && !(*html == '"' && *(html-1) != '\\')) html++;
      value = (char *) calloc(1, (size_t)(html-tmp+1));
      if (!value) {
	free(name);
	name = NULL;
	free(tag);
	tag = NULL;
	
	if (tmp2 != NULL) {
	  freeArgs(tmp2, pc);
	  tmp2 = NULL;
	}
	return pc->end;
      }
      memcpy(value, tmp, (size_t)(html-tmp));
      if (html < pc->end)
	html++;
      tmp2 = addArgToTable(tmp2, name, value, pc);
    } else if (*html == '\'') {
      html++;
      if (html >= pc->end) {
	free(tag);
	return html;
      }
      tmp = html;
      while(html < pc->end && !(*html == '\'' && *(html-1) != '\\')) html++;

      value =  (char *)calloc(1, (size_t)(html-tmp+1));
      if (!value) {
	free(name);
	name = NULL;
	free(tag);
	tag = NULL;	
	if (tmp2 != NULL) {
	  freeArgs(tmp2, pc);
	  tmp2 = NULL;
	}
	return pc->end;
      }
      memcpy(value, tmp, (size_t)(html-tmp));
      if (html < pc->end)
	html++;
      tmp2 = addArgToTable(tmp2, name, value, pc);
    }
    tmp = NULL;
    value = NULL;
    name = NULL;
  }
  if (html < pc->end) html++;
  ret = pc->xmlCallBack(tag, tmp2, pc->numArgs, pc);
  if (tmp2 != NULL) {
    freeArgs(tmp2, pc);
    tmp2 = NULL;
  }
  free(tag);
  tag = NULL;
  pc->numArgsStatus=0;
  return (ret != 0 ? pc->end : html);
}

/* cannibalistic function, munches the actuall tag */
static const char *eatUp(const char *html,
			 struct PC_ * pc){
  while ( (html < pc->end) &&
	  (*html != '>') ) {
    html++;
  }
  if (html < pc->end)
    html++;
  return html;
}

/* cannibalistic function, munches the actuall text */
static const char *eatUpText(const char *html,
			     struct PC_ * pc){
  while ( (html < pc->end)
	  && (*html != '<') )
    html++;
  return html;
}


/* decides, if a found '?' leads to PHP or XML if requisited
   otherwise it gormandizes them up. *burps* */
static const char *parseXML(const char *html, struct PC_ * pc) {
  /* conditional expressions inside a conditional expression
     don't try _this_ at home kids! ;-) */
  if (html+1 >= pc->end) 
    return html;
  html=(((tolower((int)(*(html+1))))==(int)('p')) ?
	( (pc->phpCallBack) ? parsePHP   (html, pc) :  eatUp(html, pc) ) :
	( (pc->xmlCallBack) ? parseXMLtag(html, pc) :  eatUp(html, pc) )   );
  return html;
}

static const char *parseStartTag (const char *html, struct PC_ * pc) {
  char *tag, *name, *value;
  const char * tmp;
  const char * start = html;
  int ret = 0;
  struct ArgvTable *tmp2 = NULL;

  pc->numArgs = 0;
  tmp = html;
  while(html < pc->end && !isspace((int)*html) &&
	*html != '>' && *html != '/') html++;
  
  tag = (char *)calloc(1, (size_t)(html-tmp+1));
  if (!tag) {
    return pc->end;
  }
  memcpy(tag, tmp, (size_t)(html-tmp));

  if (strncasecmp("script", tag, 6) == 0) {
    pc->lhtml_script_passthru = 1;
  }
  else if (strncasecmp("pre", tag, 3) == 0) {
    pc->lhtml_script_passthru = 2;
  }
  if (html >= pc->end)
    return pc->end;

  if (*html == '>') {
    if (pc->startCallBack) {
      ret = pc->startCallBack(tag, NULL, 0, pc);
      free(tag);
      tag = NULL;

      /* this check is redundant */
      /* if (*html == '>') */ html++;
      return((ret != 0) ? pc->end : html);
    }
  }
  else if (*html == '/' ) {   /* XHTML empty tag like <hr/>, <br/>*/
    /**********************************************
     * You may choose now between two behaviors    *
     * of libhtmlparse to handle XHTML empty tags: *
     * a) call XHTMLCallBack                       *
     * b) call start- AND endCallBack              *
     ***********************************************/
    if (pc->startCallBack != NULL && !(pc->XHTMLCallBack)) {
      ret = pc->startCallBack(tag, NULL, 0, pc);
    }
    if (pc->endCallBack != NULL && ret==0 && !(pc->XHTMLCallBack)) {
      ret = pc->endCallBack(tag, pc);
    }
    if(pc->XHTMLCallBack){
      ret = pc->XHTMLCallBack(tag, NULL, 0, pc);
    }

    free(tag);
    tag = NULL;

    html += 2;
    return((ret != 0) ? pc->end : html);
  }

  while(html < pc->end && isspace((int)*html)) html++;

  while(html < pc->end && *html != '>' ) {
    while ( (html < pc->end) && (isspace((int)*html))) html++;
    if (html+1 >= pc->end)
      break;
    if (*html == '>') 
      break;

    if (*html == '/' && *(html+1) == '>') {
      html++; 
      break;
    }

    tmp = html;
    while(html < pc->end && !isspace((int)*html) &&
	  *html != '=' && *html != '>') html++;
    name = (char *)calloc(1, (size_t)(html-tmp+1));
    if (!name) {
      free(tag);
      return pc->end;
    }

    memcpy(name, tmp, (size_t)(html-tmp));
    if (html >= pc->end) {
      free(tag);
      return pc->end;
    }
    if (isspace((int)*html)) {
      const char *x = html;
      while (x < pc->end && *x != '>' && *x != '=') x++;
      if (x >= pc->end) {
	free(tag);
	return pc->end;
      }
      if (*x == '=') {
	html = x;
	goto namevalue;
      }
      tmp2 = addArgToTable(tmp2, name, NULL, pc);
      while(html+1 < pc->end && isspace((int)*html) &&
	    *html != '>' &&
	    !(*html == '/' && *(html+1) == '>'))
	html++;
    } else {
      
      if (*html == '/') {
	html++;
	break;
      }

      /* html++ is repeated after the while loop
       * and may cause deletion of important info */
      if (*html == '>') {
	tmp2 = addArgToTable(tmp2, name, NULL, pc);
	/*html++;*/
	break;
      }

    namevalue:
      if (*html == '=') html++;

      while ( (html < pc->end) && (isspace(*html))) html++;

      if (html >= pc->end) {
	free(tag);
	return pc->end;
      }
      if (*html != '\'') {
	tmp = html;
	while(html+1 < pc->end && *html != '>' &&
	      !isspace((int)*html) &&
	      !(*html == '/' && *(html+1) == '>'))
	  html++;
	value = (char *)calloc(1, (size_t)(html-tmp+1));
	if (value == NULL) {
	  free(name);
	  name = NULL;
	  free(tag);
	  tag = NULL;
	  
	  freeArgs(tmp2, pc);
	  return pc->end;
	}	
	memcpy(value, tmp, (size_t)(html-tmp));
	tmp2 = addArgToTable(tmp2, name, value, pc);
      } else if (*html == '"') {
	html++;
	tmp = html;
	while (html < pc->end &&
	       !(*html == '"' && *(html-1) != '\\'))
	  html++;
	value = (char *) calloc(1, (size_t)(html-tmp+1));
	if (value == NULL) {
	  free(name);
	  name = NULL;
	  free(tag);
	  tag = NULL;
	  
	  freeArgs(tmp2, pc);
	  return pc->end;
	}
	
	memcpy(value, tmp, (size_t)(html-tmp));
	if (html < pc->end)
	  html++;
	tmp2 = addArgToTable(tmp2, name, value, pc);
      } else if (*html == '\'') {
	html++;
	tmp = html;
	while(html < pc->end && !(*html == '\'' &&
				 *(html-1) != '\\')) html++;
	
	value = (char *)calloc(1, (size_t)(html-tmp+1));
	if (value == NULL) {
	  free(name);
	  name = NULL;
	  free(tag);
	  tag = NULL;
	
	  freeArgs(tmp2, pc);
	  return pc->end;
	}
	
	memcpy(value, tmp, (size_t)(html-tmp));
	if (html < pc->end)
	  html++;
	tmp2 = addArgToTable(tmp2, name, value, pc);
      }
      tmp = NULL;
    }
  }
  if (html < pc->end) html++;

  if (html - start > 2) {
    if (pc->startCallBack != NULL && (*(html-2)!='/')) {
      ret = pc->startCallBack(tag, tmp2, pc->numArgs, pc);
    }
    if (pc->endCallBack != NULL && ret==0 && *(html-2)=='/'
	&& !(pc->XHTMLCallBack)) {
      ret = pc->endCallBack(tag, pc);
    }
    /* these tags may have arguments too, e.g. <hr noshade/> */
    if (pc->XHTMLCallBack != NULL && *(html-2)=='/') {
      ret = pc->XHTMLCallBack(tag, tmp2, pc->numArgs, pc);
    }
  }
  if(tmp2 != NULL){
    freeArgs(tmp2, pc);
  }
  free(tag);
  tag = NULL;

  pc->numArgsStatus=0;

  /* this is a bad hack, feel free to write a better one (maybe a more readable one? ;-)*/
  return
    (pc->XHTMLCallBack != NULL) ?
    (html) :
    ((ret != 0) ? pc->end : html);
}

static const char *parseDecl(const char *html, struct PC_ * pc) {
  char *tag, *name, *value;
  const char *tmp;
  int ret=0;
  struct ArgvTable *tmp2 = NULL;

  pc->numArgs = 0;
  tmp = html;
  while(html < pc->end && !isspace((int)*html) && *html != '>') html++;
  if (html >= pc->end)
    return pc->end;
  tag = (char *)calloc(1, (size_t)(html-tmp+1));
  if (!tag) {
    return pc->end;
  }

  memcpy(tag, tmp, (size_t)(html-tmp));

  if (*html == '>') {
    if (pc->declCallBack) {
      ret = pc->declCallBack(tag, NULL, 0, pc);
      free(tag);
      tag = NULL;

      if (*html == '>') html++;
      return((ret != 0) ? pc->end : html);
    }
  }

  while(html < pc->end && isspace((int)*html)) html++;

  while(html < pc->end && *html != '>') {
    while ( (html<pc->end) && (isspace((int)*html)) ) html++;
    if (html >= pc->end)
      return pc->end;
    if (*html == '>') break;
    tmp = html;
    switch(*tmp) {
    case '\'' :
      html++;
      tmp = html;
      while (html < pc->end && !(*html == '\'' && *html != '\\'))
	html++;
      break;
    case '"'  :
      html++;
      tmp = html;
      while(html < pc->end && !(*html == '"' && *html != '\\'))
	html++;
      break;
    default  :
      while(html < pc->end && !isspace((int)*html) && *html != '=' && *html != '>')
	html++;
      break;
    }

    name = (char *) calloc(1, (size_t)(html-tmp+1));
    if (!name) {
      free(tag);
      tag = NULL;
      return pc->end;
    }

    memcpy(name, tmp, (size_t)(html-tmp));
    if (html >= pc->end) {
      free(tag);
      free(name);
      return pc->end;
    }

    if (isspace((int)*html)) {
      tmp2 = addArgToTable(tmp2, name, NULL, pc);
      while (html < pc->end && isspace((int)*html) && *html != '>')
	html++;
      continue;
    }
    if (html >= pc->end) {
      free(tag);
      free(name);
      return pc->end;
    }

    if (*html == '>') {
      tmp2 = addArgToTable(tmp2, name, NULL, pc);
      html++;
      break;
    }
    if (html+1 >= pc->end) {
      free(tag);
      free(name);
      return pc->end;
    }

    if (*(html+1) == '>') {
      tmp2 = addArgToTable(tmp2, name, NULL, pc);
      html += 2;
      break;
    }
    if (html >= pc->end) {
      free(tag);
      free(name);
      return pc->end;
    }

    if (*html == '=') html++;
    switch(*html){
    case '\''  :
      html++;
      tmp = html;
      while(html < pc->end && !(*html == '\'' && *(html-1) != '\\'))
	html++;

      value = (char *) calloc(1, (size_t)(html-tmp+1));
      if (!value) {
	free(name);
	name = NULL;
	free(tag);
	tag = NULL;
	
	freeArgs(tmp2, pc);
	return pc->end;
      }

      memcpy(value, tmp, (size_t)(html-tmp));
      if (html < pc->end)
	html++;
      tmp2 = addArgToTable(tmp2, name, value, pc);
      break;
    case '"'  :
      html++;
      tmp = html;
      while (html < pc->end && !(*html == '"' && *(html-1) != '\\'))
	html++;
      value =  (char *)calloc(1, (size_t)(html-tmp+1));
      if (!value) {
	free(name);       
	free(tag);	
	freeArgs(tmp2, pc);
	return pc->end;
      }

      memcpy(value, tmp, (size_t)(html-tmp));
      if (html < pc->end)
	html++;
      tmp2 = addArgToTable(tmp2, name, value, pc);
      break;
    default  :
      html++;
      tmp = html;
      while (html < pc->end && *html != '>' && !isspace((int)*html))
	html++;
      value = (char *) calloc(1, (size_t)(html-tmp+1));
      if (!value) {
	free(name);
	name = NULL;
	free(tag);
	tag = NULL;
	
	freeArgs(tmp2, pc);
	return pc->end;
      }

      memcpy(value, tmp, (size_t)(html-tmp));
      tmp2 = addArgToTable(tmp2, name, value, pc);
      break;
    }
    tmp = NULL;
  }

  if (html < pc->end) html++;

  if (pc->declCallBack) {
    ret = pc->declCallBack(tag, tmp2, pc->numArgs, pc);
    freeArgs(tmp2, pc);
    free(tag);
    tag = NULL;
    return((ret != 0) ? pc->end : html);
  }
  freeArgs(tmp2, pc);
  pc->numArgsStatus=0;

  return html;
}

static const char *parseForEntities (const char *tmp, struct PC_ * pc){
  char *entity, *text ;
  const char *tmp1, *tmp2;
  int ret=0, count=0;
  while (tmp < pc->end){
    tmp1 = tmp;
    while (tmp < pc->end && *tmp != '&')tmp++;

    text = (char *)calloc(1, (size_t)(tmp-tmp1+1));
    if (text == NULL) {
      return pc->end;
    }

    memcpy(text, tmp1, (size_t)(tmp-tmp1));
    /* the chunk of text before the first entity will
       not be called, if it starts with an entity*/
    if(strlen(text)>0 && (!(isspace((int)*text)))){
      if (pc->textCallBack) {
	ret = pc->textCallBack(text, pc);
      }
      free(text);
      text = NULL;
      tmp1 = pc->end;
    }
    if(*tmp == '&'){
      tmp++;
      tmp2=tmp;
      /* sometimes the ';' is absent, it's a bad hack, just to avoid more trouble */
      while( tmp < pc->end && (*tmp != ';' && count != 9) ){
	tmp++;
	count++;
      }
      entity = (char *)calloc(1, (size_t)(tmp-tmp2+1));
      if (!entity) {
	return pc->end;
      } else {
	memcpy(entity, tmp2, (size_t)(tmp-tmp2));
	if (*tmp == ';' || count == 9){  /* should I add an errortrap here? */
	  ret = pc->entityCallBack(entity, pc);
	  free(entity);
	  entity = NULL;
	  tmp2 = pc->end;
	  count = 0;
	}
      }
    }
    if (tmp < pc->end) tmp++;
  }
  return tmp;
}

static void parse (const char *html, struct PC_ * pc) {
  while (html < pc->end) {
    /* while(isspace(*html)){html++;} there may be leading blanks in some autogenerated files
       add this or not, that is the question ;-)) */

    if (pc->lhtml_script_passthru != 0) {
      const char *text;
      char *tmp;

      text = html;
      if (pc->lhtml_script_passthru == 1 ){
	while(text+7 < pc->end) {
	  if (*text == '<') {
	    if (*(text+2) == 's' || *(text+2) == 'S') {
	      if (*(text+7) == 't' || *(text+7) == 'T') {
		break;
	      }
	    }
	  }
	  if (text < pc->end) text++;
	} 
      }
      if (pc->lhtml_script_passthru == 2 ){
	while (text + 4 < pc->end) {
	  if (*text == '<') {
	    if (*(text+2) == 'p' || *(text+2) == 'P') {
	      if (*(text+4) == 'e' || *(text+4) == 'E') {
		break;
	      }
	    }
	  }
	  if (text < pc->end) text++;
	}
      }
      if (pc->textCallBack != NULL) {
	tmp = (char *) malloc((size_t)(text-html+1));
	if (tmp == NULL) 
	  return;
	strncpy(tmp, html, (size_t)(text-html));
	tmp[text-html] = '\0';  /* strncpy does not zero-terminate! */
	int ret = pc->textCallBack(tmp, pc);
	if (ret != 0) {
	  free(tmp);
	  tmp = NULL;	
	  return;
	}	
	free(tmp);
	tmp = NULL;
      }

      pc->lhtml_script_passthru = 0;
      html = text;
    }

    if (*html == '<'){
      html++;
      if (html < pc->end) {
	switch (*html) { 
	case '!'   :
	  html++;
	  
	  /* I must admit, I like conditional expressions,
	     they are so obviously obfuscated ;-)          */
	  
	  html = (*html == '-') ?
	    ((pc->commentCallBack) ? parseComment(html, pc) : eatUp(html, pc)) :
	    ((pc->declCallBack)    ? parseDecl(html, pc)    : eatUp(html, pc))  ;
	  break;
	case '?'  : 			/* XML/PHP tag */
	  html = (pc->xmlCallBack != NULL || pc->phpCallBack != NULL) ?
	    parseXML(html, pc) :
	    eatUp(html, pc);
	    break;
	case '/'  : 			/* HTML end tag */
	  html = (pc->endCallBack) ?
	    parseEndTag(html, pc) :
	    eatUp(html, pc);
	    break;
	default  : 			/* HTML start tag */
	  html = (pc->XHTMLCallBack != NULL || pc->startCallBack != NULL) ?
	    parseStartTag(html, pc) :
	    eatUp(html, pc);
	    break;
	}
      }
    } else {				 /* All other text */
      /* while(isspace(*html))html++;   it seems to be faster inside the function */
      html = (pc->textCallBack)  ?
	parseText(html, pc):
	eatUpText(html, pc);
    }
  }
  return;
}



/* ******************* now: LE specifics *************** */


/**
 * Add a keyword.
 **/
static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
					      char * keyword,
					      struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = (EXTRACTOR_KeywordList*)malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = strdup(keyword);
  result->keywordType = type;
  return result;
}

/**
 * Called by the parser whenever we see text.
 **/
static int texts (char *comment, struct PC_ * pc) {
  if (pc->nextTextAction) {
    pc->result = addKeyword(pc->nextKeywordType,
			    comment,
			    pc->result);
    pc->nextTextAction = 0;
  }
  return 0;
}

static int hasTag(char * arg,
		  char * val,
		  struct ArgvTable * args,
		  int numargs) {
  int i;
  for (i=0;i<numargs;i++) {
    if ( (NULL != args[i].arg) &&
	 (NULL != args[i].val) &&
	 (0 == strcasecmp(args[i].arg, arg)) &&
	 (0 == strcasecmp(args[i].val, val)) )
      return 1;
  }
  return 0;
}

static char * getTag(char * arg,
		     struct ArgvTable * args,
		     int numargs) {
  int i;
  for (i=0;i<numargs;i++)
    if (0 == strcasecmp(args[i].arg, arg))
      return args[i].val;
  return NULL;
}

static struct {
  char * name;
  EXTRACTOR_KeywordType type;
} tagmap[] = {
   { "author" , EXTRACTOR_AUTHOR},
   { "description" , EXTRACTOR_DESCRIPTION},
   { "language", EXTRACTOR_LANGUAGE},
   { "rights", EXTRACTOR_COPYRIGHT},
   { "publisher", EXTRACTOR_PUBLISHER},
   { "date", EXTRACTOR_DATE},
   { "keywords", EXTRACTOR_KEYWORDS},
   {NULL, EXTRACTOR_UNKNOWN},
};



static int starttag(char *tag,
		    struct ArgvTable *args,
		    int numargs,
		    struct PC_ * pc) {
  int i;

  if (0 == strcasecmp(tag,"title")) {
    pc->nextTextAction = 1;
    pc->nextKeywordType = EXTRACTOR_TITLE;
    return 0;
  }
  if (0 == strcasecmp(tag,"meta")) {
    i = 0;
    while (tagmap[i].name != NULL) {
      if (hasTag("name",tagmap[i].name,args, numargs))
	pc->result = addKeyword(tagmap[i].type,
				getTag("content",
				       args, numargs),
				pc->result);
      i++;
    }
  }
  /* Don't do this, you can't be certain...*/
#if I_AM_CERTAIN
  if (0 == strcasecmp(tag,"html")) {
    pc->result = addKeyword(EXTRACTOR_MIMETYPE,
			    "text/html",
			    pc->result);
    return 0;
  }
#endif
  if ( (tag != NULL) &&
       ( (0 == strcasecmp(tag, "body")) ||
	 (0 == strcasecmp(tag, "/body")) ) )
    return 1;
  return 0;
}

static int endtag (char *tag, struct PC_ * pc) {
  pc->nextTextAction = 0;
  if ( (tag != NULL) &&
       ( (0 == strcasecmp(tag, "head")) ||
	 (0 == strcasecmp(tag, "/head")) ) )
    return 1;
  return 0;
}


/* which mime-types should not be subjected to
   the HTML extractor (no use trying & parsing
   is expensive!) */
static char * blacklist[] = {
  "image/jpeg",
  "image/gif",
  "image/png",
  "image/x-png",
  "image/xcf",
  "image/tiff",
  "application/java",
  "application/pdf",
  "application/postscript",
  "application/elf",
  "application/gnunet-directory",
  "application/x-gzip",
  "application/bz2",
  "application/x-rpm",
  "application/x-rar",
  "application/x-zip",
  "application/x-arj",
  "application/x-compress",
  "application/x-tar",
  "application/x-lha",
  "application/x-gtar",
  "application/x-dpkg",
  "application/ogg",
  "audio/real",
  "audio/x-wav",
  "audio/avi",
  "audio/midi",
  "audio/mpeg",
  "video/real",
  "video/asf",
  "video/quicktime",
  NULL,
};

/* mimetype = text/html */
struct EXTRACTOR_Keywords * 
libextractor_html_extract(const char * filename,
			  const char * data,
			  const size_t size,
			  struct EXTRACTOR_Keywords * prev) {
  ParserContext pc;
  size_t xsize;
  const char * mime;

  if (size == 0)
    return prev;

  mime = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE,
			       prev);
  if (mime != NULL) {
    int j;
    j = 0;
    while (blacklist[j] != NULL) {
      if (0 == strcmp(blacklist[j], mime))
	return prev;
      j++;
    }
  }

  memset(&pc,
	 0,
	 sizeof(ParserContext));
  pc.end = &data[size];
  pc.result = prev;
  pc.textCallBack = &texts;
  pc.startCallBack = &starttag;
  pc.endCallBack = &endtag;
  if (size > 1024 * 32)
    xsize = 1024 * 32;
  else
    xsize = size;
#ifdef strnlen
  if (strnlen(data, xsize) < xsize - 1)
    return prev;
#endif
  parse(data, &pc);
  return pc.result;
}

