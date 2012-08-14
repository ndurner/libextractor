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

/* verbatim from mp3extractor */
static const char *const genre_names[] = {
  gettext_noop ("Blues"),
  gettext_noop ("Classic Rock"),
  gettext_noop ("Country"),
  gettext_noop ("Dance"),
  gettext_noop ("Disco"),
  gettext_noop ("Funk"),
  gettext_noop ("Grunge"),
  gettext_noop ("Hip-Hop"),
  gettext_noop ("Jazz"),
  gettext_noop ("Metal"),
  gettext_noop ("New Age"),
  gettext_noop ("Oldies"),
  gettext_noop ("Other"),
  gettext_noop ("Pop"),
  gettext_noop ("R&B"),
  gettext_noop ("Rap"),
  gettext_noop ("Reggae"),
  gettext_noop ("Rock"),
  gettext_noop ("Techno"),
  gettext_noop ("Industrial"),
  gettext_noop ("Alternative"),
  gettext_noop ("Ska"),
  gettext_noop ("Death Metal"),
  gettext_noop ("Pranks"),
  gettext_noop ("Soundtrack"),
  gettext_noop ("Euro-Techno"),
  gettext_noop ("Ambient"),
  gettext_noop ("Trip-Hop"),
  gettext_noop ("Vocal"),
  gettext_noop ("Jazz+Funk"),
  gettext_noop ("Fusion"),
  gettext_noop ("Trance"),
  gettext_noop ("Classical"),
  gettext_noop ("Instrumental"),
  gettext_noop ("Acid"),
  gettext_noop ("House"),
  gettext_noop ("Game"),
  gettext_noop ("Sound Clip"),
  gettext_noop ("Gospel"),
  gettext_noop ("Noise"),
  gettext_noop ("Alt. Rock"),
  gettext_noop ("Bass"),
  gettext_noop ("Soul"),
  gettext_noop ("Punk"),
  gettext_noop ("Space"),
  gettext_noop ("Meditative"),
  gettext_noop ("Instrumental Pop"),
  gettext_noop ("Instrumental Rock"),
  gettext_noop ("Ethnic"),
  gettext_noop ("Gothic"),
  gettext_noop ("Darkwave"),
  gettext_noop ("Techno-Industrial"),
  gettext_noop ("Electronic"),
  gettext_noop ("Pop-Folk"),
  gettext_noop ("Eurodance"),
  gettext_noop ("Dream"),
  gettext_noop ("Southern Rock"),
  gettext_noop ("Comedy"),
  gettext_noop ("Cult"),
  gettext_noop ("Gangsta Rap"),
  gettext_noop ("Top 40"),
  gettext_noop ("Christian Rap"),
  gettext_noop ("Pop/Funk"),
  gettext_noop ("Jungle"),
  gettext_noop ("Native American"),
  gettext_noop ("Cabaret"),
  gettext_noop ("New Wave"),
  gettext_noop ("Psychedelic"),
  gettext_noop ("Rave"),
  gettext_noop ("Showtunes"),
  gettext_noop ("Trailer"),
  gettext_noop ("Lo-Fi"),
  gettext_noop ("Tribal"),
  gettext_noop ("Acid Punk"),
  gettext_noop ("Acid Jazz"),
  gettext_noop ("Polka"),
  gettext_noop ("Retro"),
  gettext_noop ("Musical"),
  gettext_noop ("Rock & Roll"),
  gettext_noop ("Hard Rock"),
  gettext_noop ("Folk"),
  gettext_noop ("Folk/Rock"),
  gettext_noop ("National Folk"),
  gettext_noop ("Swing"),
  gettext_noop ("Fast-Fusion"),
  gettext_noop ("Bebob"),
  gettext_noop ("Latin"),
  gettext_noop ("Revival"),
  gettext_noop ("Celtic"),
  gettext_noop ("Bluegrass"),
  gettext_noop ("Avantgarde"),
  gettext_noop ("Gothic Rock"),
  gettext_noop ("Progressive Rock"),
  gettext_noop ("Psychedelic Rock"),
  gettext_noop ("Symphonic Rock"),
  gettext_noop ("Slow Rock"),
  gettext_noop ("Big Band"),
  gettext_noop ("Chorus"),
  gettext_noop ("Easy Listening"),
  gettext_noop ("Acoustic"),
  gettext_noop ("Humour"),
  gettext_noop ("Speech"),
  gettext_noop ("Chanson"),
  gettext_noop ("Opera"),
  gettext_noop ("Chamber Music"),
  gettext_noop ("Sonata"),
  gettext_noop ("Symphony"),
  gettext_noop ("Booty Bass"),
  gettext_noop ("Primus"),
  gettext_noop ("Porn Groove"),
  gettext_noop ("Satire"),
  gettext_noop ("Slow Jam"),
  gettext_noop ("Club"),
  gettext_noop ("Tango"),
  gettext_noop ("Samba"),
  gettext_noop ("Folklore"),
  gettext_noop ("Ballad"),
  gettext_noop ("Power Ballad"),
  gettext_noop ("Rhythmic Soul"),
  gettext_noop ("Freestyle"),
  gettext_noop ("Duet"),
  gettext_noop ("Punk Rock"),
  gettext_noop ("Drum Solo"),
  gettext_noop ("A Cappella"),
  gettext_noop ("Euro-House"),
  gettext_noop ("Dance Hall"),
  gettext_noop ("Goa"),
  gettext_noop ("Drum & Bass"),
  gettext_noop ("Club-House"),
  gettext_noop ("Hardcore"),
  gettext_noop ("Terror"),
  gettext_noop ("Indie"),
  gettext_noop ("BritPop"),
  gettext_noop ("Negerpunk"),
  gettext_noop ("Polsk Punk"),
  gettext_noop ("Beat"),
  gettext_noop ("Christian Gangsta Rap"),
  gettext_noop ("Heavy Metal"),
  gettext_noop ("Black Metal"),
  gettext_noop ("Crossover"),
  gettext_noop ("Contemporary Christian"),
  gettext_noop ("Christian Rock"),
  gettext_noop ("Merengue"),
  gettext_noop ("Salsa"),
  gettext_noop ("Thrash Metal"),
  gettext_noop ("Anime"),
  gettext_noop ("JPop"),
  gettext_noop ("Synthpop"),
};

