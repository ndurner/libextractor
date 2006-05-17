/*
     This file is part of libextractor.
     (C) 2002, 2003, 2006 Vidyut Samanta and Christian Grothoff

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
#include <zlib.h>
#include <math.h>

#define DEBUG 0

typedef struct {
  unsigned int size;
  unsigned int type;
} Atom;

typedef struct {
  unsigned int one;
  unsigned int type;
  unsigned long long size;
} LongAtom;

static unsigned long long ntohll(unsigned long long n) {
#if __BYTE_ORDER == __BIG_ENDIAN
  return n;
#else
  return (((unsigned long long)ntohl(n)) << 32) + ntohl(n >> 32);
#endif
}

static void
addKeyword(EXTRACTOR_KeywordType type,
	   const char * keyword,
	   struct EXTRACTOR_Keywords ** list) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = *list;
  result->keyword = strdup(keyword);
  result->keywordType = type;
  *list = result;
}


/**
 * Check if at position pos there is a valid atom.
 * @return 0 if the atom is invalid, 1 if it is valid
 */
static int checkAtomValid(const char * buffer,	
			  size_t size,
			  size_t pos) {
  unsigned long long atomSize;
  const Atom * atom;
  const LongAtom * latom;
  if ( (pos >= size) ||
       (pos + sizeof(Atom) > size) ||
       (pos + sizeof(Atom) < pos) )
    return 0;
  atom = (const Atom*) &buffer[pos];
  if (ntohl(atom->size) == 1) {
    if ( (pos + sizeof(LongAtom) > size) ||
	 (pos + sizeof(LongAtom) < pos) )
      return 0;
    latom = (const LongAtom*) &buffer[pos];
    atomSize = ntohll(latom->size);
    if ( (atomSize < sizeof(LongAtom)) ||
	 (atomSize + pos > size) ||
	 (atomSize + pos < atomSize) )
      return 0;
  } else {
    atomSize = ntohl(atom->size);
    if ( (atomSize < sizeof(Atom)) ||
	 (atomSize + pos > size) ||
	 (atomSize + pos < atomSize) )
      return 0;
  }
  return 1;
}

/**
 * Assumes that checkAtomValid has already been called.
 */
static unsigned long long getAtomSize(const char * buf) {
  const Atom * atom;
  const LongAtom * latom;
  atom = (const Atom*) buf;
  if (ntohl(atom->size) == 1) {
    latom = (const LongAtom*) buf;
    return ntohll(latom->size);
  }
  return ntohl(atom->size);
}

/**
 * Assumes that checkAtomValid has already been called.
 */
static unsigned int getAtomHeaderSize(const char * buf) {
  const Atom * atom;

  atom = (const Atom*) buf;
  if (ntohl(atom->size) == 1) 
    return sizeof(const LongAtom);  
  return sizeof(Atom);
}

/**
 * Assumes that checkAtomValid has already been called.
 */
typedef int (*AtomHandler)(const char * input,
			   size_t size,
			   size_t pos,
			   struct EXTRACTOR_Keywords ** list);

/**
 * Call the handler for the atom at the given position.
 * Will check validity of the given atom.
 *
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int handleAtom(const char * input,
		      size_t size,
		      size_t pos,
		      struct EXTRACTOR_Keywords ** list);

/**
 * Process all atoms.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int processAllAtoms(const char * input,
			   size_t size,
			   struct EXTRACTOR_Keywords ** list) {
  size_t pos;

  if (size < sizeof(Atom))
    return 1;
  pos = 0;
  while (pos < size - sizeof(Atom)) {
    if (0 == handleAtom(input,
			size,
			pos,
			list))
      return 0;
    pos += getAtomSize(&input[pos]);
  }
  return 1;
}

/**
 * Handle the moov atom.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int moovHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  unsigned int hdr = getAtomHeaderSize(&input[pos]);
  return processAllAtoms(&input[pos+hdr], 
			 getAtomSize(&input[pos]) - hdr,
			 list);
}

typedef struct {
  Atom header;
  /* major brand */
  char type[4];
  /* minor version */
  unsigned int version;
  /* compatible brands */
  char compatibility[4]; 
} FileType;

typedef struct {
  const char * ext;
  const char * mime;
} C2M;

