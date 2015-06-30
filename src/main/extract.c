/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2005, 2006, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/
/**
 * @file main/extract.c
 * @brief command-line tool to run GNU libextractor
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"
#include "getopt.h"
#include <signal.h>

#define YES 1
#define NO 0


/**
 * Which keyword types should we print?
 */
static int *print;

/**
 * How verbose are we supposed to be?
 */
static int verbose;

/**
 * Run plugins in-process.
 */
static int in_process;

/**
 * Read file contents into memory, then feed them to extractor.
 */
static int from_memory;

#ifndef WINDOWS
/**
 * Install a signal handler to ignore SIGPIPE.
 */
static void
ignore_sigpipe ()
{
  struct sigaction oldsig;
  struct sigaction sig;

  memset (&sig, 0, sizeof (struct sigaction));
  sig.sa_handler = SIG_IGN;
  sigemptyset (&sig.sa_mask);
#ifdef SA_INTERRUPT
  sig.sa_flags = SA_INTERRUPT;  /* SunOS */
#else
  sig.sa_flags = SA_RESTART;
#endif
  if (0 != sigaction (SIGPIPE, &sig, &oldsig))
    FPRINTF (stderr,
             "Failed to install SIGPIPE handler: %s\n", strerror (errno));
}
#endif


/**
 * Information about command-line options.
 */
struct Help 
{
  /**
   * Single-character option name, '\0' for none.
   */ 
  char shortArg;
  
  /**
   * Long name of the option.
   */ 
  const char * longArg;

  /**
   * Name of the mandatory argument, NULL for no argument.
   */
  const char * mandatoryArg;

  /**
   * Help text for the option.
   */
  const char * description;
};


/**
 * Indentation for descriptions.
 */
#define BORDER 29


/**
 * Display help text (--help).
 *
 * @param general binary name
 * @param description program description
 * @param opt program options (NULL-terminated array)
 */
static void 
format_help (const char *general,
	     const char *description,
	     const struct Help *opt) 
{
  size_t slen;
  unsigned int i;
  ssize_t j;
  size_t ml;
  size_t p;
  char scp[80];
  const char *trans;
	
  printf (_("Usage: %s\n%s\n\n"),
	  gettext(general),
	  gettext(description));
  printf (_("Arguments mandatory for long options are also mandatory for short options.\n"));
  slen = 0;
  i = 0;
  while (NULL != opt[i].description) 
    {
      if (0 == opt[i].shortArg)
	printf ("      ");
      else
	printf ("  -%c, ",
		opt[i].shortArg);
      printf ("--%s",
	      opt[i].longArg);
      slen = 8 + strlen(opt[i].longArg);
      if (NULL != opt[i].mandatoryArg) 
	{
	  printf ("=%s",
		  opt[i].mandatoryArg);
	  slen += 1+strlen(opt[i].mandatoryArg);
	}
      if (slen > BORDER) 
	{
	  printf ("\n%*s", BORDER, "");
	  slen = BORDER;
	}
      if (slen < BORDER) 
	{
	  printf ("%*s", (int) (BORDER - slen), "");
	  slen = BORDER;
	}
      trans = gettext(opt[i].description);
      ml = strlen(trans);
      p = 0;
    OUTER:
      while (ml - p > 78 - slen) 
	{
	  for (j=p+78-slen;j>p;j--)
	    {
	      if (isspace( (unsigned char) trans[j]))
		{
		  memcpy(scp,
			 &trans[p],
			 j-p);
		  scp[j-p] = '\0';
		  printf ("%s\n%*s",
			  scp,
			  BORDER + 2,
			  "");
		  p = j+1;
		  slen = BORDER + 2;
		  goto OUTER;
		}
	    }
	  /* could not find space to break line */
	  memcpy (scp,
		  &trans[p],
		  78 - slen);
	  scp[78 - slen] = '\0';
	  printf ("%s\n%*s",
		  scp,
		  BORDER+2,
		  "");	
	  slen = BORDER+2;
	  p = p + 78 - slen;
	}
      /* print rest */
      if (p < ml)
	printf("%s\n",
	       &trans[p]);
      i++;
    }
}