#define GENRE_NAME_COUNT \
    ((unsigned int)(sizeof genre_names / sizeof (const char *const)))


static const char *languages[] = {
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


typedef struct
{
  const char *ext;
  const char *mime;
} C2M;

/* see http://www.mp4ra.org/filetype.html 
 *     http://www.ftyps.com/ */
static C2M ftMap[] = {
  {"qt  ", "video/quicktime"},
  {"isom", "video/mp4"},        /* ISO Base Media files */
  {"iso2", "video/mp4"},
  {"mp41", "video/mp4"},        /* MPEG-4 (ISO/IEC 14491-1) version 1 */
  {"mp42", "video/mp4"},        /* MPEG-4 (ISO/IEC 14491-1) version 2 */
  {"3gp1", "video/3gpp"},
  {"3gp2", "video/3gpp"},
  {"3gp3", "video/3gpp"},
  {"3gp4", "video/3gpp"},
  {"3gp5", "video/3gpp"},
  {"3g2a", "video/3gpp2"},
  {"mmp4", "video/mp4"},        /* Mobile MPEG-4 */
  {"M4A ", "audio/mp4"},
  {"M4B ", "audio/mp4"},
  {"M4P ", "audio/mp4"},
  {"M4V ", "video/mp4"},
  {"mj2s", "video/mj2"},        /* Motion JPEG 2000 */
  {"mjp2", "video/mj2"},
  {NULL, NULL},
};

typedef struct CHE
{
  const char *pfx;
  enum EXTRACTOR_MetaType type;
} CHE;

static CHE cHm[] = {
  {"aut", EXTRACTOR_METATYPE_AUTHOR_NAME},
  {"cpy", EXTRACTOR_METATYPE_COPYRIGHT},
  {"day", EXTRACTOR_METATYPE_CREATION_DATE},
  {"ed1", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed2", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed3", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed4", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed5", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed6", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed7", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed8", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"ed9", EXTRACTOR_METATYPE_MODIFICATION_DATE},
  {"cmt", EXTRACTOR_METATYPE_COMMENT},
  {"url", EXTRACTOR_METATYPE_URL},
  {"enc", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE},
  {"hst", EXTRACTOR_METATYPE_BUILDHOST},
  {"nam", EXTRACTOR_METATYPE_TITLE},
  {"gen", EXTRACTOR_METATYPE_GENRE},
  {"mak", EXTRACTOR_METATYPE_CAMERA_MAKE},
  {"mod", EXTRACTOR_METATYPE_CAMERA_MODEL},
  {"des", EXTRACTOR_METATYPE_DESCRIPTION},
  {"dis", EXTRACTOR_METATYPE_DISCLAIMER},
  {"dir", EXTRACTOR_METATYPE_MOVIE_DIRECTOR},
  {"src", EXTRACTOR_METATYPE_CONTRIBUTOR_NAME},
  {"prf", EXTRACTOR_METATYPE_PERFORMER },
  {"prd", EXTRACTOR_METATYPE_PRODUCER},
  {"PRD", EXTRACTOR_METATYPE_PRODUCT_VERSION}, 
  {"swr", EXTRACTOR_METATYPE_PRODUCED_BY_SOFTWARE},
  {"isr", EXTRACTOR_METATYPE_ISRC},
  {"wrt", EXTRACTOR_METATYPE_WRITER},
  {"wrn", EXTRACTOR_METATYPE_WARNING},
  {"chp", EXTRACTOR_METATYPE_CHAPTER_NAME},
  {"inf", EXTRACTOR_METATYPE_DESCRIPTION},
  {"req", EXTRACTOR_METATYPE_TARGET_PLATFORM},      /* hardware requirements */
  {"fmt", EXTRACTOR_METATYPE_FORMAT},
  {NULL, EXTRACTOR_METATYPE_RESERVED },
};


typedef struct
{
  const char *atom_type;
  enum EXTRACTOR_MetaType type;
} ITTagConversionEntry;

/* iTunes Tags:
 * see http://atomicparsley.sourceforge.net/mpeg-4files.html */
static ITTagConversionEntry it_to_extr_table[] = {
  {"\xa9" "alb", EXTRACTOR_METATYPE_ALBUM},
  {"\xa9" "ART", EXTRACTOR_METATYPE_ARTIST},
  {"aART", EXTRACTOR_METATYPE_ARTIST},
  {"\xa9" "cmt", EXTRACTOR_METATYPE_COMMENT},
  {"\xa9" "day", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  {"\xa9" "nam", EXTRACTOR_METATYPE_TITLE},
  {"trkn", EXTRACTOR_METATYPE_TRACK_NUMBER},
  {"disk", EXTRACTOR_METATYPE_DISC_NUMBER},
  {"\xa9" "gen", EXTRACTOR_METATYPE_GENRE},
  {"gnre", EXTRACTOR_METATYPE_GENRE},
  {"\xa9" "wrt", EXTRACTOR_METATYPE_WRITER},
  {"\xa9" "too", EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE},
  {"cprt", EXTRACTOR_METATYPE_COPYRIGHT},
  {"\xa9" "grp", EXTRACTOR_METATYPE_GROUP},
  {"catg", EXTRACTOR_METATYPE_SECTION},
  {"keyw", EXTRACTOR_METATYPE_KEYWORDS},
  {"desc", EXTRACTOR_METATYPE_DESCRIPTION},
  {"tvnn", EXTRACTOR_METATYPE_NETWORK_NAME},
  {"tvsh", EXTRACTOR_METATYPE_SHOW_NAME}, 
  {"tven", EXTRACTOR_METATYPE_NETWORK_NAME},
  {NULL, EXTRACTOR_METATYPE_RESERVED}
};


typedef struct
{
  unsigned int size;
  unsigned int type;
} Atom;

typedef struct
{
  unsigned int one;
  unsigned int type;
  unsigned long long size;
} LongAtom;

static unsigned long long
ntohll (unsigned long long n)
{
#if __BYTE_ORDER == __BIG_ENDIAN
  return n;
#else
  return (((unsigned long long) ntohl (n)) << 32) + ntohl (n >> 32);
#endif
}


/**
 * Check if at position pos there is a valid atom.
 * @return 0 if the atom is invalid, 1 if it is valid
 */
static int
checkAtomValid (const char *buffer, size_t size, size_t pos)
{
  unsigned long long atomSize;
  const Atom *atom;
  const LongAtom *latom;
  if ((pos >= size) ||
      (pos + sizeof (Atom) > size) || (pos + sizeof (Atom) < pos))
    return 0;
  atom = (const Atom *) &buffer[pos];
  if (ntohl (atom->size) == 1)
    {
      if ((pos + sizeof (LongAtom) > size) || (pos + sizeof (LongAtom) < pos))
        return 0;
      latom = (const LongAtom *) &buffer[pos];
      atomSize = ntohll (latom->size);
      if ((atomSize < sizeof (LongAtom)) ||
          (atomSize + pos > size) || (atomSize + pos < atomSize))
        return 0;
    }
  else
    {
      atomSize = ntohl (atom->size);
      if ((atomSize < sizeof (Atom)) ||
          (atomSize + pos > size) || (atomSize + pos < atomSize))
        return 0;
    }
  return 1;
}

/**
 * Assumes that checkAtomValid has already been called.
 */
static unsigned long long
getAtomSize (const char *buf)
{
  const Atom *atom;
  const LongAtom *latom;
  atom = (const Atom *) buf;
  if (ntohl (atom->size) == 1)
    {
      latom = (const LongAtom *) buf;
      return ntohll (latom->size);
    }
  return ntohl (atom->size);
}

/**
 * Assumes that checkAtomValid has already been called.
 */
static unsigned int
getAtomHeaderSize (const char *buf)
{
  const Atom *atom;

  atom = (const Atom *) buf;
  if (ntohl (atom->size) == 1)
    return sizeof (const LongAtom);
  return sizeof (Atom);
}

struct ExtractContext
{
  EXTRACTOR_MetaDataProcessor proc;
  void *proc_cls;
  int ret;
};

static void
addKeyword (enum EXTRACTOR_MetaType type,
	    const char *str,
	    struct ExtractContext *ec)
{
  if (ec->ret != 0)
    return;
  ec->ret = ec->proc (ec->proc_cls,
		      "qt",
		      type,
		      EXTRACTOR_METAFORMAT_UTF8,
		      "text/plain",
		      str,
		      strlen(str)+1);
}



/**
 * Assumes that checkAtomValid has already been called.
 */
typedef int (*AtomHandler) (const char *input,
                            size_t size,
                            size_t pos, struct ExtractContext *ec);

typedef struct
{
  char *name;
  AtomHandler handler;
} HandlerEntry;

/**
 * Call the handler for the atom at the given position.
 * Will check validity of the given atom.
 *
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int handleAtom (HandlerEntry *handlers,
                       const char *input,
                       size_t size,
                       size_t pos, 
		       struct ExtractContext *ec);

static HandlerEntry all_handlers[];
static HandlerEntry ilst_handlers[];

/**
 * Process atoms.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int
processAtoms (HandlerEntry *handlers, const char *input,
                 size_t size, struct ExtractContext *ec)
{
  size_t pos;

  if (size < sizeof (Atom))
    return 1;
  pos = 0;
  while (pos < size - sizeof (Atom))
    {
      if (0 == handleAtom (handlers, input, size, pos, ec))
        return 0;
      pos += getAtomSize (&input[pos]);
    }
  return 1;
}

/**
 * Process all atoms.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int
processAllAtoms (const char *input,
                 size_t size, struct ExtractContext *ec)
{
  return processAtoms(all_handlers, input, size, ec);
}

/**
 * Handle the moov atom.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int
moovHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  unsigned int hdr = getAtomHeaderSize (&input[pos]);
  return processAllAtoms (&input[pos + hdr],
                          getAtomSize (&input[pos]) - hdr, ec);
}

/* see http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap1/chapter_2_section_5.html */
typedef struct
{
  Atom header;
  /* major brand */
  char type[4];
  /* minor version */
  unsigned int version;
  /* compatible brands */
  char compatibility[4];
} FileType;

static int
ftypHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  const FileType *ft;
  int i;

  if (getAtomSize (&input[pos]) < sizeof (FileType)) {
    return 0;
  }
  ft = (const FileType *) &input[pos];

  i = 0;
  while ((ftMap[i].ext != NULL) && (0 != memcmp (ft->type, ftMap[i].ext, 4)))
    i++;
  if (ftMap[i].ext != NULL)
    addKeyword (EXTRACTOR_METATYPE_MIMETYPE, ftMap[i].mime, ec);
  return 1;
}

typedef struct
{
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

static int
mvhdHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  const MovieHeaderAtom *m;
  char duration[16];
  if (getAtomSize (&input[pos]) != sizeof (MovieHeaderAtom))
    return 0;
  m = (const MovieHeaderAtom *) &input[pos];
  snprintf (duration,
	    sizeof(duration),
	    "%us",
	    ntohl (m->duration) / ntohl (m->timeScale));
  addKeyword (EXTRACTOR_METATYPE_DURATION, duration, ec);
  return 1;
}

typedef struct
{
  Atom cmovAtom;
  Atom dcomAtom;
  char compressor[4];
  Atom cmvdAtom;
  unsigned int decompressedSize;
} CompressedMovieHeaderAtom;

static int
cmovHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  const CompressedMovieHeaderAtom *c;
  unsigned int s;
  char *buf;
  int ret;
  z_stream z_state;
  int z_ret_code;


  if (getAtomSize (&input[pos]) < sizeof (CompressedMovieHeaderAtom))
    return 0;
  c = (const CompressedMovieHeaderAtom *) &input[pos];
  if ((ntohl (c->dcomAtom.size) != 12) ||
      (0 != memcmp (&c->dcomAtom.type, "dcom", 4)) ||
      (0 != memcmp (c->compressor, "zlib", 4)) ||
      (0 != memcmp (&c->cmvdAtom.type, "cmvd", 4)) ||
      (ntohl (c->cmvdAtom.size) !=
       getAtomSize (&input[pos]) - sizeof (Atom) * 2 - 4))
    {
      return 0;                 /* dcom must be 12 bytes */
    }
  s = ntohl (c->decompressedSize);
  if (s > 16 * 1024 * 1024)
    return 1;                   /* ignore, too big! */
  buf = malloc (s);
  if (buf == NULL)
    return 1;                   /* out of memory, handle gracefully */

  z_state.next_in = (unsigned char *) &c[1];
  z_state.avail_in = ntohl (c->cmvdAtom.size);
  z_state.avail_out = s;
  z_state.next_out = (unsigned char *) buf;
  z_state.zalloc = (alloc_func) 0;
  z_state.zfree = (free_func) 0;
  z_state.opaque = (voidpf) 0;
  z_ret_code = inflateInit (&z_state);
  if (Z_OK != z_ret_code)
    {
      free (buf);
      return 0;                 /* crc error? */
    }
  z_ret_code = inflate (&z_state, Z_NO_FLUSH);
  if ((z_ret_code != Z_OK) && (z_ret_code != Z_STREAM_END))
    {
      free (buf);
      return 0;                 /* decode error? */
    }
  z_ret_code = inflateEnd (&z_state);
  if (Z_OK != z_ret_code)
    {
      free (buf);
      return 0;                 /* decode error? */
    }
  ret = handleAtom (all_handlers, buf, s, 0, ec);
  free (buf);
  return ret;
}

typedef struct
{
  short integer;
  short fraction;
} QTFixed;

typedef struct
{
  Atom hdr;
  unsigned int flags;           /* 1 byte of version, 3 bytes of flags */
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
  QTFixed matrix[3][3];
  /* in pixels */
  QTFixed track_width;
  /* in pixels */
  QTFixed track_height;
} TrackAtom;

static int
tkhdHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  const TrackAtom *m;
  char dimensions[40];

  if (getAtomSize (&input[pos]) < sizeof (TrackAtom))
    return 0;
  m = (const TrackAtom *) &input[pos];
  if (ntohs (m->track_width.integer) != 0)
    {
      /* if actually a/the video track */
      snprintf (dimensions,
                sizeof(dimensions),
                "%dx%d",
                ntohs (m->track_width.integer),
                ntohs (m->track_height.integer));
      addKeyword (EXTRACTOR_METATYPE_IMAGE_DIMENSIONS, dimensions, ec);
    }
  return 1;
}

