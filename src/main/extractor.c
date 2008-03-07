/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005, 2006 Vidyut Samanta and Christian Grothoff

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

#if HAVE_LTDL_H
#include <ltdl.h>
#else
#include <../../libltdl/ltdl.h>
#endif

#if HAVE_LIBBZ2
#include <bzlib.h>
#endif

#if HAVE_ZLIB
#include <zlib.h>
#endif

#define DEBUG 0

/**
 * The sources of keywords as strings.
 */
static const char *keywordTypes[] = {
  gettext_noop("unknown"), /* 0 */
  gettext_noop("filename"),
  gettext_noop("mimetype"),
  gettext_noop("title"),
  gettext_noop("author"),
  gettext_noop("artist"), /* 5 */
  gettext_noop("description"),
  gettext_noop("comment"),
  gettext_noop("date"),
  gettext_noop("publisher"),
  gettext_noop("language"), /* 10 */
  gettext_noop("album"),
  gettext_noop("genre"),
  gettext_noop("location"),
  gettext_noop("version"),
  gettext_noop("organization"), /* 15 */
  gettext_noop("copyright"),
  gettext_noop("subject"),
  gettext_noop("keywords"),
  gettext_noop("contributor"),
  gettext_noop("resource-type"), /* 20 */
  gettext_noop("format"),
  gettext_noop("resource-identifier"),
  gettext_noop("source"),
  gettext_noop("relation"),
  gettext_noop("coverage"), /* 25 */
  gettext_noop("software"),
  gettext_noop("disclaimer"),
  gettext_noop("warning"),
  gettext_noop("translated"),
  gettext_noop("creation date"), /* 30 */
  gettext_noop("modification date"),
  gettext_noop("creator"),
  gettext_noop("producer"),
  gettext_noop("page count"),
  gettext_noop("page orientation"), /* 35 */
  gettext_noop("paper size"),
  gettext_noop("used fonts"),
  gettext_noop("page order"),
  gettext_noop("created for"),
  gettext_noop("magnification"), /* 40 */
  gettext_noop("release"),
  gettext_noop("group"),
  gettext_noop("size"),
  gettext_noop("summary"),
  gettext_noop("packager"), /* 45 */
  gettext_noop("vendor"),
  gettext_noop("license"),
  gettext_noop("distribution"),
  gettext_noop("build-host"),
  gettext_noop("operating system"), /* 50 */
  gettext_noop("dependency"),
  gettext_noop("MD4"),
  gettext_noop("MD5"),
  gettext_noop("SHA-0"),
  gettext_noop("SHA-1"), /* 55 */
  gettext_noop("RipeMD160"),
  gettext_noop("resolution"),
  gettext_noop("category"),
  gettext_noop("book title"),
  gettext_noop("priority"), /* 60 */
  gettext_noop("conflicts"),
  gettext_noop("replaces"),
  gettext_noop("provides"),
  gettext_noop("conductor"),
  gettext_noop("interpreter"), /* 65 */
  gettext_noop("owner"),
  gettext_noop("lyrics"),
  gettext_noop("media type"),
  gettext_noop("contact"),
  gettext_noop("binary thumbnail data"), /* 70 */
  gettext_noop("publication date"),
  gettext_noop("camera make"),
  gettext_noop("camera model"),
  gettext_noop("exposure"),
  gettext_noop("aperture"), /* 75 */
  gettext_noop("exposure bias"),
  gettext_noop("flash"),
  gettext_noop("flash bias"),
  gettext_noop("focal length"),
  gettext_noop("focal length (35mm equivalent)"), /* 80 */
  gettext_noop("iso speed"),
  gettext_noop("exposure mode"),
  gettext_noop("metering mode"),
  gettext_noop("macro mode"),
  gettext_noop("image quality"), /* 85 */
  gettext_noop("white balance"),
  gettext_noop("orientation"),
  gettext_noop("template"),
  gettext_noop("split"),
  gettext_noop("product version"), /* 90 */
  gettext_noop("last saved by"),
  gettext_noop("last printed"),
  gettext_noop("word count"),
  gettext_noop("character count"),
  gettext_noop("total editing time"), /* 95 */
  gettext_noop("thumbnails"),
  gettext_noop("security"),
  gettext_noop("created by software"),
  gettext_noop("modified by software"),
  gettext_noop("revision history"), /* 100 */
  gettext_noop("lower case conversion"),
  gettext_noop("company"),
  gettext_noop("generator"),
  gettext_noop("character set"),
  gettext_noop("line count"), /* 105 */
  gettext_noop("paragraph count"),
  gettext_noop("editing cycles"),
  gettext_noop("scale"),
  gettext_noop("manager"),
  gettext_noop(/* movie director */"director"), /* 110 */
  gettext_noop("duration"),
  gettext_noop("information"),
  gettext_noop("full name"),
  gettext_noop("chapter"),
  gettext_noop("year"), /* 115 */
  gettext_noop("link"),
  gettext_noop("music CD identifier"),
  gettext_noop("play counter"),
  gettext_noop("popularity meter"),
  gettext_noop("content type"), /* 120 */
  gettext_noop("encoded by"),
  gettext_noop("time"),
  gettext_noop("musician credits list"),
  gettext_noop("mood"),
  gettext_noop("format version"), /* 125 */
  gettext_noop("television system"),
  gettext_noop("song count"),
  gettext_noop("starting song"),
  gettext_noop("hardware dependency"),
  gettext_noop("ripper"), /* 130 */
  gettext_noop("filesize"),
  gettext_noop("track number"),
  gettext_noop("international standard recording code"),
  gettext_noop("disc number"), /* 134 */
  NULL,
};

/* the number of keyword types (for bounds-checking) */
#define HIGHEST_TYPE_NUMBER 135

#ifdef HAVE_LIBOGG
#if HAVE_VORBIS
#define WITH_OGG 1
#endif
#endif

