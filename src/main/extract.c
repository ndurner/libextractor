/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006, 2009 Vidyut Samanta and Christian Grothoff

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
#include "getopt.h"

#include <signal.h>

#define YES 1
#define NO 0


/**
 * Which keyword types should we print?
 */
static int * print;

/**
 * How verbose are we supposed to be?
 */
static int verbose;

/**
 * Run plugins in-process.
 */
static int in_process;


static void
catcher (int sig)
{
}

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
  sig.sa_handler = &catcher;
  sigemptyset (&sig.sa_mask);
#ifdef SA_INTERRUPT
  sig.sa_flags = SA_INTERRUPT;  /* SunOS */
#else
  sig.sa_flags = SA_RESTART;
#endif
  if (0 != sigaction (SIGPIPE, &sig, &oldsig))
    fprintf (stderr,
             "Failed to install SIGPIPE handler: %s\n", strerror (errno));
}
#endif



typedef struct {
  char shortArg;
  char * longArg;
  char * mandatoryArg;
  char * description;
} Help;

#define BORDER 29

static void formatHelp(const char * general,
		       const char * description,
		       const Help * opt) {
  int slen;
  int i;
  int j;
  int ml;
  int p;
  char scp[80];
  const char * trans;
	
  printf(_("Usage: %s\n%s\n\n"),
	 gettext(general),
	 gettext(description));
  printf(_("Arguments mandatory for long options are also mandatory for short options.\n"));
  slen = 0;
  i = 0;
  while (opt[i].description != NULL) {
    if (opt[i].shortArg == 0)
      printf("      ");
    else
      printf("  -%c, ",
	     opt[i].shortArg);
    printf("--%s",
	   opt[i].longArg);
    slen = 8 + strlen(opt[i].longArg);
    if (opt[i].mandatoryArg != NULL) {
      printf("=%s",
	     opt[i].mandatoryArg);
      slen += 1+strlen(opt[i].mandatoryArg);
    }
    if (slen > BORDER) {
      printf("\n%*s", BORDER, "");
      slen = BORDER;
    }
    if (slen < BORDER) {
      printf("%*s", BORDER-slen, "");
      slen = BORDER;
    }
    trans = gettext(opt[i].description);
    ml = strlen(trans);
    p = 0;
  OUTER:
    while (ml - p > 78 - slen) {
      for (j=p+78-slen;j>p;j--) {
	if (isspace( (unsigned char) trans[j])) {
	  memcpy(scp,
		 &trans[p],
		 j-p);
	  scp[j-p] = '\0';
	  printf("%s\n%*s",
		 scp,
		 BORDER+2,
		 "");
	  p = j+1;
	  slen = BORDER+2;
	  goto OUTER;
	}
      }
      /* could not find space to break line */
      memcpy(scp,
	     &trans[p],
	     78 - slen);
      scp[78 - slen] = '\0';
      printf("%s\n%*s",
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

static void
printHelp ()
{
  static Help help[] = {
    { 'b', "bibtex", NULL,
      gettext_noop("print output in bibtex format") },
    { 'g', "grep-friendly", NULL,
      gettext_noop("produce grep-friendly output (all results on one line per file)") },
    { 'h', "help", NULL,
      gettext_noop("print this help") },
    { 'i', "in-process", NULL,
      gettext_noop("run plugins in-process (simplifies debugging)") },
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
  formatHelp(_("extract [OPTIONS] [FILENAME]*"),
	     _("Extract metadata from files."),
	     help);

}

#include "iconv.c"

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
  char * keyword;
  iconv_t cd;
  const char *stype;
  const char *mt;

  if (print[type] != YES)
    return 0;
  if (verbose > 3)
    fprintf (stdout,
	     _("Found by `%s' plugin:\n"),
	     plugin_name);
  mt = EXTRACTOR_metatype_to_string(type);
  stype = (mt == NULL) ? _("unknown") : gettext(mt);
  switch (format)
    {
    case EXTRACTOR_METAFORMAT_UNKNOWN:
      fprintf (stdout,
	       _("%s - (unknown, %u bytes)\n"),
	       stype,
	       (unsigned int) data_len);
      break;
    case EXTRACTOR_METAFORMAT_UTF8:
      cd = iconv_open(nl_langinfo(CODESET), "UTF-8");
      if (cd != (iconv_t) -1)
	keyword = iconv_helper(cd,
			       data);
      else
	keyword = strdup(data);
      if (keyword != NULL)
	{
	  fprintf (stdout,
		   "%s - %s\n",
		   stype,
		   keyword);
	  free(keyword);
	}
      if (cd != (iconv_t) -1)
	iconv_close(cd);
      break;
    case EXTRACTOR_METAFORMAT_BINARY:
      fprintf (stdout,
	       _("%s - (binary, %u bytes)\n"),
	       stype,
	       (unsigned int) data_len);
      break;
    case EXTRACTOR_METAFORMAT_C_STRING:
      fprintf (stdout,
	       "%s - %s\n",
	       stype,
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
  char * keyword;
  iconv_t cd;
  const char *mt;

  if (print[type] != YES)
    return 0;
  mt = EXTRACTOR_metatype_to_string(type);
  if (mt == NULL)
    mt = gettext_noop ("unknown");
  switch (format)
    {
    case EXTRACTOR_METAFORMAT_UNKNOWN:      
      break;
    case EXTRACTOR_METAFORMAT_UTF8:
      if (verbose > 1)
	fprintf (stdout,
		 "%s: ",
		 gettext(mt));
      cd = iconv_open(nl_langinfo(CODESET), "UTF-8");
      if (cd != (iconv_t) -1)
	keyword = iconv_helper(cd,
			       data);
      else
	keyword = strdup(data);
      if (keyword != NULL)
	{
	  fprintf (stdout,
		   "`%s' ",
		   keyword);
	  free(keyword);
	}
      if (cd != (iconv_t) -1)
	iconv_close(cd);
      break;
    case EXTRACTOR_METAFORMAT_BINARY:
      break;
    case EXTRACTOR_METAFORMAT_C_STRING:
      if (verbose > 1)
	fprintf (stdout,
		 "%s ",
		 gettext(mt));
      fprintf (stdout,
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
  const char *bibTexName;
  enum EXTRACTOR_MetaType le_type;
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
start_bibtex ()
{
  int i;
  
  i = 0;
  while (btm[i].bibTexName != NULL)
    {
      free (btm[i].value);
      btm[i].value = NULL;
      i++;
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
  int i;

  if (print[type] != YES)
    return 0;
  if (format != EXTRACTOR_METAFORMAT_UTF8)
    return 0;
  if (type == EXTRACTOR_METATYPE_BIBTEX_ENTRY_TYPE)
    {
      entry_type = strdup (data);
      return 0;
    }
  i = 0;
  while (btm[i].bibTexName != NULL)
    {
      if ( (btm[i].value == NULL) &&
	   (btm[i].le_type == type) )
	btm[i].value = strdup (data);
      i++;
    }  
  return 0;
}


static void
finish_bibtex (const char *fn)
{
  int i;
  const char *et;
  char temp[20];

  if (entry_type != NULL)
    et = entry_type;
  else
    et = "misc";
  if ( (btm[0].value == NULL) ||
       (btm[1].value == NULL) ||
       (btm[2].value == NULL) )          
    fprintf (stdout,
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
      
      for (i=strlen(temp)-1;i>=0;i-- )
	if (! isalnum( (unsigned char) temp[i]) ) 
	  temp[i] = '_';
	else 
	  temp[i] = tolower( (unsigned char) temp[i]);
      fprintf (stdout,
	       "@%s %s { ",
	       et,
	       temp);
    }

	     
  i = 0;
  while (btm[i].bibTexName != NULL)
    {
      if (btm[i].value != NULL) 
	fprintf (stdout,
		 "\t%s = {%s},\n",
		 btm[i].bibTexName,
		 btm[i].value);
      i++;
    }  
  fprintf(stdout, "}\n\n");
}


/**
 * Main function for the 'extract' tool.  Invoke with a list of
 * filenames to extract keywords from.
 */
int
main (int argc, char *argv[])
{
  int i;
  struct EXTRACTOR_PluginList *plugins;
  int option_index;
  int c;
  char * libraries = NULL;
  int nodefault = NO;
  int defaultAll = YES;
  int bibtex = NO;
  int grepfriendly = NO;
  int ret = 0;
  EXTRACTOR_MetaDataProcessor processor = NULL;

#if ENABLE_NLS
  setlocale(LC_ALL, "");
  textdomain(PACKAGE);
#endif
#ifndef WINDOWS
  ignore_sigpipe ();
#endif
  print = malloc (sizeof (int) * EXTRACTOR_metatype_get_max ());
  if (print == NULL)
    {
      fprintf (stderr, 
	       "malloc failed: %s\n",
	       strerror (errno));
      return 1;
    }
  for (i = 0; i < EXTRACTOR_metatype_get_max (); i++)
    print[i] = YES;		/* default: print everything */

  while (1)
    {
      static struct option long_options[] = {
	{"bibtex", 0, 0, 'b'},
	{"grep-friendly", 0, 0, 'g'},
	{"help", 0, 0, 'h'},
	{"in-process", 0, 0, 'i'},
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
      c = getopt_long (argc,
		       argv, 
		       "abghil:Lnp:vVx:",
		       long_options,
		       &option_index);

      if (c == -1)
	break;			/* No more flags to process */
      switch (c)
	{
	case 'b':
	  bibtex = YES;
	  if (processor != NULL)
	    {
	      fprintf (stderr,
		       _("Illegal combination of options, cannot combine multiple styles of printing.\n"));
	      return 0;
	    }
	  processor = &print_bibtex;
	  break;
	case 'g':
	  grepfriendly = YES;
	  if (processor != NULL)
	    {
	      fprintf (stderr,
		       _("Illegal combination of options, cannot combine multiple styles of printing.\n"));
	      return 0;
	    }
	  processor = &print_selected_keywords_grep_friendly;
	  break;
	case 'h':
	  printHelp();
	  return 0;
	case 'i':
	  in_process = 1;
	  break;
	case 'l':
	  libraries = optarg;
	  break;
	case 'L':
	  i = 0;
	  while (NULL != EXTRACTOR_metatype_to_string (i))
	    printf ("%s\n",
		    gettext(EXTRACTOR_metatype_to_string (i++)));
	  return 0;
	case 'n':
	  nodefault = YES;
	  break;
	case 'p':
	  if (optarg == NULL) 
	    {
	      fprintf(stderr,
		      _("You must specify an argument for the `%s' option (option ignored).\n"),
		      "-p");
	      break;
	    }
	  if (defaultAll == YES)
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
	      fprintf(stderr,
		      "Unknown keyword type `%s', use option `%s' to get a list.\n",
		      optarg,
		       "-L");
	      return -1;
	    }
	  break;
       	case 'v':
	  printf ("extract v%s\n", PACKAGE_VERSION);
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
	      fprintf (stderr,
		       "Unknown keyword type `%s', use option `%s' to get a list.\n",
		       optarg,
		       "-L");
	      return -1;
	    }
	  break;
	default:
	  fprintf (stderr,
		   _("Use --help to get a list of options.\n"));
	  return -1;
	}			/* end of parsing commandline */
    }				/* while (1) */
  if (optind < 0)
    {
      fprintf (stderr,
	       "Unknown error parsing options\n");
      free (print);
      return -1;
    }
  if (argc - optind < 1)
    {
      fprintf (stderr,
	       "Invoke with list of filenames to extract keywords form!\n");
      free (print);
      return -1;
    }

  /* build list of libraries */
  if (nodefault == NO)
    plugins = EXTRACTOR_plugin_add_defaults (in_process
					     ? EXTRACTOR_OPTION_IN_PROCESS
					     : EXTRACTOR_OPTION_DEFAULT_POLICY);
  else
    plugins = NULL;
  if (libraries != NULL)
    plugins = EXTRACTOR_plugin_add_config (plugins, 
					   libraries,
					   in_process
					   ? EXTRACTOR_OPTION_IN_PROCESS
					   : EXTRACTOR_OPTION_DEFAULT_POLICY);
  if (processor == NULL)
    processor = &print_selected_keywords;

  /* extract keywords */
  if (bibtex == YES)
    fprintf(stdout,
	    _("%% BiBTeX file\n"));
  for (i = optind; i < argc; i++) {
    errno = 0;
    if (grepfriendly == YES)
      fprintf (stdout, "%s ", argv[i]);
    else if (bibtex == NO)
      fprintf (stdout,
	       _("Keywords for file %s:\n"),
	       argv[i]);
    else
      start_bibtex ();
    EXTRACTOR_extract (plugins,
		       argv[i],
		       NULL, 0,
		       processor,
		       NULL);    
    if (0 != errno) {
      if (verbose > 0) {
	fprintf(stderr,
		"%s: %s: %s\n",
		argv[0], argv[i], strerror(errno));
      }
      ret = 1;
      if (grepfriendly == YES)
	fprintf (stdout, "\n");
      continue;
    }
    if (grepfriendly == YES)
      fprintf (stdout, "\n");
    if (bibtex)
      finish_bibtex (argv[i]);
    if (verbose > 0)
      printf ("\n");
  }
  free (print);
  EXTRACTOR_plugin_remove_all (plugins);
  start_bibtex (); /* actually free's stuff */
  return ret;
}

/* end of extract.c */