static C2M ftMap[] = {
  { "qt  ", "video/quicktime" },
  { "isom", "video/mp4" },  /* ISO Base Media files */
  { "mp41", "video/mp4" },  /* MPEG-4 (ISO/IEC 14491-1) version 1 */
  { "mp42", "video/mp4" },  /* MPEG-4 (ISO/IEC 14491-1) version 2 */
  { "3gp1", "video/3gpp"},
  { "3gp2", "video/3gpp"},
  { "3gp3", "video/3gpp"},
  { "3gp4", "video/3gpp"},
  { "3gp5", "video/3gpp"},
  { "3g2a", "video/3gpp2"},
  { "mmp4", "video/mp4"},    /* Mobile MPEG-4 */
  { "M4A ", "video/mp4"},
  { "M4P ", "video/mp4"},
  { "mjp2", "video/mj2"},    /* Motion JPEG 2000 */
  { NULL, NULL },
};

static int ftypHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  const FileType * ft;
  int i;

  if (getAtomSize(&input[pos]) != sizeof(FileType))
    return 0;
  ft = (const FileType* ) &input[pos];

  i = 0;
  while ( (ftMap[i].ext != NULL) &&
	  (0 != memcmp(ft->type, ftMap[i].ext, 4)) )
    i++;
  if (ftMap[i].ext != NULL) 
    addKeyword(EXTRACTOR_MIMETYPE,
	       ftMap[i].mime,
	       list);  
  return 1;
}

typedef struct {
  Atom hdr;
  unsigned char version;
  unsigned char flags[3];
  /* in seconds since midnight, January 1, 1904 */
  unsigned int creationTime;
  /* in seconds since midnight, January 1, 1904 */
  unsigned int modificationTime;
  /* number of time units that pass per second in the movies time
     coordinate system */
  unsigned int timeScale;
  /* A time value that indicates the duration of the movie in time
     scale units. */
  unsigned int duration;
  unsigned int preferredRate;
  /* A 16-bit fixed-point number that specifies how loud to 
     play. 1.0 indicates full volume */
  unsigned short preferredVolume;
  unsigned char reserved[10];
  unsigned char matrix[36];
  unsigned int previewTime;
  unsigned int previewDuration;
  unsigned int posterTime;
  unsigned int selectionTime;
  unsigned int selectionDuration;
  unsigned int currentTime;
  unsigned int nextTrackId;
} MovieHeaderAtom;

static int mvhdHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  const MovieHeaderAtom * m;
  char duration[16];
  if (getAtomSize(&input[pos]) != sizeof(MovieHeaderAtom))
    return 0;
  m = (const MovieHeaderAtom* ) &input[pos];
  snprintf(duration, 16, "%us", ntohl(m->duration) / ntohl(m->timeScale));
  addKeyword(EXTRACTOR_DURATION,
	     duration,
	     list);
  return 1;
}

typedef struct {
  Atom cmovAtom;
  Atom dcomAtom;
  char compressor[4];
  Atom cmvdAtom;
  unsigned int decompressedSize;
} CompressedMovieHeaderAtom;

static int cmovHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  const CompressedMovieHeaderAtom * c;
  unsigned int s;
  char * buf;
  int ret; 
  z_stream z_state;
  int z_ret_code;


  if (getAtomSize(&input[pos]) < sizeof(CompressedMovieHeaderAtom))
    return 0;
  c = (const CompressedMovieHeaderAtom * ) &input[pos];
  if ( (ntohl(c->dcomAtom.size) != 12) ||
       (0 != memcmp(&c->dcomAtom.type, "dcom", 4)) ||
       (0 != memcmp(c->compressor, "zlib", 4)) ||
       (0 != memcmp(&c->cmvdAtom.type, "cmvd", 4)) ||
       (ntohl(c->cmvdAtom.size) != getAtomSize(&input[pos]) - sizeof(Atom) * 2 - 4)) {
    return 0; /* dcom must be 12 bytes */
  }
  s = ntohl(c->decompressedSize);
  if (s > 16 * 1024 * 1024)
    return 1; /* ignore, too big! */
  buf = malloc(s);
  if (buf == NULL)
    return 1; /* out of memory, handle gracefully */
  
  z_state.next_in = (unsigned char*) &c[1];
  z_state.avail_in = ntohl(c->cmvdAtom.size);
  z_state.avail_out = s;
  z_state.next_out = (unsigned char*) buf;
  z_state.zalloc = (alloc_func)0;
  z_state.zfree = (free_func)0;
  z_state.opaque = (voidpf)0;
  z_ret_code = inflateInit (&z_state);
  if (Z_OK != z_ret_code) {
    free(buf);
    return 0; /* crc error? */
  }
  z_ret_code = inflate(&z_state, Z_NO_FLUSH);
  if ((z_ret_code != Z_OK) && (z_ret_code != Z_STREAM_END)) {
    free(buf);
    return 0; /* decode error? */
  }
  z_ret_code = inflateEnd(&z_state);
  if (Z_OK != z_ret_code) {
    free(buf);
    return 0; /* decode error? */
  }
  ret = handleAtom(buf,
		   s,
		   0,
		   list);
  free(buf);
  return ret;
}