#if HAVE_VORBISFILE
#define WITH_OGG 1
#endif

#if HAVE_EXIV2
#define EXSO "libextractor_exiv2:"
#else
#define EXSO ""
#endif

#if WITH_OGG
#define OGGSO "libextractor_ogg:"
#else
#define OGGSO ""
#endif

#if HAVE_FLAC
#define FLACSO "libextractor_flac:"
#else
#define FLACSO ""
#endif

#if HAVE_ZLIB
#define QTSO "libextractor_qt:"
#else
#define QTSO ""
#endif

#if HAVE_GSF
#define OLESO "libextractor_ole2:"
#else
#define OLESO ""
#endif

#if HAVE_MPEG2
#define MPEGSO "libextractor_mpeg:"
#else
#define MPEGSO ""
#endif 

/* ATTN: order matters (for performance!) since
   mime-types can be used to avoid parsing once
   the type has been established! */
#define DEFSO \
"libextractor_html:\
libextractor_man:\
libextractor_ps:\
libextractor_pdf:\
libextractor_mp3:\
libextractor_id3v2:\
libextractor_id3v23:\
libextractor_id3v24:\
libextractor_mime:\
libextractor_tar:\
libextractor_dvi:\
libextractor_deb:\
libextractor_png:\
libextractor_gif:\
libextractor_wav:\
libextractor_flv:\
libextractor_real:\
libextractor_jpeg:\
libextractor_tiff:\
libextractor_zip:\
libextractor_rpm:\
libextractor_riff:\
libextractor_applefile:\
libextractor_elf:\
libextractor_oo:\
libextractor_asf:\
libextractor_sid:\
libextractor_nsfe:\
libextractor_nsf"

#define DEFAULT_LIBRARIES MPEGSO EXSO OLESO OGGSO FLACSO QTSO DEFSO

const char * EXTRACTOR_getDefaultLibraries() {
  return DEFAULT_LIBRARIES;
}

/* determine installation path */

static char * cut_bin(char * in) {
  size_t p;

  if (in == NULL)
    return NULL;
  p = strlen(in);
  if (p > 4) {
    if ( (in[p-1] == '/') ||
	 (in[p-1] == '\\') )
      in[--p] = '\0';
    if (0 == strcmp(&in[p-3],
		    "bin")) {
      in[p-3] = '\0';
      p -= 3;
    }
  }
  return in;
}

static char * cut_lib(char * in) {
  size_t p;

  if (in == NULL)
    return NULL;
  p = strlen(in);
  if (p > 4) {
    if ( (in[p-1] == '/') ||
	 (in[p-1] == '\\') )
      in[--p] = '\0';
    if (0 == strcmp(&in[p-3],
		    "lib")) {
      in[p-3] = '\0';
      p -= 3;
    }
  }
  return in;
}


#if LINUX
/**
 * Try to determine path by reading /proc/PID/exe or
 * /proc/PID/maps.
 *
 * Note that this may fail if LE is installed in one directory
 * and the binary linking against it sits elsewhere.
 */
static char *
get_path_from_proc_exe() {
  char fn[64];
  char line[1024];
  char dir[1024];
  char * lnk;
  size_t size;
  FILE * f;

  snprintf(fn,
	   64,
	   "/proc/%u/maps",
	   getpid());
  f = fopen(fn, "r");
  if (f != NULL) {
    while (NULL != fgets(line, 1024, f)) {
      if ( (1 == sscanf(line,
			"%*x-%*x %*c%*c%*c%*c %*x %*2u:%*2u %*u%*[ ]%s",
			dir)) &&
	   (NULL != strstr(dir,
			   "libextractor")) ) {
	strstr(dir, "libextractor")[0] = '\0';
	fclose(f);
	return strdup(dir);
      }
    }
    fclose(f);
  }
  snprintf(fn,
	   64,
	   "/proc/%u/exe",
	   getpid());
  lnk = malloc(1029); /* 1024 + 5 for "lib/" catenation */
  size = readlink(fn, lnk, 1023);
  if ( (size == 0) || (size >= 1024) ) {
    free(lnk);
    return NULL;
  }
  lnk[size] = '\0';
  while ( (lnk[size] != '/') &&
	  (size > 0) )
    size--;
  if ( (size < 4) ||
       (lnk[size-4] != '/') ) {
    /* not installed in "/bin/" -- binary path probably useless */
    free(lnk);
    return NULL;
  }
  lnk[size] = '\0';
  lnk = cut_bin(lnk);
  lnk = realloc(lnk, strlen(lnk) + 5);
  strcat(lnk, "lib/"); /* guess "lib/" as the library dir */
  return lnk;
}
#endif

#if WINDOWS
/**
 * Try to determine path with win32-specific function
 */
static char * get_path_from_module_filename() {
  char * path;
  char * idx;

  path = malloc(4103); /* 4096+nil+6 for "/lib/" catenation */
  GetModuleFileName(NULL, path, 4096);
  idx = path + strlen(path);
  while ( (idx > path) &&
	  (*idx != '\\') &&
	  (*idx != '/') )
    idx--;
  *idx = '\0';
  path = cut_bin(path);
  path = realloc(path, strlen(path) + 6);
  strcat(path, "/lib/"); /* guess "lib/" as the library dir */
  return path;
}
#endif

#if DARWIN
static char * get_path_from_dyld_image() {
  const char * path;
  char * p, * s;
  int i;
  int c;

  p = NULL;
  c = _dyld_image_count();
  for (i = 0; i < c; i++) {
    if (_dyld_get_image_header(i) == &_mh_dylib_header) {
      path = _dyld_get_image_name(i);
      if (path != NULL && strlen(path) > 0) {
        p = strdup(path);
        s = p + strlen(p);
        while ( (s > p) && (*s != '/') )
          s--;
        s++;
        *s = '\0';
      }
      break;
    }
  }
  return p;
}
#endif