static int
trakHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  unsigned int hdr = getAtomHeaderSize (&input[pos]);
  return processAllAtoms (&input[pos + hdr],
                          getAtomSize (&input[pos]) - hdr, ec);
}

static int
metaHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  unsigned int hdr = getAtomHeaderSize (&input[pos]);
  if (getAtomSize (&input[pos]) < hdr + 4)
    return 0;
  return processAllAtoms (&input[pos + hdr + 4],
                          getAtomSize (&input[pos]) - hdr - 4, ec);
}

typedef struct
{
  Atom header;
  unsigned short length;
  unsigned short language;
} InternationalText;

/*
 * see http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter
_3_section_2.html
 *   "User Data Text Strings and Language Codes"
 * TODO: make conformant
 */
static int
processTextTag (const char *input,
                size_t size,
                size_t pos,
                enum EXTRACTOR_MetaType type, struct ExtractContext *ec)
{
  unsigned long long as;
  unsigned short len;
  unsigned short lang;
  const InternationalText *txt;
  char *meta;
  int i;

  /* contains "international text":
     16-bit size + 16 bit language code */
  as = getAtomSize (&input[pos]);
  if (as < sizeof (InternationalText))
    return 0;                   /* invalid */
  txt = (const InternationalText *) &input[pos];
  len = ntohs (txt->length);
  if (len + sizeof (InternationalText) > as)
    return 0;                   /* invalid */
  lang = ntohs (txt->language);
  if (lang >= sizeof (languages) / sizeof (char *))
    return 0;                   /* invalid */
  addKeyword (EXTRACTOR_METATYPE_LANGUAGE, languages[lang], ec);

