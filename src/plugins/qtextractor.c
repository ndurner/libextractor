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
/*
  This code is based on Bitzi's bitcollider.  Original
  Copyright as follows:

      (PD) 2004 The Bitzi Corporation

1. This work and others bearing the above label were
created by, or on behalf of, the Bitzi Corporation.

2. The Bitzi Corporation places these works into the
public domain, disclaiming all rights granted us by
copyright law.

You are completely free to copy, use, redistribute
and modify this work, though you should be aware of
points (3) and (4), below.

3. The Bitzi Corporation reserves all rights with
regard to any of its trademarks which may appear
herein, such as "Bitzi" or "Bitcollider". Please take
care that your uses of this work do not infringe on
our trademarks or imply our endorsement, for example
be sure to change labels and identifying strings in
your derivative works.

4. THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Please see http://bitzi.com/publicdomain or write
info@bitzi.com for more info.
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

/*
 * We implement our own rounding function, because the availability of
 * C99's round(), nearbyint(), rint(), etc. seems to be spotty, whereas
 * floor() is available in math.h on all C compilers.
 */
static double round_double(double num) {
  return floor(num + 0.5);
}

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
  if (getAtomSize(&input[pos]) != sizeof(MovieHeaderAtom))
    return 0;
  m = (const MovieHeaderAtom* ) &input[pos];
  /* TODO: extract metadata */
#if DEBUG
  printf("mvhdHandler not implemented\n");
#endif
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
  unsigned char matrix[36];
  /* in pixels */
  unsigned int track_width;
  /* in pixels */
  unsigned int track_height;
} TrackAtom;

static int tkhdHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  const TrackAtom * m;
  if (getAtomSize(&input[pos]) < sizeof(TrackAtom))
    return 0;
  m = (const TrackAtom* ) &input[pos];
  /* TODO: extract metadata */
#if DEBUG
  printf("tkhdHandler not implemented\n");
#endif
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
  unsigned int flags;
  unsigned int componentType;
  unsigned int componentSubType;
  unsigned int componentManufacturer;
  unsigned int componentFlags;
  unsigned int componentFlagsMask;
} PublicHandler;


static int hdlrHandler(const char * input,
		       size_t size,
		       size_t pos,
		       struct EXTRACTOR_Keywords ** list) {
  const PublicHandler * m;
  if (getAtomSize(&input[pos]) < sizeof(PublicHandler))
    return 0;
  m = (const PublicHandler* ) &input[pos];
  /* TODO: extract metadata */
#if DEBUG
  printf("hdlrHandler not implemented\n");
#endif
  return 1;
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

static int c_cpyHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_COPYRIGHT,
			list);
}

static int c_swrHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_SOFTWARE,
			list);
}

static int c_dayHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_CREATION_DATE,
			list);
}

static int c_dirHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_MOVIE_DIRECTOR,
			list);
}

static int c_fmtHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_FORMAT,
			list);
}

static int c_infHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_COMMENT,
			list);
}

static int c_prdHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_PRODUCER,
			list);
}

static int c_prfHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_ARTIST,
			list);
}

static int c_reqHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_CREATED_FOR, /* hardware requirements */
			list);
}

static int c_srcHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_CONTRIBUTOR,
			list);
}

static int c_edXHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_DESCRIPTION,
			list);
}