/**
 * This may also fail -- for example, if extract
 * is not also installed.
 */
static char *
get_path_from_PATH() {
  struct stat sbuf;
  char * path;
  char * pos;
  char * end;
  char * buf;
  const char * p;
  size_t size;

  p = getenv("PATH");
  if (p == NULL)
    return NULL;
  path = strdup(p); /* because we write on it */
  buf = malloc(strlen(path) + 20);
  size = strlen(path);
  pos = path;

  while (NULL != (end = strchr(pos, ':'))) {
    *end = '\0';
    sprintf(buf, "%s/%s", pos, "extract");
    if (0 == stat(buf, &sbuf)) {
      pos = strdup(pos);
      free(buf);
      free(path);
      pos = cut_bin(pos);
      pos = realloc(pos, strlen(pos) + 5);
      strcat(pos, "lib/");
      return pos;
    }
    pos = end + 1;
  }
  sprintf(buf, "%s/%s", pos, "extract");
  if (0 == stat(buf, &sbuf)) {
    pos = strdup(pos);
    free(buf);
    free(path);
    pos = cut_bin(pos);
    pos = realloc(pos, strlen(pos) + 5);
    strcat(pos, "lib/");
    return pos;
  }
  free(buf);
  free(path);
  return NULL;
}

static char *
get_path_from_ENV_PREFIX() {
  const char * p;

  p = getenv("LIBEXTRACTOR_PREFIX");
  if (p != NULL) {
    char * s = malloc(strlen(p) + 6);
    if (s != NULL) {
      int len;
      strcpy(s, p);
      s = cut_bin(cut_lib(s));
      len = strlen(s);
      s = realloc(s, len + 6);
      if (len > 0 && s[len-1] != '/')
        strcat(s, "/lib/");
      else
        strcat(s, "lib/");
      return s;
    }
  }
  return NULL;
}

/*
 * @brief get the path to the plugin directory
 * @return a pointer to the dir path (to be freed by the caller)
 */
static char * os_get_installation_path() {
  size_t n;
  char * tmp;
  char * lpref;
  char * pexe;
  char * modu;
  char * dima;
  char * path;

  lpref = get_path_from_ENV_PREFIX();
#if LINUX
  pexe = get_path_from_proc_exe();
#else
  pexe = NULL;
#endif
#if WINDOWS
  modu = get_path_from_module_filename();
#else
  modu = NULL;
#endif
#if DARWIN
  dima = get_path_from_dyld_image();
#else
  dima = NULL;
#endif
  path = get_path_from_PATH();
  n = 1;
  if (lpref != NULL)
    n += strlen(lpref) + strlen(PLUGINDIR "/:");
  if (pexe != NULL)
    n += strlen(pexe) + strlen(PLUGINDIR "/:");
  if (modu != NULL)
    n += strlen(modu) + strlen(PLUGINDIR "/:");
  if (dima != NULL)
    n += strlen(dima) + strlen(PLUGINDIR "/:");
  if (path != NULL)
    n += strlen(path) + strlen(PLUGINDIR "/:");
  tmp = malloc(n);
  tmp[0] = '\0';
  if (lpref != NULL) {
    strcat(tmp, lpref);
    strcat(tmp, PLUGINDIR "/:");
    free(lpref);
  }
  if (pexe != NULL) {
    strcat(tmp, pexe);
    strcat(tmp, PLUGINDIR "/:");
    free(pexe);
  }
  if (modu != NULL) {
    strcat(tmp, modu);
    strcat(tmp, PLUGINDIR "/:");
    free(modu);
  }
  if (dima != NULL) {
    strcat(tmp, dima);
    strcat(tmp, PLUGINDIR "/:");
    free(dima);
  }
  if (path != NULL) {
    strcat(tmp, path);
    strcat(tmp, PLUGINDIR "/:");
    free(path);
  }
  if (strlen(tmp) > 0)
    tmp[strlen(tmp)-1] = '\0';
  if (strlen(tmp) == 0) {
    free(tmp);
    return NULL;
  }
  return tmp;
}


/* ************library initialization ***************** */

static char * old_dlsearchpath = NULL;

/* using libtool, needs init! */
void __attribute__ ((constructor)) le_ltdl_init() {
  int err;
  const char * opath;
  char * path;
  char * cpath;

#if ENABLE_NLS
  setlocale(LC_ALL, "");
  BINDTEXTDOMAIN(PACKAGE, LOCALEDIR);
  BINDTEXTDOMAIN("iso-639", ISOLOCALEDIR); /* used by wordextractor */
#endif
  err = lt_dlinit ();
  if (err > 0) {
#if DEBUG
    fprintf(stderr,
	    _("Initialization of plugin mechanism failed: %s!\n"),
	    lt_dlerror());
#endif
    return;
  }
  opath = lt_dlgetsearchpath();
  if (opath != NULL)
    old_dlsearchpath = strdup(opath);
  path = os_get_installation_path();
  if (path != NULL) {
    if (opath != NULL) {
      cpath = malloc(strlen(path) + strlen(opath) + 4);
      strcpy(cpath, opath);
      strcat(cpath, ":");
      strcat(cpath, path);
      lt_dlsetsearchpath(cpath);
      free(path);
      free(cpath);
    } else {
      lt_dlsetsearchpath(path);
      free(path);
    }
  }
#ifdef MINGW
  InitWinEnv();
#endif
}

void __attribute__ ((destructor)) le_ltdl_fini() {
  lt_dlsetsearchpath(old_dlsearchpath);
  if (old_dlsearchpath != NULL) {
    free(old_dlsearchpath);
    old_dlsearchpath = NULL;
  }
#ifdef MINGW
  ShutdownWinEnv();
#endif
  lt_dlexit ();
}

/**
 * Open a file
 */