typedef struct {
  short integer;
  short fraction;
} Fixed;

typedef struct {
  Atom hdr;
  unsigned int flags; /* 1 byte of version, 3 bytes of flags */
  /* in seconds since midnight, January 1, 1904 */
  unsigned int creationTime;
  /* in seconds since midnight, January 1, 1904 */
  unsigned int modificationTime;
  unsigned int trackID;
  unsigned int reserved_0;
  unsigned int duration;
  unsigned int reserved_1;
  unsigned int reserved_2;
  unsigned short layer;
  unsigned short alternate_group;
  unsigned short volume;
  unsigned short reserved_3;
  Fixed matrix[3][3];
  /* in pixels */
  Fixed track_width;
  /* in pixels */
  Fixed track_height;
} TrackAtom;

static int tkhdHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  const TrackAtom * m;
  char dimensions[40];

  if (getAtomSize(&input[pos]) < sizeof(TrackAtom))
    return 0;
  m = (const TrackAtom* ) &input[pos];
  if (ntohs(m->track_width.integer) != 0) {
    /* if actually a/the video track */
    snprintf(dimensions, 
	     40, 
	     "%dx%d", 
	     ntohs(m->track_width.integer), 
	     ntohs(m->track_height.integer));
    addKeyword(EXTRACTOR_FORMAT,
	       dimensions,
	       list);
  }
  return 1;
}

static int trakHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  unsigned int hdr = getAtomHeaderSize(&input[pos]);
  return processAllAtoms(&input[pos+hdr], 
			 getAtomSize(&input[pos]) - hdr,
			 list);
}

static int metaHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  unsigned int hdr = getAtomHeaderSize(&input[pos]);
  if (getAtomSize(&input[pos]) < hdr + 4)
    return 0;
  return processAllAtoms(&input[pos+hdr+4], 
			 getAtomSize(&input[pos]) - hdr - 4,
			 list);
}

typedef struct {
  Atom header;
  unsigned short length;
  unsigned short language;
} InternationalText;

static const char * languages[] = {
  "English",
  "French",
  "German",
  "Italian",
  "Dutch",
  "Swedish",
  "Spanish",
  "Danish",
  "Portuguese",
  "Norwegian",
  "Hebrew",
  "Japanese",
  "Arabic",
  "Finnish",
  "Greek",
  "Icelandic",
  "Maltese",
  "Turkish",
  "Croatian",
  "Traditional Chinese",
  "Urdu",
  "Hindi",
  "Thai",
  "Korean",
  "Lithuanian",
  "Polish",
  "Hungarian",
  "Estonian",
  "Lettish",
  "Saamisk",
  "Lappish",
  "Faeroese",
  "Farsi",
  "Russian",
  "Simplified Chinese",
  "Flemish",
  "Irish",
  "Albanian",
  "Romanian",
  "Czech",
  "Slovak",
  "Slovenian",
  "Yiddish",
  "Serbian",
  "Macedonian",
  "Bulgarian",
  "Ukrainian",
  "Byelorussian",
  "Uzbek",
  "Kazakh",
  "Azerbaijani",
  "AzerbaijanAr",
  "Armenian",
  "Georgian",
  "Moldavian",
  "Kirghiz",
  "Tajiki",
  "Turkmen",
  "Mongolian",
  "MongolianCyr",
  "Pashto",
  "Kurdish",
  "Kashmiri",
  "Sindhi",
  "Tibetan",
  "Nepali",
  "Sanskrit",
  "Marathi",
  "Bengali",
  "Assamese",
  "Gujarati",
  "Punjabi",
  "Oriya",
  "Malayalam",
  "Kannada",
  "Tamil",
  "Telugu",
  "Sinhalese",
  "Burmese",
  "Khmer",
  "Lao",
  "Vietnamese",
  "Indonesian",
  "Tagalog",
  "MalayRoman",
  "MalayArabic",
  "Amharic",
  "Tigrinya",
  "Galla",
  "Oromo",
  "Somali",
  "Swahili",
  "Ruanda",
  "Rundi",
  "Chewa",
  "Malagasy",
  "Esperanto",
  "Welsh",
  "Basque",
  "Catalan",
  "Latin",
  "Quechua",
  "Guarani",
  "Aymara",
  "Tatar",
  "Uighur",
  "Dzongkha",
  "JavaneseRom",
};