static int c_wrtHandler(const char * input,
			size_t size,
			size_t pos,
			struct EXTRACTOR_Keywords ** list) {
  return processTextTag(input,
			size,
			pos,
			EXTRACTOR_AUTHOR,
			list);
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
  { "hdlr", &hdlrHandler },
  { "ftyp", &ftypHandler },
  { "\xa9""swr", &c_swrHandler },
  { "\xa9""cpy", &c_cpyHandler },
  { "\xa9""day", &c_dayHandler },
  { "\xa9""dir", &c_dirHandler },
  { "\xa9""ed1", &c_edXHandler },
  { "\xa9""ed2", &c_edXHandler },
  { "\xa9""ed3", &c_edXHandler },
  { "\xa9""ed4", &c_edXHandler },
  { "\xa9""ed5", &c_edXHandler },
  { "\xa9""ed6", &c_edXHandler },
  { "\xa9""ed7", &c_edXHandler },
  { "\xa9""ed8", &c_edXHandler },
  { "\xa9""ed9", &c_edXHandler },
  { "\xa9""fmt", &c_fmtHandler },
  { "\xa9""inf", &c_infHandler },
  { "\xa9""prd", &c_prdHandler },
  { "\xa9""prf", &c_prfHandler },
  { "\xa9""req", &c_reqHandler },
  { "\xa9""src", &c_srcHandler },
  { "\xa9""wrt", &c_wrtHandler },
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








#if 0
/* Wrap the metadata we're collecting into a struct for easy passing */
typedef struct {
  unsigned int width;			/* width in pixels */
  unsigned int height;			/* height in pixels */
  unsigned int fps;			/* frames per second */
  unsigned int duration;		/* duration in milliseconds */
  unsigned int bitrate;		/* bitrate in kbps */
  const char * codec;			/* video compression codec */
} Data;



/* QuickTime uses big-endian ordering, and block ("atom") lengths include the
 * entire atom, including the fourcc specifying atom type and the length
 * integer itself.
 */
static int parse_quicktime(const char * input,
			   size_t size,
			   Data * data) {
  char fourcc[5];
  unsigned blockLen;
  unsigned subBlockLen;
  unsigned subSubBlockLen;
  unsigned timescale;
  long blockStart;
  long subBlockStart;
  long subSubBlockStart;
  size_t pos;
  
  fseek(file, 4L, SEEK_SET);
  fread(fourcc, sizeof(char), 4, file);
  /* If data is first, header's at end of file, so skip to it */
  if (memcmp(fourcc, "mdat", 4)==0) {
    fseek(file, 0L, SEEK_SET);
    blockLen = fread_be(file, 4);
    fseek(file, (long) (blockLen + 4), SEEK_SET);
    fread(fourcc, sizeof(char), 4, file);
  }
  
  if (memcmp(fourcc, "moov", 4)!=0)
    return 1;
  blockStart = ftell(file);
  blockLen = fread_be(file, 4);	/* mvhd length */
  fread(fourcc, sizeof(char), 4, file);
  if (memcmp(fourcc, "mvhd", 4)!=0)
    return 1;
  
  /* Now we're at the start of the movie header */
  
  /* 20: time scale (time units per second) (4 bytes) */
  fseek(file, blockStart + 20, SEEK_SET);
  timescale = fread_be(file, 4);
  
  /* 24: duration in time units (4 bytes) */
  data->duration = (unsigned int) round_double((double) fread_be(file, 4)
					       / timescale * 1000);
  
  /* Skip the rest of the mvhd */
  fseek(file, blockStart + blockLen, SEEK_SET);
  
  /* Find and parse trak atoms */
  while (!feof(file)) {
    unsigned int width;
    unsigned int height;
    
    /* Find the next trak atom */
    blockStart = ftell(file);
    blockLen = fread_be(file, 4);	/* trak (or other atom) length */
    fread(fourcc, sizeof(char), 4, file);
    if(memcmp(fourcc, "trak", 4)!=0)	/* If it's not a trak atom, skip it */
      {
	if(!feof(file))
	  fseek(file, blockStart + blockLen, SEEK_SET);
	continue;
      }
    
    subBlockStart = ftell(file);
    subBlockLen = fread_be(file, 4);	/* tkhd length */
    fread(fourcc, sizeof(char), 4, file);
    if(memcmp(fourcc, "tkhd", 4)!=0)
      return 1;
    
    /* Now in the track header */
    
    /* 84: width (2 bytes) */
    fseek(file, subBlockStart + 84, SEEK_SET);
    width = fread_be(file, 2);
    
    /* 88: height (2 bytes) */
    fseek(file, subBlockStart + 88, SEEK_SET);
    height = fread_be(file, 2);
    
    /* Note on above: Apple's docs say that width/height are 4-byte integers,
     * but all files I've seen have the data stored in the high-order two
     * bytes, with the low-order two being 0x0000.  Interpreting it the
     * "official" way would make width/height be thousands of pixels each.
     */
    
    /* Skip rest of tkhd */
    fseek(file, subBlockStart + subBlockLen, SEEK_SET);
    
    /* Find mdia atom for this trak */
    subBlockStart = ftell(file);
    subBlockLen = fread_be(file, 4);
    fread(fourcc, sizeof(char), 4, file);
    while(memcmp(fourcc, "mdia", 4)!=0) {
      fseek(file, subBlockStart + subBlockLen, SEEK_SET);
      subBlockStart = ftell(file);
      subBlockLen = fread_be(file, 4);
      fread(fourcc, sizeof(char), 4, file);
    }
    
    /* Now we're in the mdia atom; first sub-atom should be mdhd */
    subSubBlockStart = ftell(file);
    subSubBlockLen = fread_be(file, 4);
    fread(fourcc, sizeof(char), 4, file);
    if(memcmp(fourcc, "mdhd", 4)!=0)
      return 1;
    /* TODO: extract language from the mdhd?  For now skip to hdlr. */
    fseek(file, subSubBlockStart + subSubBlockLen, SEEK_SET);
    subSubBlockStart = ftell(file);
    subSubBlockLen = fread_be(file, 4);
    fread(fourcc, sizeof(char), 4, file);
    if(memcmp(fourcc, "hdlr", 4)!=0)
      return 1;
    /* 12: Component type: "mhlr" or "dhlr"; we only care about mhlr,
     * which should (?) appear first */
    fseek(file, subSubBlockStart + 12, SEEK_SET);
    fread(fourcc, sizeof(char), 4, file);
    if(memcmp(fourcc, "mhlr", 4)!=0)
      return 1;
    fread(fourcc, sizeof(char), 4, file);
    if(memcmp(fourcc, "vide", 4)==0)	/* This is a video trak */
      {
	data->height = height;
	data->width = width;
      }
    
    /* Skip rest of the trak */
    fseek(file, blockStart + blockLen, SEEK_SET);
  }
  return 0;
}
#endif

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