static int fileopen(const char *filename, int oflag, ...)
{
  int mode;
  char *fn;

#ifdef MINGW
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = plibc_conv_to_win_path(filename, szFile)) != ERROR_SUCCESS)
  {
    errno = ENOENT;
    SetLastError(lRet);

    return -1;
  }
  fn = szFile;
#else
  fn = (char *) filename;
#endif

  if (oflag & O_CREAT)
  {
    va_list arg;
    va_start(arg, oflag);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  else
  {
    mode = 0;
  }

#ifdef MINGW
  /* Set binary mode */
  mode |= O_BINARY;
#endif

  return open(fn, oflag, mode);
}



/**
 * Load the default set of libraries. The default set of
 * libraries consists of the libraries that are part of
 * the libextractor distribution (except split and filename
 * extractor) plus the extractors that are specified
 * in the environment variable "LIBEXTRACTOR_LIBRARIES".
 *
 * @return the default set of libraries.
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_loadDefaultLibraries ()
{
  char *env;
  char *tmp;
  EXTRACTOR_ExtractorList *res;


  env = getenv ("LIBEXTRACTOR_LIBRARIES");
  if (env == NULL)
    {
      return EXTRACTOR_loadConfigLibraries (NULL, DEFAULT_LIBRARIES);
    }
  tmp = malloc (strlen (env) + strlen (DEFAULT_LIBRARIES) + 2);
  strcpy (tmp, env);
  strcat (tmp, ":");
  strcat (tmp, DEFAULT_LIBRARIES);
  res = EXTRACTOR_loadConfigLibraries (NULL, tmp);
  free (tmp);
  return res;
}

/**
 * Get the textual name of the keyword.
 * @return NULL if the type is not known
 */
const char *
EXTRACTOR_getKeywordTypeAsString(const EXTRACTOR_KeywordType type)
{
  if ((type >= 0) && (type < HIGHEST_TYPE_NUMBER))
    return keywordTypes[type];
  else
    return NULL;
}

static void *getSymbolWithPrefix(void *lib_handle,
                                 const char *lib_name,
                                 const char *sym_name)
{
  size_t name_size
    = strlen(lib_name)
    + strlen(sym_name)
    + 1 /* for the zero delim. */
    + 1 /* for the optional '_' prefix */;
  char *name=malloc(name_size),*first_error;
  void *symbol=NULL;

  snprintf(name,
	   name_size,
	   "_%s%s",
	   lib_name,
	   sym_name);

  symbol=lt_dlsym(lib_handle,name+1 /* skip the '_' */);
  if (symbol==NULL) {
    first_error=strdup(lt_dlerror());
    symbol=lt_dlsym(lib_handle,name /* now try with the '_' */);
#if DEBUG
    fprintf(stderr,
	    _("Resolving symbol `%s' in library `%s' failed, "
	      "so I tried `%s', but that failed also.  Errors are: "
	      "`%s' and `%s'.\n"),
             name+1,
             lib_name,
             name,
             first_error,
             lt_dlerror());
#endif
    free(first_error);
  }
  free(name);
  return symbol;
}

/**
 * Load a dynamic library.
 * @return 1 on success, -1 on error
 */
static int
loadLibrary (const char *name,
	     void **libHandle,
	     ExtractMethod * method)
{
  *libHandle = lt_dlopenext (name);
  if (*libHandle == NULL)
    {
#if DEBUG
      fprintf (stderr,
	       _("Loading `%s' plugin failed: %s\n"),
	       name,
	       lt_dlerror ());
#endif
      return -1;
    }

  *method = (ExtractMethod) getSymbolWithPrefix (*libHandle, name, "_extract");
  if (*method == NULL) {
    lt_dlclose (*libHandle);
    return -1;
  }
  return 1;
}

/* Internal function that accepts options. */
static EXTRACTOR_ExtractorList *
EXTRACTOR_addLibrary2 (EXTRACTOR_ExtractorList * prev,
		       const char *library, const char *options)
{
  EXTRACTOR_ExtractorList *result;
  void *handle;
  ExtractMethod method;

  if (-1 == loadLibrary (library, &handle, &method))
    return prev;
  result = malloc (sizeof (EXTRACTOR_ExtractorList));
  result->next = prev;
  result->libraryHandle = handle;
  result->extractMethod = method;
  result->libname = strdup (library);
  if( options )
    result->options = strdup (options);
  else
    result->options = NULL;
  return result;
}

/**
 * Add a library for keyword extraction.
 * @param prev the previous list of libraries, may be NULL
 * @param library the name of the library
 * @return the new list of libraries, equal to prev iff an error occured
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_addLibrary (EXTRACTOR_ExtractorList * prev,
		      const char *library)
{
  return EXTRACTOR_addLibrary2(prev, library, NULL);
}

/* Internal function which takes options. */
static EXTRACTOR_ExtractorList *
EXTRACTOR_addLibraryLast2 (EXTRACTOR_ExtractorList * prev,
			   const char *library, const char *options)
{
  EXTRACTOR_ExtractorList *result;
  EXTRACTOR_ExtractorList *pos;
  void *handle;
  ExtractMethod method;

  if (-1 == loadLibrary (library, &handle, &method))
    return prev;
  result = malloc (sizeof (EXTRACTOR_ExtractorList));
  result->next = NULL;
  result->libraryHandle = handle;
  result->extractMethod = method;
  result->libname = strdup (library);
  if( options )
    result->options = strdup (options);
  else
    result->options = NULL;
  if (prev == NULL)
    return result;
  pos = prev;
  while (pos->next != NULL)
    pos = pos->next;
  pos->next = result;
  return prev;
}