  meta = malloc (len + 1);
  if (meta == NULL)
    return 0;
  memcpy (meta, &txt[1], len);
  meta[len] = '\0';
  for (i = 0; i < len; i++)
    if (meta[i] == '\r')
      meta[i] = '\n';
  addKeyword (type, meta, ec);
  free (meta);
  return 1;
}


static int
c_Handler (const char *input,
           size_t size, size_t pos, struct ExtractContext *ec)
{
  int i;

  i = 0;
  while ((cHm[i].pfx != NULL) && (0 != memcmp (&input[pos+5], cHm[i].pfx, 3)))
    i++;
  if (cHm[i].pfx != NULL)
    return processTextTag (input, size, pos, cHm[i].type, ec);
  return -1;                    /* not found */
}

static int
udtaHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  unsigned int hdr = getAtomHeaderSize (&input[pos]);
  return processAllAtoms (&input[pos + hdr],
                          getAtomSize (&input[pos]) - hdr, ec);
}

static int
processDataAtom (const char *input,
		 size_t size, /* parent atom size */
		 size_t pos,
		 const char *patom,
		 enum EXTRACTOR_MetaType type,
		 struct ExtractContext *ec)
{
  char *meta;
  unsigned char version;
  unsigned int flags;
  unsigned long long asize;
  unsigned int len;
  unsigned int hdr;
  int i;