/**
 * Run --help.
 */
static void
print_help ()
{
  static struct Help help[] = 
    {
      { 'b', "bibtex", NULL,
	gettext_noop("print output in bibtex format") },
      { 'g', "grep-friendly", NULL,
	gettext_noop("produce grep-friendly output (all results on one line per file)") },
      { 'h', "help", NULL,
	gettext_noop("print this help") },
      { 'i', "in-process", NULL,
	gettext_noop("run plugins in-process (simplifies debugging)") },
      { 'm', "from-memory", NULL,
	gettext_noop("read data from file into memory and extract from memory") },
      { 'l', "library", "LIBRARY",
	gettext_noop("load an extractor plugin named LIBRARY") },
      { 'L', "list", NULL,
	gettext_noop("list all keyword types") },
      { 'n', "nodefault", NULL,
	gettext_noop("do not use the default set of extractor plugins") },
      { 'p', "print", "TYPE",
	gettext_noop("print only keywords of the given TYPE (use -L to get a list)") },
      { 'v', "version", NULL,
	gettext_noop("print the version number") },
      { 'V', "verbose", NULL,
	gettext_noop("be verbose") },
      { 'x', "exclude", "TYPE",
	gettext_noop("do not print keywords of the given TYPE") },
      { 0, NULL, NULL, NULL },
    };
  format_help (_("extract [OPTIONS] [FILENAME]*"),
	      _("Extract metadata from files."),
	      help);

}

#if HAVE_ICONV
#include "iconv.c"
#endif

/**
 * Print a keyword list to a file.
 *
 * @param cls closure, not used
 * @param plugin_name name of the plugin that produced this value;
 *        special values can be used (i.e. '<zlib>' for zlib being
 *        used in the main libextractor library and yielding
 *        meta data).
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data 
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return 0 to continue extracting, 1 to abort
 */ 
static int
print_selected_keywords (void *cls,
			 const char *plugin_name,
			 enum EXTRACTOR_MetaType type,
			 enum EXTRACTOR_MetaFormat format,
			 const char *data_mime_type,
			 const char *data,
			 size_t data_len)
{ 
  char *keyword;
#if HAVE_ICONV
  iconv_t cd;
#endif
  const char *stype;
  const char *mt;

  if (YES != print[type])
    return 0;
  if (verbose > 3)
    FPRINTF (stdout,
	     _("Found by `%s' plugin:\n"),
	     plugin_name);
  mt = EXTRACTOR_metatype_to_string (type);
  stype = (NULL == mt) ? _("unknown") : gettext(mt);
  switch (format)
    {
    case EXTRACTOR_METAFORMAT_UNKNOWN:
      FPRINTF (stdout,
	       _("%s - (unknown, %u bytes)\n"),
	       stype,
	       (unsigned int) data_len);
      break;
    case EXTRACTOR_METAFORMAT_UTF8:
#if HAVE_ICONV
      cd = iconv_open (nl_langinfo(CODESET), "UTF-8");
      if (((iconv_t) -1) != cd)
	keyword = iconv_helper (cd,
				data,
				data_len);
      else
#endif
	keyword = strdup (data);
      if (NULL != keyword)
	{
	  FPRINTF (stdout,
		   "%s - %s\n",
		   stype,
		   keyword);
	  free (keyword);
	}
#if HAVE_ICONV
      if (((iconv_t) -1) != cd)
	iconv_close (cd);
#endif
      break;
    case EXTRACTOR_METAFORMAT_BINARY:
      FPRINTF (stdout,
	       _("%s - (binary, %u bytes)\n"),
	       stype,
	       (unsigned int) data_len);
      break;
    case EXTRACTOR_METAFORMAT_C_STRING:
      FPRINTF (stdout,
	       "%s - %.*s\n",
	       stype,
	       (int) data_len,
	       data);
      break;
    default:
      break;
    }
  return 0;
}