static int processTextTag(const char * input,
			  size_t size,
			  size_t pos,
			  EXTRACTOR_KeywordType type,
			  struct EXTRACTOR_Keywords ** list) {
  unsigned long long as;
  unsigned short len;
  unsigned short lang;
  const InternationalText * txt;
  char * meta;
  int i;

  /* contains "international text":
   16-bit size + 16 bit language code */
  as = getAtomSize(&input[pos]);
  if (as < sizeof(InternationalText))
    return 0; /* invalid */
  txt = (const InternationalText*) &input[pos];
  len = ntohs(txt->length);
  if (len + sizeof(InternationalText) > as)
    return 0; /* invalid */
  lang = ntohs(txt->language);
  if (lang > 138)
    return 0; /* invalid */
  addKeyword(EXTRACTOR_LANGUAGE,
	     languages[lang],
	     list);
  /* TODO: what is the character set encoding here? 
     For now, let's assume it is Utf-8 (cannot find
     anything in the public documentation) */
  meta = malloc(len + 1);
  memcpy(meta, &txt[1], len);
  meta[len] = '\0';
  for (i=0;i<len;i++)
    if (meta[i] == '\r')
      meta[i] = '\n';
  addKeyword(type,
	     meta,
	     list);
  free(meta);
  return 1;
}

typedef struct CHE {
  const char * pfx;
  EXTRACTOR_KeywordType type;
} CHE;

static CHE cHm[] = {
  { "aut", EXTRACTOR_AUTHOR, },
  { "cpy", EXTRACTOR_COPYRIGHT, },
  { "day", EXTRACTOR_CREATION_DATE, },
  { "cmt", EXTRACTOR_COMMENT, },
  { "hst", EXTRACTOR_BUILDHOST, },
  { "inf", EXTRACTOR_INFORMATION, },
  { "nam", EXTRACTOR_FULL_NAME, },
  { "mak", EXTRACTOR_CAMERA_MAKE, },
  { "mod", EXTRACTOR_CAMERA_MODEL, },
  { "des", EXTRACTOR_DESCRIPTION, },
  { "dis", EXTRACTOR_DISCLAIMER, },
  { "dir", EXTRACTOR_MOVIE_DIRECTOR, },
  { "src", EXTRACTOR_CONTRIBUTOR, }, 
  { "prf", EXTRACTOR_ARTIST, }, /* performer */
  { "req", EXTRACTOR_CREATED_FOR, }, /* hardware requirements */
  { "fmt", EXTRACTOR_FORMAT, },
  { "prd", EXTRACTOR_PRODUCER, },
  { "PRD", EXTRACTOR_PRODUCTVERSION, }, /* just product */
  { "swr", EXTRACTOR_SOFTWARE, },
  { "wrt", EXTRACTOR_AUTHOR, }, /* writer */
  { "wrn", EXTRACTOR_WARNING, },
  { "ed1", EXTRACTOR_REVISION_HISTORY, },
  { "ed2", EXTRACTOR_REVISION_HISTORY, },
  { "ed3", EXTRACTOR_REVISION_HISTORY, },
  { "ed4", EXTRACTOR_REVISION_HISTORY, },
  { "ed5", EXTRACTOR_REVISION_HISTORY, },
  { "ed6", EXTRACTOR_REVISION_HISTORY, },
  { "ed7", EXTRACTOR_REVISION_HISTORY, },
  { "ed8", EXTRACTOR_REVISION_HISTORY, },
  { "ed9", EXTRACTOR_REVISION_HISTORY, }, 
  { "chp", EXTRACTOR_CHAPTER, }, 
  { NULL, EXTRACTOR_UNKNOWN },
};