  hdr = getAtomHeaderSize (&input[pos]);
  asize = getAtomSize (&input[pos]);
  if (memcmp(&input[pos+4], "data", 4) != 0)
    return -1;

  if (asize < hdr + 8 || /* header + u32 flags + u32 reserved */
      asize > (getAtomSize(&patom[0]) - 8))
    return 0;

  len = (unsigned int)(asize - (hdr + 8));

  version = input[pos+8];
  flags = ((unsigned char)input[pos+9]<<16) |
          ((unsigned char)input[pos+10]<<8) | 
          (unsigned char)input[pos+11];
#if DEBUG
  printf("[data] version:%02x flags:%08x txtlen:%d\n", version, flags, len);
#endif

  if (version != 0)
    return -1;

  if (flags == 0x0) { /* binary data */
    if (memcmp(&patom[4], "gnre", 4) == 0) {
      if (len >= 2) {
        unsigned short genre = ((unsigned char)input[pos+16] << 8) |
                                (unsigned char)input[pos+17];
        if (genre > 0 && genre < GENRE_NAME_COUNT)
          addKeyword(type, genre_names[genre-1], ec);
      }
      return 1;
    }
    else if ((memcmp(&patom[4], "trkn", 4) == 0) || 
        (memcmp(&patom[4], "disk", 4) == 0)) {
      if (len >= 4) {
        unsigned short n = ((unsigned char)input[pos+18] << 8) |
                            (unsigned char)input[pos+19];
        char s[8];
	snprintf(s, 8, "%d", n);
        addKeyword(type, s, ec);
      }
    }
    else {
      return -1;
    }
  }
  else if (flags == 0x1) { /* text data */
    meta = malloc (len + 1);
    if (meta == NULL)
      return 0;
    memcpy (meta, &input[pos+16], len);
    meta[len] = '\0';
    for (i = 0; i < len; i++)
      if (meta[i] == '\r')
        meta[i] = '\n';
    addKeyword (type, meta, ec);
    free (meta);
    return 1;
  }