/**
 * Print a keyword list to a file without new lines.
 *
 * @param cls closure, not used
 * @param plugin_name name of the plugin that produced this value;
 *        special values can be used (i.e. '<zlib>' for zlib being
 *        used in the main libextractor library and yielding
 *        meta data).
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data 
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return 0 to continue extracting, 1 to abort
 */ 
static int
print_selected_keywords_grep_friendly (void *cls,
				       const char *plugin_name,
				       enum EXTRACTOR_MetaType type,
				       enum EXTRACTOR_MetaFormat format,
				       const char *data_mime_type,
				       const char *data,
				       size_t data_len)
{ 
  char *keyword;
#if HAVE_ICONV 
  iconv_t cd;
#endif
  const char *mt;

  if (YES != print[type])
    return 0;
  mt = EXTRACTOR_metatype_to_string (type);
  if (NULL == mt)
    mt = gettext_noop ("unknown");
  switch (format)
    {
    case EXTRACTOR_METAFORMAT_UNKNOWN:      
      break;
    case EXTRACTOR_METAFORMAT_UTF8:
      if (verbose > 1)
	FPRINTF (stdout,
		 "%s: ",
		 gettext(mt));
#if HAVE_ICONV 
      cd = iconv_open (nl_langinfo (CODESET), "UTF-8");
      if (((iconv_t) -1) != cd)
	keyword = iconv_helper (cd,
				data,
				data_len);
      else
#endif
	keyword = strdup (data);
      if (NULL != keyword)
	{
	  FPRINTF (stdout,
		   "`%s' ",
		   keyword);
	  free (keyword);
	}
#if HAVE_ICONV 
      if (((iconv_t) -1) != cd)
	iconv_close (cd);
#endif
      break;
    case EXTRACTOR_METAFORMAT_BINARY:
      break;
    case EXTRACTOR_METAFORMAT_C_STRING:
      if (verbose > 1)
	FPRINTF (stdout,
		 "%s ",
		 gettext(mt));
      FPRINTF (stdout,
	       "`%s'",
	       data);
      break;
    default:
      break;
    }
  return 0;
}


/**
 * Entry in the map we construct for each file.
 */
struct BibTexMap
{
  /**
   * Name in bibTeX
   */
  const char *bibTexName;

  /**
   * Meta type for the value.
   */
  enum EXTRACTOR_MetaType le_type;

  /**
   * The value itself.
   */
  char *value;
};


/**
 * Type of the entry for bibtex.
 */
static char *entry_type;

/**
 * Mapping between bibTeX strings, libextractor
 * meta data types and values for the current document.
 */
static struct BibTexMap btm[] =
  {
    { "title", EXTRACTOR_METATYPE_TITLE, NULL},
    { "year", EXTRACTOR_METATYPE_PUBLICATION_YEAR, NULL },
    { "author", EXTRACTOR_METATYPE_AUTHOR_NAME, NULL },
    { "book", EXTRACTOR_METATYPE_BOOK_TITLE, NULL},
    { "edition", EXTRACTOR_METATYPE_BOOK_EDITION, NULL},
    { "chapter", EXTRACTOR_METATYPE_BOOK_CHAPTER_NUMBER, NULL},
    { "journal", EXTRACTOR_METATYPE_JOURNAL_NAME, NULL},
    { "volume", EXTRACTOR_METATYPE_JOURNAL_VOLUME, NULL},
    { "number", EXTRACTOR_METATYPE_JOURNAL_NUMBER, NULL},
    { "pages", EXTRACTOR_METATYPE_PAGE_COUNT, NULL },
    { "pages", EXTRACTOR_METATYPE_PAGE_RANGE, NULL },
    { "school", EXTRACTOR_METATYPE_AUTHOR_INSTITUTION, NULL},
    { "publisher", EXTRACTOR_METATYPE_PUBLISHER, NULL },
    { "address", EXTRACTOR_METATYPE_PUBLISHER_ADDRESS, NULL },
    { "institution", EXTRACTOR_METATYPE_PUBLISHER_INSTITUTION, NULL },
    { "series", EXTRACTOR_METATYPE_PUBLISHER_SERIES, NULL},
    { "month", EXTRACTOR_METATYPE_PUBLICATION_MONTH, NULL },
    { "url", EXTRACTOR_METATYPE_URL, NULL}, 
    { "note", EXTRACTOR_METATYPE_COMMENT, NULL},
    { "eprint", EXTRACTOR_METATYPE_BIBTEX_EPRINT, NULL },
    { "type", EXTRACTOR_METATYPE_PUBLICATION_TYPE, NULL },
    { NULL, 0, NULL }
  };