static int c_Handler(const char * input,
		     size_t size,
		     size_t pos,
		     struct EXTRACTOR_Keywords ** list) {
  int i;

  i = 0;
  while ( (cHm[i].pfx != NULL) &&
	  (0 != memcmp(&input[5], cHm[i].pfx, 3)) )
    i++;
  if (cHm[i].pfx != NULL)
    return processTextTag(input,
			  size,
			  pos,
			  cHm[i].type,
			  list);
  return -1; /* not found */
}

static int udtaHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  unsigned int hdr = getAtomHeaderSize(&input[pos]);
  return processAllAtoms(&input[pos+hdr], 
			 getAtomSize(&input[pos]) - hdr,
			 list);
}

typedef struct {
  char * name;
  AtomHandler handler;
} HandlerEntry;

static HandlerEntry handlers[] = {
  { "moov", &moovHandler },
  { "cmov", &cmovHandler },
  { "mvhd", &mvhdHandler },
  { "trak", &trakHandler },
  { "tkhd", &tkhdHandler },
  { "meta", &metaHandler },
  { "udta", &udtaHandler },
  { "ftyp", &ftypHandler },
  { "\xa9""swr", &c_Handler },
  { "\xa9""cpy", &c_Handler },
  { "\xa9""day", &c_Handler },
  { "\xa9""dir", &c_Handler },
  { "\xa9""ed1", &c_Handler },
  { "\xa9""ed2", &c_Handler },
  { "\xa9""ed3", &c_Handler },
  { "\xa9""ed4", &c_Handler },
  { "\xa9""ed5", &c_Handler },
  { "\xa9""ed6", &c_Handler },
  { "\xa9""ed7", &c_Handler },
  { "\xa9""ed8", &c_Handler },
  { "\xa9""ed9", &c_Handler },
  { "\xa9""fmt", &c_Handler },
  { "\xa9""inf", &c_Handler },
  { "\xa9""prd", &c_Handler },
  { "\xa9""prf", &c_Handler },
  { "\xa9""req", &c_Handler },
  { "\xa9""src", &c_Handler },
  { "\xa9""wrt", &c_Handler },
  { "\xa9""aut", &c_Handler },
  { "\xa9""hst", &c_Handler },
  { "\xa9""wrt", &c_Handler },
  { "\xa9""cmt", &c_Handler },
  { "\xa9""mak", &c_Handler },
  { "\xa9""mod", &c_Handler },
  { "\xa9""nam", &c_Handler },
  { "\xa9""des", &c_Handler },
  { "\xa9""PRD", &c_Handler },
  { "\xa9""wrn", &c_Handler },
  { "\xa9""chp", &c_Handler },
  /*  { "name", &nameHandler }, */
  { NULL, NULL },
};

/**
 * Call the handler for the atom at the given position.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int handleAtom(const char * input,
		      size_t size,
		      size_t pos,
		      struct EXTRACTOR_Keywords ** list) {
  int i;
  if (0 == checkAtomValid(input, size, pos)) {
    return 0;
  }
  i = 0;
  while ( (handlers[i].name != NULL) &&
	  (0 != memcmp(&input[pos + 4], handlers[i].name, 4)) )
    i++;
  if (handlers[i].name == NULL) {
#if DEBUG
    char b[5];
    memcpy(b, &input[pos+4], 4);
    b[4] = '\0';
    printf("No handler for `%s'\n",
	   b);
#endif
    return -1;
  }
  i = handlers[i].handler(input, size, pos, list);  
#if DEBUG
  printf("Running handler for `%4s' at %u completed with result %d\n",
	 &input[pos + 4],
	 pos, 
	 i);
#endif
  return i;
}

/* mimetypes:
   video/quicktime: mov,qt: Quicktime animation;
   video/x-quicktime: mov,qt: Quicktime animation;
   application/x-quicktimeplayer: qtl: Quicktime list;
 */
struct EXTRACTOR_Keywords * 
libextractor_qt_extract(const char * filename,
			const char * data,
			size_t size,
			struct EXTRACTOR_Keywords * prev) {
  processAllAtoms(data,
		  size,
		  &prev);
  return prev;
}

/*  end of qtextractor.c */