  return -1;
}

/* NOTE: iTunes tag processing should, in theory, be limited to iTunes
 * file types (from ftyp), but, in reality, it seems that there are other
 * files, like 3gpp, out in the wild with iTunes tags. */
static int
iTunesTagHandler (const char *input,
           size_t size, size_t pos, struct ExtractContext *ec)
{
  unsigned long long asize;
  unsigned int hdr;
  int i;

  hdr = getAtomHeaderSize (&input[pos]);
  asize = getAtomSize (&input[pos]);

  if (asize < hdr + 8) /* header + at least one atom */
    return 0;

  i = 0;
  while ((it_to_extr_table[i].atom_type != NULL) && 
         (0 != memcmp (&input[pos+4], it_to_extr_table[i].atom_type, 4)))
    i++;
  if (it_to_extr_table[i].atom_type != NULL)
    return processDataAtom(input, asize, pos+hdr, &input[pos],  
                           it_to_extr_table[i].type, ec);

  return -1;
}


static int 
ilstHandler (const char *input,
             size_t size, size_t pos, struct ExtractContext *ec)
{
  unsigned int hdr = getAtomHeaderSize (&input[pos]);
  return processAtoms(ilst_handlers, &input[pos + hdr],
                      getAtomSize(&input[pos]) - hdr, ec);
}