/**
 * Clean up the bibtex processor in preparation for the next round.
 */
static void 
cleanup_bibtex ()
{
  unsigned int i;
  
  for (i = 0; NULL != btm[i].bibTexName; i++)
    {
      free (btm[i].value);
      btm[i].value = NULL;
    }
  free (entry_type);
  entry_type = NULL;
}


/**
 * Callback function for printing meta data in bibtex format.
 *
 * @param cls closure, not used
 * @param plugin_name name of the plugin that produced this value;
 *        special values can be used (i.e. '<zlib>' for zlib being
 *        used in the main libextractor library and yielding
 *        meta data).
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data 
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return 0 to continue extracting (always)
 */
static int
print_bibtex (void *cls,
	      const char *plugin_name,
	      enum EXTRACTOR_MetaType type,
	      enum EXTRACTOR_MetaFormat format,
	      const char *data_mime_type,
	      const char *data,
	      size_t data_len)
{
  unsigned int i;

  if (YES != print[type])
    return 0;
  if (EXTRACTOR_METAFORMAT_UTF8 != format)
    return 0;
  if (EXTRACTOR_METATYPE_BIBTEX_ENTRY_TYPE == type)
    {
      entry_type = strdup (data);
      return 0;
    }
  for (i = 0; NULL != btm[i].bibTexName; i++)
    if ( (NULL == btm[i].value) &&
	 (btm[i].le_type == type) )
      btm[i].value = strdup (data);
  return 0;
}


/**
 * Print the computed bibTeX entry.
 *
 * @param fn file for which the entry was created.
 */
static void
finish_bibtex (const char *fn)
{
  unsigned int i;
  ssize_t n;
  const char *et;
  char temp[20];

  if (NULL != entry_type)
    et = entry_type;
  else
    et = "misc";
  if ( (NULL == btm[0].value) ||
       (NULL == btm[1].value) ||
       (NULL == btm[2].value) )          
    FPRINTF (stdout,
	     "@%s %s { ",
	     et,
	     fn);
  else
    {
      snprintf (temp,
		sizeof (temp),
		"%.5s%.5s%.5s",
		btm[2].value,
		btm[1].value,
		btm[0].value);
      for (n=strlen (temp)-1;n>=0;n-- )
	if (! isalnum ( (unsigned char) temp[n]) ) 
	  temp[n] = '_';
	else 
	  temp[n] = tolower ( (unsigned char) temp[n]);
      FPRINTF (stdout,
	       "@%s %s { ",
	       et,
	       temp);
    }
  for (i=0; NULL != btm[i].bibTexName; i++)
    if (NULL != btm[i].value) 
      FPRINTF (stdout,
	       "\t%s = {%s},\n",
	       btm[i].bibTexName,
	       btm[i].value);
  FPRINTF (stdout, "%s", "}\n\n");
}


#ifdef WINDOWS
static int
_wchar_to_str (const wchar_t *wstr, char **retstr, UINT cp)
{
  char *str;
  int len, lenc;
  BOOL lossy = FALSE;
  DWORD error;

  SetLastError (0);
  len = WideCharToMultiByte (cp, 0, wstr, -1, NULL, 0, NULL, (cp == CP_UTF8 || cp == CP_UTF7) ? NULL : &lossy);
  error = GetLastError ();
  if (len <= 0)
    return -1;
  
  str = malloc (sizeof (char) * len);
  
  SetLastError (0);
  lenc = WideCharToMultiByte (cp, 0, wstr, -1, str, len, NULL, (cp == CP_UTF8 || cp == CP_UTF7) ? NULL : &lossy);
  error = GetLastError ();
  if (lenc != len)
  {
    free (str);
    return -3;
  }
  *retstr = str;
  if (lossy)
    return 1;
  return 0;
}
#endif