/**
 * Add a library for keyword extraction at the END of the list.
 * @param prev the previous list of libraries, may be NULL
 * @param library the name of the library
 * @return the new list of libraries, always equal to prev
 *         except if prev was NULL and no error occurs
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_addLibraryLast (EXTRACTOR_ExtractorList * prev,
			  const char *library)
{
  return EXTRACTOR_addLibraryLast2(prev, library, NULL);
}

/**
 * Load multiple libraries as specified by the user.
 * @param config a string given by the user that defines which
 *        libraries should be loaded. Has the format
 *        "[[-]LIBRARYNAME[:[-]LIBRARYNAME]*]". For example,
 *        libextractor_mp3.so:libextractor_ogg.so loads the
 *        mp3 and the ogg library. The '-' before the LIBRARYNAME
 *        indicates that the library should be added to the end
 *        of the library list (addLibraryLast).
 * @param prev the  previous list of libraries, may be NULL
 * @return the new list of libraries, equal to prev iff an error occured
 *         or if config was empty (or NULL).
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_loadConfigLibraries (EXTRACTOR_ExtractorList * prev,
			       const char *config)
{
  char *cpy;
  int pos;
  int last;
  int lastconf;
  int len;

  if (config == NULL)
    return prev;
  len = strlen(config);
  cpy = strdup(config);
  pos = 0;
  last = 0;
  lastconf = 0;
  while (pos < len)
    {
      while ((cpy[pos] != ':') && (cpy[pos] != '\0') &&
	     (cpy[pos] != '('))
	pos++;
      if( cpy[pos] == '(' ) {
 	cpy[pos++] = '\0';	/* replace '(' by termination */
	lastconf = pos;         /* start config from here, after (. */
	while ((cpy[pos] != '\0') && (cpy[pos] != ')'))
	  pos++; /* config until ) or EOS. */
	if( cpy[pos] == ')' ) {
	  cpy[pos++] = '\0'; /* write end of config here. */
	  while ((cpy[pos] != ':') && (cpy[pos] != '\0'))
	    pos++; /* forward until real end of string found. */
	  cpy[pos++] = '\0';
	} else {
	  cpy[pos++] = '\0'; /* end of string. */
	}
      } else {
	lastconf = -1;         /* NULL config when no (). */
	cpy[pos++] = '\0';	/* replace ':' by termination */
      }
      if (cpy[last] == '-')
	{
	  last++;
	  if( lastconf != -1 )
	    prev = EXTRACTOR_addLibraryLast2 (prev, &cpy[last],
					      &cpy[lastconf]);
	  else
	    prev = EXTRACTOR_addLibraryLast2 (prev, &cpy[last], NULL);
	}
      else
	if( lastconf != -1 )
	  prev = EXTRACTOR_addLibrary2 (prev, &cpy[last], &cpy[lastconf]);
	else
	  prev = EXTRACTOR_addLibrary2 (prev, &cpy[last], NULL);

      last = pos;
    }
  free (cpy);
  return prev;
}

/**
 * Remove a library for keyword extraction.
 * @param prev the current list of libraries
 * @param library the name of the library to remove
 * @return the reduced list, unchanged if the library was not loaded
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_removeLibrary(EXTRACTOR_ExtractorList * prev,
			const char *library)
{
  EXTRACTOR_ExtractorList *pos;
  EXTRACTOR_ExtractorList *first;
  pos = prev;
  first = prev;
  while ((pos != NULL) && (0 != strcmp (pos->libname, library)))
    {
      prev = pos;
      pos = pos->next;
    }
  if (pos != NULL)
    {
      /* found, close library */
      if (first == pos)
	first = pos->next;
      else
	prev->next = pos->next;
      /* found */
      free (pos->libname);
      if( pos->options )
	free (pos->options);
      if( pos->libraryHandle )
	lt_dlclose (pos->libraryHandle);
      free (pos);
    }
#if DEBUG
  else
    fprintf(stderr,
	    _("Unloading plugin `%s' failed!\n"),
	    library);
#endif
  return first;
}

/**
 * Remove all extractors.
 * @param libraries the list of extractors
 */
void
EXTRACTOR_removeAll (EXTRACTOR_ExtractorList * libraries)
{
  while (libraries != NULL)
    libraries = EXTRACTOR_removeLibrary (libraries, libraries->libname);
}



/**
 * How many bytes do we actually try to scan? (from the beginning
 * of the file).  Limit to 1 GB.
 */
#define MAX_READ 1024 * 1024 * 1024

/**
 * How many bytes do we actually try to decompress? (from the beginning
 * of the file).  Limit to 16 MB.
 */
#define MAX_DECOMPRESS 16 * 1024 * 1024