static HandlerEntry all_handlers[] = {
  {"moov", &moovHandler},
  {"cmov", &cmovHandler},
  {"mvhd", &mvhdHandler},
  {"trak", &trakHandler},
  {"tkhd", &tkhdHandler},
  {"ilst", &ilstHandler},
  {"meta", &metaHandler},
  {"udta", &udtaHandler},
  {"ftyp", &ftypHandler},
  {"\xa9" "swr", &c_Handler},
  {"\xa9" "cpy", &c_Handler},
  {"\xa9" "day", &c_Handler},
  {"\xa9" "dir", &c_Handler},
  {"\xa9" "ed1", &c_Handler},
  {"\xa9" "ed2", &c_Handler},
  {"\xa9" "ed3", &c_Handler},
  {"\xa9" "ed4", &c_Handler},
  {"\xa9" "ed5", &c_Handler},
  {"\xa9" "ed6", &c_Handler},
  {"\xa9" "ed7", &c_Handler},
  {"\xa9" "ed8", &c_Handler},
  {"\xa9" "ed9", &c_Handler},
  {"\xa9" "fmt", &c_Handler},
  {"\xa9" "inf", &c_Handler},
  {"\xa9" "prd", &c_Handler},
  {"\xa9" "prf", &c_Handler},
  {"\xa9" "req", &c_Handler},
  {"\xa9" "src", &c_Handler},
  {"\xa9" "wrt", &c_Handler},
  {"\xa9" "aut", &c_Handler},
  {"\xa9" "hst", &c_Handler},
  {"\xa9" "wrt", &c_Handler},
  {"\xa9" "cmt", &c_Handler},
  {"\xa9" "mak", &c_Handler},
  {"\xa9" "mod", &c_Handler},
  {"\xa9" "nam", &c_Handler},
  {"\xa9" "des", &c_Handler},
  {"\xa9" "PRD", &c_Handler},
  {"\xa9" "wrn", &c_Handler},
  {"\xa9" "chp", &c_Handler},
  /*  { "name", &nameHandler }, */
  {NULL, NULL},
};