/**
 * Makes a copy of argv that consists of a single memory chunk that can be
 * freed with a single call to free ();
 */
static char **
_make_continuous_arg_copy (int argc, char *const *argv)
{
  size_t argvsize = 0;
  int i;
  char **new_argv;
  char *p;
  for (i = 0; i < argc; i++)
    argvsize += strlen (argv[i]) + 1 + sizeof (char *);
  new_argv = malloc (argvsize + sizeof (char *));
  if (NULL == new_argv)
    return NULL;
  p = (char *) &new_argv[argc + 1];
  for (i = 0; i < argc; i++)
  {
    new_argv[i] = p;
    strcpy (p, argv[i]);
    p += strlen (argv[i]) + 1;
  }
  new_argv[argc] = NULL;
  return (char **) new_argv;
}


/**
 * Returns utf-8 encoded arguments.
 * Returned argv has u8argv[u8argc] == NULL.
 * Returned argv is a single memory block, and can be freed with a single
 *   free () call.
 *
 * @param argc argc (as given by main())
 * @param argv argv (as given by main())
 * @param u8argc a location to store new argc in (though it's th same as argc)
 * @param u8argv a location to store new argv in
 * @return 0 on success, -1 on failure
 */
static int
_get_utf8_args (int argc, char *const *argv, int *u8argc, char ***u8argv)
{
#ifdef WINDOWS
  wchar_t *wcmd;
  wchar_t **wargv;
  int wargc;
  int i;
  char **split_u8argv;

  wcmd = GetCommandLineW ();
  if (NULL == wcmd)
    return -1;
  wargv = CommandLineToArgvW (wcmd, &wargc);
  if (NULL == wargv)
    return -1;

  split_u8argv = malloc (wargc * sizeof (char *));

  for (i = 0; i < wargc; i++)
  {
    if (_wchar_to_str (wargv[i], &split_u8argv[i], CP_UTF8) != 0)
    {
      int j;
      int e = errno;
      for (j = 0; j < i; j++)
        free (split_u8argv[j]);
      free (split_u8argv);
      LocalFree (wargv);
      errno = e;
      return -1;
    }
  }

  *u8argv = _make_continuous_arg_copy (wargc, split_u8argv);
  if (NULL == *u8argv)
    {
      free (split_u8argv);
      return -1;
    }
  *u8argc = wargc;

  for (i = 0; i < wargc; i++)
    free (split_u8argv[i]);
  free (split_u8argv);
#else
  *u8argv = _make_continuous_arg_copy (argc, argv);
  if (NULL == *u8argv)
    return -1;
  *u8argc = argc;
#endif
  return 0;
}