static EXTRACTOR_KeywordList *
getKeywords (EXTRACTOR_ExtractorList * extractor,
	     const char * filename,
	     const unsigned char * data,
	     size_t size) {
  EXTRACTOR_KeywordList *result;
  unsigned char * buf;
  size_t dsize;
#if HAVE_ZLIB
  z_stream strm;
  int ret;
  size_t pos;
#endif
#if HAVE_LIBBZ2
  bz_stream bstrm;
  int bret;
  size_t bpos;
#endif

  result = NULL;
  buf = NULL;
  dsize = 0;
#if HAVE_ZLIB
  /* try gzip decompression first */
  if ( (size >= 12) &&
       (data[0] == 0x1f) &&
       (data[1] == 0x8b) &&
       (data[2] == 0x08) ) {

    /*
     * Skip gzip header - we might want to retrieve parts of it as keywords
     */
    unsigned gzip_header_length = 10;

    if (data[3] & 0x4) /* FEXTRA  set */
      gzip_header_length += 2 + (unsigned) (data[10] & 0xff)
                              + (((unsigned) (data[11] & 0xff)) * 256);

    if(data[3] & 0x8) /* FNAME set */
    {
      const unsigned char * cptr = data + gzip_header_length;

      /*
       * stored file name is here
       * extremely long file names might break the following code.
       */

      while(cptr < data + size)
      {
        if('\0' == *cptr)
          break;

        cptr++;
      }
      gzip_header_length = (cptr - data) + 1;
    }

    if(data[3] & 0x16) /* FCOMMENT set */
    {
      const unsigned char * cptr = data + gzip_header_length;

      /*
       * stored comment is here
       */

      while(cptr < data + size)
      {
        if('\0' == *cptr)
          break;

        cptr ++;
      }

      gzip_header_length = (cptr - data) + 1;
    }

    if(data[3] & 0x2) /* FCHRC set */
      gzip_header_length += 2;

    memset(&strm,
	   0,
	   sizeof(z_stream));
#ifdef ZLIB_VERNUM
    gzip_header_length = 0;
#endif
    if (size > gzip_header_length) {
      strm.next_in = (Bytef*) data + gzip_header_length;
      strm.avail_in = size - gzip_header_length;
    } else {
      strm.next_in = (Bytef*) data;
      strm.avail_in = 0;
    }
    strm.total_in = 0;
    strm.zalloc = NULL;
    strm.zfree = NULL;
    strm.opaque = NULL;

    /*
     * note: maybe plain inflateInit(&strm) is adequate,
     * it looks more backward-compatible also ;
     *
     * ZLIB_VERNUM isn't defined by zlib version 1.1.4 ;
     * there might be a better check.
     */
#ifdef ZLIB_VERNUM
    if (Z_OK == inflateInit2(&strm,
			     15 + 32)) {
#else
    if (Z_OK == inflateInit2(&strm,
			     -MAX_WBITS)) {
#endif
      dsize = 2 * size;
      if (dsize > MAX_DECOMPRESS)
	dsize = MAX_DECOMPRESS;
      buf = malloc(dsize);
      pos = 0;
      if (buf == NULL) {
	inflateEnd(&strm);
      } else {
	strm.next_out = (Bytef*) buf;
	strm.avail_out = dsize;
	do {
	  ret = inflate(&strm,
			Z_SYNC_FLUSH);
	  if (ret == Z_OK) {
	    if (dsize == MAX_DECOMPRESS)
	      break;
	    pos += strm.total_out;
	    strm.total_out = 0;
	    dsize *= 2;
	    if (dsize > MAX_DECOMPRESS)
	      dsize = MAX_DECOMPRESS;
	    buf = realloc(buf, dsize);
	    strm.next_out = (Bytef*) &buf[pos];
	    strm.avail_out = dsize - pos;
	  } else if (ret != Z_STREAM_END) {
	    /* error */
	    free(buf);
	    buf = NULL;
	  }
	} while ( (buf != NULL) &&		
		  (ret != Z_STREAM_END) );
	dsize = pos + strm.total_out;
	inflateEnd(&strm);
	if (dsize == 0) {
	  free(buf);
	  buf = NULL;
	}
      }
    }
  }
#endif

#if HAVE_LIBBZ2
  if ( (size >= 4) &&
       (data[0] == 'B') &&
       (data[1] == 'Z') &&
       (data[2] == 'h') ) {
    /* now try bz2 decompression */
    memset(&bstrm,
	   0,
	   sizeof(bz_stream));
    bstrm.next_in = (char*) data;
    bstrm.avail_in = size;
    bstrm.total_in_lo32 = 0;
    bstrm.total_in_hi32 = 0;
    bstrm.bzalloc = NULL;
    bstrm.bzfree = NULL;
    bstrm.opaque = NULL;
    if ( (buf == NULL) &&
	 (BZ_OK == BZ2_bzDecompressInit(&bstrm,
					0,
					0)) ) {
      dsize = 2 * size;
      if (dsize > MAX_DECOMPRESS)
	dsize = MAX_DECOMPRESS;
      buf = malloc(dsize);
      bpos = 0;
      if (buf == NULL) {
	BZ2_bzDecompressEnd(&bstrm);
      } else {
	bstrm.next_out = (char*) buf;
	bstrm.avail_out = dsize;
	do {
	  bret = BZ2_bzDecompress(&bstrm);
	  if (bret == Z_OK) {
	    if (dsize == MAX_DECOMPRESS)
	      break;
	    bpos += bstrm.total_out_lo32;
	    bstrm.total_out_lo32 = 0;
	    dsize *= 2;
	    if (dsize > MAX_DECOMPRESS)
	      dsize = MAX_DECOMPRESS;
	    buf = realloc(buf, dsize);
	    bstrm.next_out = (char*) &buf[bpos];
	    bstrm.avail_out = dsize - bpos;
	  } else if (bret != BZ_STREAM_END) {
	    /* error */
	    free(buf);
	    buf = NULL;
	  }
	} while ( (buf != NULL) &&
		  (bret != BZ_STREAM_END) );
	dsize = bpos + bstrm.total_out_lo32;
	BZ2_bzDecompressEnd(&bstrm);
	if (dsize == 0) {
	  free(buf);
	  buf = NULL;
	}
      }
    }
  }
#endif


  /* finally, call plugins */
  if (buf != NULL) {
    data = buf;
    size = dsize;
  }
  while (extractor != NULL) {
    result = extractor->extractMethod(filename,
				      (char*) data,
				      size,
				      result,
				      extractor->options);
    extractor = extractor->next;
  }
  if (buf != NULL)
    free(buf);
  errno = 0; /* kill transient errors */
  return result;
}

/**
 * Extract keywords from a file using the available extractors.
 * @param extractor the list of extractor libraries
 * @param filename the name of the file
 * @return the list of keywords found in the file, NULL if none
 *         were found (or other errors)
 */
EXTRACTOR_KeywordList *
EXTRACTOR_getKeywords (EXTRACTOR_ExtractorList * extractor,
		       const char * filename) {
  EXTRACTOR_KeywordList *result;
  int file;
  void * buffer;
  struct stat fstatbuf;
  size_t size;
  int eno, dir;

  if (-1 == STAT(filename, &fstatbuf))
    return NULL;

  if (!S_ISDIR(fstatbuf.st_mode)) {
    dir = 0;
      
#ifdef O_LARGEFILE
    file = fileopen(filename, O_RDONLY | O_LARGEFILE);
#else
    file = fileopen(filename, O_RDONLY);
#endif
    if (-1 == file)
      return NULL;
  
    size = fstatbuf.st_size;
    if (size == 0) {
      close(file);
      return NULL;
    }
  
    if (size > MAX_READ)
      size = MAX_READ; /* do not mmap/read more than 1 GB! */
    buffer = MMAP(NULL, size, PROT_READ, MAP_PRIVATE, file, 0);
    if ( (buffer == NULL) || (buffer == (void *) -1) ) {
      eno = errno;
      close(file);
      errno = eno;
      return NULL;
    }
  }
  else {
    dir = 1;
    
    size = 0;
    buffer = malloc(1);
  }
  
  result = getKeywords(extractor,
		       filename,
		       buffer,
		       size);
  
  if (dir)
    free(buffer);
  else {
    MUNMAP (buffer, size);
    close(file);
  }
  return result;
}



/**
 * Extract keywords from a buffer in memory
 * using the available extractors.
 *
 * @param extractor the list of extractor libraries
 * @param data the data of the file
 * @param size the number of bytes in data
 * @return the list of keywords found in the file, NULL if none
 *         were found (or other errors)
 */
EXTRACTOR_KeywordList *
EXTRACTOR_getKeywords2(EXTRACTOR_ExtractorList * extractor,
		       const void * data,
		       size_t size) {
  if (data == NULL)
    return NULL;
  return getKeywords(extractor,
		     NULL,
		     data,
		     size);
}

static void
removeKeyword (const char *keyword,
	       const EXTRACTOR_KeywordType type,
	       const unsigned int options,
	       EXTRACTOR_KeywordList ** list,
	       EXTRACTOR_KeywordList * current) {
  EXTRACTOR_KeywordList *first;
  EXTRACTOR_KeywordList *pos;
  EXTRACTOR_KeywordList *prev;
  EXTRACTOR_KeywordList *next;

  first = *list;
  pos = first;
  prev = NULL;
  while (pos != NULL) {
    if (pos == current) {
      prev = pos;
      pos = current->next;
    }
    if (pos == NULL)
      break;
    if ( (0 == strcmp (pos->keyword, keyword)) &&
	 ( (pos->keywordType == type) ||
	   ( ((options & EXTRACTOR_DUPLICATES_TYPELESS) > 0) &&
	     ( (pos->keywordType == EXTRACTOR_SPLIT) ||
	       (type != EXTRACTOR_SPLIT)) ) ||
	   ( ((options & EXTRACTOR_DUPLICATES_REMOVE_UNKNOWN) > 0) &&
	     (pos->keywordType == EXTRACTOR_UNKNOWN)) ) ) {
      /* remove! */
      if (prev == NULL)
	first = pos->next;
      else
	prev->next = pos->next;
      next = pos->next;
      free (pos->keyword);
      free (pos);
      pos = next;
    } else {
      prev = pos;
      pos = pos->next;
    }
  } /* end while */
  *list = first;
}

/**
 * Remove duplicate keywords from the list.
 * @param list the original keyword list (destroyed in the process!)
 * @param options a set of options (DUPLICATES_XXXX)
 * @return a list of keywords without duplicates
 */
EXTRACTOR_KeywordList *
EXTRACTOR_removeDuplicateKeywords (EXTRACTOR_KeywordList * list,
				   const unsigned int options) {
  EXTRACTOR_KeywordList *pos;

  pos = list;
  while (pos != NULL) {
    removeKeyword(pos->keyword,
		  pos->keywordType,
		  options,
		  &list,
		  pos);
    pos = pos->next;
  }
  return list;
}

/**
 * Remove empty (all-whitespace) keywords from the list.
 * @param list the original keyword list (destroyed in the process!)
 * @return a list of keywords without duplicates
 */
EXTRACTOR_KeywordList *
EXTRACTOR_removeEmptyKeywords (EXTRACTOR_KeywordList * list) {
  EXTRACTOR_KeywordList * pos;
  EXTRACTOR_KeywordList * last;

  last = NULL;
  pos = list;
  while (pos != NULL)
    {
      int allWhite;
      int i;
      allWhite = 1;
      for (i=strlen(pos->keyword)-1;i>=0;i--)
	if (! isspace(pos->keyword[i]))
	  allWhite = 0;
      if (allWhite)
	{
	  EXTRACTOR_KeywordList * next;
	  next = pos->next;
	  if (last == NULL)
	    list = next;
	  else
	    last->next = next;
	  free(pos->keyword);
	  free(pos);
	  pos = next;
	}
      else
	{
	  last = pos;
	  pos = pos->next;
	}
    }
  return list;
}

/**
 * Remove keywords of a particular type from the list.
 * @param list the original keyword list (altered in the process!)
 * @param type the type to remove
 * @return a list of keywords without entries of given type
 */
EXTRACTOR_KeywordList *
EXTRACTOR_removeKeywordsOfType(EXTRACTOR_KeywordList * list,
			       EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * pos;
  EXTRACTOR_KeywordList * last;

  last = NULL;
  pos = list;
  while (pos != NULL) {
    if (pos->keywordType == type) {
      EXTRACTOR_KeywordList * next;
      next = pos->next;
      if (last == NULL)
	list = next;
      else
	last->next = next;
      free(pos->keyword);
      free(pos);
      pos = next;
    } else {
      last = pos;
      pos = pos->next;
    }
  }
  return list;
}

#include "iconv.c"

/**
 * Print a keyword list to a file.
 * For debugging.
 * @param handle the file to write to (stdout, stderr), may NOT be NULL
 * @param keywords the list of keywords to print, may be NULL
 */
void
EXTRACTOR_printKeywords(FILE * handle,
			EXTRACTOR_KeywordList * keywords)
{
  iconv_t cd;
  char * buf;

  cd = iconv_open(
    nl_langinfo(CODESET)
    , "UTF-8");
  while (keywords != NULL)
    {
      if (cd == (iconv_t) -1)
	buf = strdup(keywords->keyword);
      else
	buf = iconvHelper(cd,
			  keywords->keyword);
      if (keywords->keywordType == EXTRACTOR_THUMBNAIL_DATA) {
	fprintf(handle,
		_("%s - (binary)\n"),
		_(keywordTypes[keywords->keywordType]));
      } else {
	if (keywords->keywordType >= HIGHEST_TYPE_NUMBER)
	  fprintf(handle,
		  _("INVALID TYPE - %s\n"),
		  buf);
	else
	  fprintf(handle,
		  "%s - %s\n",
		  _(keywordTypes[keywords->keywordType]),
		  buf);
      }
      free(buf);
      keywords = keywords->next;
    }
  if (cd != (iconv_t) -1)
    iconv_close(cd);
}

/**
 * Free the memory occupied by the keyword list (and the
 * keyword strings in it!)
 * @param keywords the list to free
 */
void
EXTRACTOR_freeKeywords (EXTRACTOR_KeywordList * keywords)
{
  EXTRACTOR_KeywordList *prev;
  while (keywords != NULL)
    {
      prev = keywords;
      keywords = keywords->next;
      free (prev->keyword);
      free (prev);
    }
}

/**
 * Return the highest type number, exclusive as in [0,highest).
 */
EXTRACTOR_KeywordType
EXTRACTOR_getHighestKeywordTypeNumber ()
{
  return HIGHEST_TYPE_NUMBER;
}

/**
 * Extract the last keyword that of the given type from the keyword list.
 * @param type the type of the keyword
 * @param keywords the keyword list
 * @return the last matching keyword, or NULL if none matches
 */
const char *
EXTRACTOR_extractLast (const EXTRACTOR_KeywordType type,
		       EXTRACTOR_KeywordList * keywords)
{
  char *result = NULL;
  while (keywords != NULL)
    {
      if (keywords->keywordType == type)
	result = keywords->keyword;
      keywords = keywords->next;
    }
  return result;
}

/**
 * Extract the last keyword of the given string from the keyword list.
 * @param type the string describing the type of the keyword
 * @param keywords the keyword list
 * @return the last matching keyword, or NULL if none matches
 */
const char *
EXTRACTOR_extractLastByString (const char * type,
			       EXTRACTOR_KeywordList * keywords)
{
  char * result = NULL;

  if (type == NULL)
    return NULL;
  while (keywords != NULL) {
    if ( (0 == strcmp(_(keywordTypes[keywords->keywordType]), type)) ||
	 (0 == strcmp(keywordTypes[keywords->keywordType], type) ) )
      result = keywords->keyword;
    keywords = keywords->next;
  }
  return result;
}

/**
 * Count the number of keywords in the keyword list.
 * @param keywords the keyword list
 * @return the number of keywords in the list
 */
unsigned int
EXTRACTOR_countKeywords (EXTRACTOR_KeywordList * keywords)
{
  int count = 0;
  while (keywords != NULL)
    {
      count++;
      keywords = keywords->next;
    }
  return count;
}

/**
 * Encode the given binary data object
 * as a 0-terminated C-string according
 * to the LE binary data encoding standard.
 *
 * @return NULL on error, the 0-terminated
 *  encoding otherwise
 */
char * EXTRACTOR_binaryEncode(const unsigned char * data,
			      size_t size) {

  char * binary;
  size_t pos;
  size_t end;
  size_t wpos;
  size_t i;
  unsigned int markers[8]; /* 256 bits */
  unsigned char marker;

 /* encode! */
  binary = malloc(2 + size + (size+256) / 254);
  if (binary == NULL)
    return NULL;

  pos = 0;
  wpos = 0;
  while (pos < size) {
    /* find unused value between 1 and 255 in
       the next 254 bytes */
    end = pos + 254;
    if (end < pos)
      break; /* integer overflow! */
    if (end > size)
      end = size;
    memset(markers,
	   0,
	   sizeof(markers));
    for (i=pos;i<end;i++)
      markers[data[i]&7] |= 1 << (data[i] >> 3);
    marker = 1;
    while (markers[marker&7] & (1 << (marker >> 3))) {
      marker++;
      if (marker == 0) {
	/* assertion failed... */
	free(binary);
	return NULL;
      }
    }
    /* recode */
    binary[wpos++] = marker;
    for (i=pos;i<end;i++)
      binary[wpos++] = data[i] == 0 ? marker : data[i];
    pos = end;
  }
  binary[wpos++] = 0; /* 0-termination! */
  return binary;
}


/**
 * This function can be used to decode the binary data
 * encoded in the libextractor metadata (i.e. for
 * the  thumbnails).
 *
 * @param in 0-terminated string from the meta-data
 * @return 1 on error, 0 on success
 */
int EXTRACTOR_binaryDecode(const char * in,
			   unsigned char ** out,
			   size_t * outSize) {
  unsigned char * buf;
  size_t pos;
  size_t wpos;
  unsigned char marker;
  size_t i;
  size_t end;
  size_t inSize;

  inSize = strlen(in);
  if (inSize == 0) {
    *out = NULL;
    *outSize = 0;
    return 0;
  }

  buf = malloc(inSize); /* slightly more than needed ;-) */
  *out = buf;

  pos = 0;
  wpos = 0;
  while (pos < inSize) {
    end = pos + 255; /* 255 here: count the marker! */
    if (end > inSize)
      end = inSize;
    marker = in[pos++];
    for (i=pos;i<end;i++)
      buf[wpos++] = (in[i] == (char) marker) ? 0 : in[i];
    pos = end;
  }
  *outSize = wpos;
  return 0;
}



/* end of extractor.c */