static HandlerEntry ilst_handlers[] = {
  {"\xa9" "alb", &iTunesTagHandler},
  {"\xa9" "ART", &iTunesTagHandler},
  {"aART", &iTunesTagHandler},
  {"\xa9" "cmt", &iTunesTagHandler},
  {"\xa9" "day", &iTunesTagHandler},
  {"\xa9" "nam", &iTunesTagHandler},
  {"\xa9" "gen", &iTunesTagHandler},
  {"gnre", &iTunesTagHandler},
  {"trkn", &iTunesTagHandler},
  {"disk", &iTunesTagHandler},
  {"\xa9" "wrt", &iTunesTagHandler},
  {"\xa9" "too", &iTunesTagHandler},
  {"tmpo", &iTunesTagHandler},
  {"cprt", &iTunesTagHandler},
  {"cpil", &iTunesTagHandler},
  {"covr", &iTunesTagHandler},
  {"rtng", &iTunesTagHandler},
  {"\xa9" "grp", &iTunesTagHandler},
  {"stik", &iTunesTagHandler},
  {"pcst", &iTunesTagHandler},
  {"catg", &iTunesTagHandler},
  {"keyw", &iTunesTagHandler},
  {"purl", &iTunesTagHandler},
  {"egid", &iTunesTagHandler},
  {"desc", &iTunesTagHandler},
  {"\xa9" "lyr", &iTunesTagHandler},
  {"tvnn", &iTunesTagHandler},
  {"tvsh", &iTunesTagHandler},
  {"tven", &iTunesTagHandler},
  {"tvsn", &iTunesTagHandler},
  {"tves", &iTunesTagHandler},
  {"purd", &iTunesTagHandler},
  {"pgap", &iTunesTagHandler},
  {NULL, NULL},
};

/**
 * Call the handler for the atom at the given position.
 * @return 0 on error, 1 for success, -1 for unknown atom type
 */
static int
handleAtom (HandlerEntry *handlers, const char *input,
            size_t size, size_t pos, struct ExtractContext *ec)
{
  int i;
  if (0 == checkAtomValid (input, size, pos))
    {
      return 0;
    }
  i = 0;
  while ((handlers[i].name != NULL) &&
         (0 != memcmp (&input[pos + 4], handlers[i].name, 4)))
    i++;
  if (handlers[i].name == NULL)
    {
#if DEBUG
      char b[5];
      memcpy (b, &input[pos + 4], 4);
      b[4] = '\0';
      printf ("No handler for `%s'\n", b);
#endif
      return -1;
    }
  i = handlers[i].handler (input, size, pos, ec);
#if DEBUG
  printf ("Running handler for `%4s' at %u completed with result %d\n",
          &input[pos + 4], pos, i);
#endif
  return i;
}

/* mimetypes:
   video/quicktime: mov,qt: Quicktime animation;
   video/x-quicktime: mov,qt: Quicktime animation;
   application/x-quicktimeplayer: qtl: Quicktime list;
 */

int 
EXTRACTOR_qt_extract (const char *data,
		      size_t size,
		      EXTRACTOR_MetaDataProcessor proc,
		      void *proc_cls,
		      const char *options)
{
  struct ExtractContext ec;
  ec.proc = proc;
  ec.proc_cls = proc_cls;
  ec.ret = 0;
  processAllAtoms (data, size, &ec);
  return ec.ret;
}

/*  end of qt_extractor.c */