/**
 * Main function for the 'extract' tool.  Invoke with a list of
 * filenames to extract keywords from.
 *
 * @param argc number of arguments in argv
 * @param argv command line options and filename to run on
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  unsigned int i;
  struct EXTRACTOR_PluginList *plugins;
  int option_index;
  int c;
  char *libraries = NULL;
  int nodefault = NO;
  int defaultAll = YES;
  int bibtex = NO;
  int grepfriendly = NO;
  int ret = 0;
  EXTRACTOR_MetaDataProcessor processor = NULL;
  char **utf8_argv;
  int utf8_argc;

#if ENABLE_NLS
  setlocale(LC_ALL, "");
  textdomain(PACKAGE);
#endif
#ifndef WINDOWS
  ignore_sigpipe ();
#endif
  if (NULL == (print = malloc (sizeof (int) * EXTRACTOR_metatype_get_max ())))
    {
      FPRINTF (stderr, 
	       "malloc failed: %s\n",
	       strerror (errno));
      return 1;
    }
  for (i = 0; i < EXTRACTOR_metatype_get_max (); i++)
    print[i] = YES;		/* default: print everything */

  if (0 != _get_utf8_args (argc, argv, &utf8_argc, &utf8_argv))
  {
    FPRINTF (stderr, "Failed to get arguments: %s\n", strerror (errno));
    return 1;
  }

  while (1)
    {
      static struct option long_options[] = {
	{"bibtex", 0, 0, 'b'},
	{"grep-friendly", 0, 0, 'g'},
	{"help", 0, 0, 'h'},
	{"in-process", 0, 0, 'i'},
        {"from-memory", 0, 0, 'm'},
	{"list", 0, 0, 'L'},
	{"library", 1, 0, 'l'},
	{"nodefault", 0, 0, 'n'},
	{"print", 1, 0, 'p'},
	{"verbose", 0, 0, 'V'},
	{"version", 0, 0, 'v'},
	{"exclude", 1, 0, 'x'},
	{0, 0, 0, 0}
      };
      option_index = 0;
      c = getopt_long (utf8_argc,
		       utf8_argv, 
		       "abghiml:Lnp:vVx:",
		       long_options,
		       &option_index);

      if (c == -1)
	break;			/* No more flags to process */
      switch (c)
	{
	case 'b':
	  bibtex = YES;
	  if (NULL != processor)
	    {
	      FPRINTF (stderr,
		       "%s",
		       _("Illegal combination of options, cannot combine multiple styles of printing.\n"));
	      free (utf8_argv);
	      return 0;
	    }
	  processor = &print_bibtex;
	  break;
	case 'g':
	  grepfriendly = YES;
	  if (NULL != processor)
	    {
	      FPRINTF (stderr,
		       "%s",
		       _("Illegal combination of options, cannot combine multiple styles of printing.\n"));
	      free (utf8_argv);
	      return 0;
	    }
	  processor = &print_selected_keywords_grep_friendly;
	  break;
	case 'h':
	  print_help ();
          free (utf8_argv);
	  return 0;
	case 'i':
	  in_process = YES;
	  break;
        case 'm':
          from_memory = YES;
          break;
	case 'l':
	  libraries = optarg;
	  break;
	case 'L':
	  i = 0;
	  while (NULL != EXTRACTOR_metatype_to_string (i))
	    printf ("%s\n",
		    gettext(EXTRACTOR_metatype_to_string (i++)));
	  free (utf8_argv);
	  return 0;
	case 'n':
	  nodefault = YES;
	  break;
	case 'p':
	  if (NULL == optarg) 
	    {
	      FPRINTF(stderr,
		      _("You must specify an argument for the `%s' option (option ignored).\n"),
		      "-p");
	      break;
	    }
	  if (YES == defaultAll)
	    {
	      defaultAll = NO;
	      i = 0;
	      while (NULL != EXTRACTOR_metatype_to_string (i))
		print[i++] = NO;
	    }
	  i = 0;
	  while (NULL != EXTRACTOR_metatype_to_string (i))
	    {
	      if ( (0 == strcmp (optarg, 
				 EXTRACTOR_metatype_to_string (i))) ||
		   (0 == strcmp (optarg, 
				 gettext(EXTRACTOR_metatype_to_string (i)))) )
		
		{
		  print[i] = YES;
		  break;
		}
	      i++;
	    }
	  if (NULL == EXTRACTOR_metatype_to_string (i))
	    {
	      FPRINTF(stderr,
		      "Unknown keyword type `%s', use option `%s' to get a list.\n",
		      optarg,
		       "-L");
	      free (utf8_argv);
	      return -1;
	    }
	  break;
       	case 'v':
	  printf ("extract v%s\n", PACKAGE_VERSION);
	  free (utf8_argv);
	  return 0;
	case 'V':
	  verbose++;
	  break;
	case 'x':
	  i = 0;
	  while (NULL != EXTRACTOR_metatype_to_string (i))
	    {
	      if ( (0 == strcmp (optarg, 
				 EXTRACTOR_metatype_to_string (i))) ||
		   (0 == strcmp (optarg, 
				 gettext(EXTRACTOR_metatype_to_string (i)))) )
		{
		  print[i] = NO;
		  break;
		}
	      i++;
	    }
	  if (NULL == EXTRACTOR_metatype_to_string (i))
	    {
	      FPRINTF (stderr,
		       "Unknown keyword type `%s', use option `%s' to get a list.\n",
		       optarg,
		       "-L");
	      free (utf8_argv);
	      return -1;
	    }
	  break;
	default:
	  FPRINTF (stderr,
		   "%s",
		   _("Use --help to get a list of options.\n"));
	  free (utf8_argv);
	  return -1;
	}			/* end of parsing commandline */
    }				/* while (1) */
  if (optind < 0)
    {
      FPRINTF (stderr,
	       "%s", "Unknown error parsing options\n");
      free (print);
      free (utf8_argv);
      return -1;
    }
  if (utf8_argc - optind < 1)
    {
      FPRINTF (stderr,
	       "%s", "Invoke with list of filenames to extract keywords form!\n");
      free (print);
      free (utf8_argv);
      return -1;
    }

  /* build list of libraries */
  if (NO == nodefault)
    plugins = EXTRACTOR_plugin_add_defaults (in_process
					     ? EXTRACTOR_OPTION_IN_PROCESS
					     : EXTRACTOR_OPTION_DEFAULT_POLICY);
  else
    plugins = NULL;
  if (NULL != libraries)
    plugins = EXTRACTOR_plugin_add_config (plugins, 
					   libraries,
					   in_process
					   ? EXTRACTOR_OPTION_IN_PROCESS
					   : EXTRACTOR_OPTION_DEFAULT_POLICY);
  if (NULL == processor)
    processor = &print_selected_keywords;

  /* extract keywords */
  if (YES == bibtex)
    FPRINTF(stdout,
	    "%s", _("% BiBTeX file\n"));
  for (i = optind; i < utf8_argc; i++) 
    {
      errno = 0;
      if (YES == grepfriendly)
	FPRINTF (stdout, "%s ", utf8_argv[i]);
      else if (NO == bibtex)
	FPRINTF (stdout,
		 _("Keywords for file %s:\n"),
		 utf8_argv[i]);
      else
	cleanup_bibtex ();
      if (NO == from_memory)
	EXTRACTOR_extract (plugins,
			   utf8_argv[i],
			   NULL, 0,
			   processor,
			   NULL);
      else
	{
	  struct stat sb;
	  unsigned char *data = NULL;
	  int f = OPEN (utf8_argv[i], O_RDONLY
#if WINDOWS
			| O_BINARY
#endif
			);
	  if ( (-1 != f) &&
	       (0 == FSTAT (f, &sb)) &&
	       (NULL != (data = malloc ((size_t) sb.st_size))) &&
	       (sb.st_size == READ (f, data, (size_t) sb.st_size) ) )
	    {
	      EXTRACTOR_extract (plugins,
				 NULL,
				 data, sb.st_size,
				 processor,
				 NULL);
	    }
	  else
	    {
	      if (verbose > 0) 
		FPRINTF(stderr,
			"%s: %s: %s\n",
			utf8_argv[0], utf8_argv[i], strerror(errno));
	      ret = 1;
	    }
	  if (NULL != data)
	    free (data);
	  if (-1 != f)
	    (void) CLOSE (f);
	}
      if (YES == grepfriendly)
	FPRINTF (stdout, "%s", "\n");
      continue;
    }
  if (YES == grepfriendly)
    FPRINTF (stdout, "%s", "\n");
  if (bibtex)
    finish_bibtex (utf8_argv[i]);
  if (verbose > 0)
    FPRINTF (stdout, "%s", "\n");
  free (print);
  free (utf8_argv);
  EXTRACTOR_plugin_remove_all (plugins);
  plugins = NULL;
  cleanup_bibtex (); /* actually free's stuff */
  return ret;
}

/* end of extract.c */
