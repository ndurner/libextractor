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

struct MetaTypeDescription
{
  const char *short_description;

  const char *long_description;
};


/**
 * The sources of keywords as strings.
 */
static const struct MetaTypeDescription meta_type_descriptions[] = {
  /* 0 */
  { gettext_noop ("reserved"),
    gettext_noop ("reserved value, do not use") },
  { gettext_noop ("mimetype"),
    gettext_noop ("mime type") },
  { gettext_noop ("embedded filename"),
    gettext_noop ("filename that was embedded (not necessarily the current filename)") },
  { gettext_noop ("comment"),
    gettext_noop ("comment about the content") },
  { gettext_noop ("title"),
    gettext_noop ("title of the work")},
  /* 5 */
  { gettext_noop ("book title"),
    gettext_noop ("title of the book containing the work") },
  { gettext_noop ("book edition"),
    gettext_noop ("edition of the book (or book containing the work)") },
  { gettext_noop ("book chapter"),
    gettext_noop ("chapter number") },
  { gettext_noop ("journal name"),
    gettext_noop ("journal or magazine the work was published in") },
  { gettext_noop ("journal volume"),    
    gettext_noop ("volume of a journal or multi-volume book") },
  /* 10 */
  { gettext_noop ("journal number"),    
    gettext_noop ("number of a journal, magazine or tech-report") },
  { gettext_noop ("page count"),
    gettext_noop ("total number of pages of the work") },
  { gettext_noop ("page range"),
    gettext_noop ("page numbers of the publication in the respective journal or book") },
  { gettext_noop ("author name"),
    gettext_noop ("name of the author(s)") },
  { gettext_noop ("author email"),
    gettext_noop ("e-mail of the author(s)") },
  /* 15 */
  { gettext_noop ("author institution"),
    gettext_noop ("institution the author worked for") },
  { gettext_noop ("publisher"),
    gettext_noop ("name of the publisher") },
  { gettext_noop ("publisher's address"),
    gettext_noop ("Address of the publisher (often only the city)") },
  { gettext_noop ("publishing institution"),
    gettext_noop ("institution that was involved in the publishing, but not necessarily the publisher") },
  { gettext_noop ("publication series"),
    gettext_noop ("series of books the book was published in") },
  /* 20 */
  { gettext_noop ("publication type"),
    gettext_noop ("type of the tech-report") },
  { gettext_noop ("publication year"),
    gettext_noop ("year of publication (or, if unpublished, the year of creation)") },
  { gettext_noop ("publication month"),
    gettext_noop ("month of publication (or, if unpublished, the month of creation)") },
  { gettext_noop ("publication day"),
    gettext_noop ("day of publication (or, if unpublished, the day of creation), relative to the given month") },
  { gettext_noop ("publication date"),
    gettext_noop ("date of publication (or, if unpublished, the date of creation)") },
  /* 25 */
  { gettext_noop ("bibtex eprint"),
    gettext_noop ("specification of an electronic publication") },
  { gettext_noop ("bibtex entry type"),
    gettext_noop ("type of the publication for bibTeX bibliographies") },
  { gettext_noop ("language"),
    gettext_noop ("language the work uses") },
  { gettext_noop ("creation time"),
    gettext_noop ("time and date of creation") },
  { gettext_noop ("URL"),
    gettext_noop ("universal resource location (where the work is made available)") },
  /* 30 */
  { gettext_noop ("URI"),
    gettext_noop ("universal resource identifier") },
  { gettext_noop ("international standard recording code"),
    gettext_noop ("ISRC number identifying the work") },
  { gettext_noop ("MD4"),
    gettext_noop ("MD4 hash") },
  { gettext_noop ("MD5"),
    gettext_noop ("MD5 hash") },
  { gettext_noop ("SHA-0"),
    gettext_noop ("SHA-0 hash") },
  /* 35 */
  { gettext_noop ("SHA-1"), 
    gettext_noop ("SHA-1 hash") },
  { gettext_noop ("RipeMD160"),
    gettext_noop ("RipeMD150 hash") },
  { gettext_noop ("GPS latitude ref"),
    gettext_noop ("GPS latitude ref") },
  { gettext_noop ("GPS latitude"),
    gettext_noop ("GPS latitude") },
  { gettext_noop ("GPS longitude ref"),
    gettext_noop ("GPS longitude ref") },
  /* 40 */
  { gettext_noop ("GPS longitude"),
    gettext_noop ("GPS longitude") },
  { gettext_noop ("city"),
    gettext_noop ("name of the city where the document originated") },
  { gettext_noop ("sublocation"), 
    gettext_noop ("more specific location of the geographic origin") },
  { gettext_noop ("country"),
    gettext_noop ("name of the country where the document originated") },
  { gettext_noop ("country code"),
    gettext_noop ("ISO 2-letter country code for the country of origin") },
  /* 45 */
  { gettext_noop ("unknown"),
    gettext_noop ("specifics are not known") },
  { gettext_noop ("description"),
    gettext_noop ("description") },
  { gettext_noop ("copyright"),
    gettext_noop ("Name of the entity holding the copyright") },
  { gettext_noop ("rights"),
    gettext_noop ("information about rights") },
  { gettext_noop ("keywords"),
    gettext_noop ("keywords") },
  /* 50 */
  { gettext_noop ("abstract"),
    gettext_noop ("abstract") },
  { gettext_noop ("summary"),
    gettext_noop ("summary") },
  { gettext_noop ("subject"),
    gettext_noop ("subject matter") },
  { gettext_noop ("creator"),
    gettext_noop ("name of the person who created the document") },
  { gettext_noop ("format"),
    gettext_noop ("name of the document format") },
  /* 55 */
  { gettext_noop ("format version"),
    gettext_noop ("version of the document format") },
  { gettext_noop ("created by software"),
    gettext_noop ("name of the software that created the document") },
  { gettext_noop ("unknown date"),
    gettext_noop ("ambiguous date (could specify creation time, modification time or access time)") },
  { gettext_noop ("creation date"),
    gettext_noop ("date the document was created") },
  { gettext_noop ("modification date"),
    gettext_noop ("date the document was modified") },
  /* 60 */
  { gettext_noop ("last printed"),
    gettext_noop ("date the document was last printed") },
  { gettext_noop ("last saved by"),
    gettext_noop ("name of the user who saved the document last") },
  { gettext_noop ("total editing time"),
    gettext_noop ("time spent editing the document") },
  { gettext_noop ("editing cycles"),
    gettext_noop ("number of editing cycles") },
  { gettext_noop ("modified by software"),
    gettext_noop ("name of software making modifications") },
  /* 65 */
  { gettext_noop ("revision history"),
    gettext_noop ("information about the revision history") },
  { gettext_noop ("embedded file size"),
    gettext_noop ("size of the contents of the container as embedded in the file") },
  { gettext_noop ("file type"),
    gettext_noop ("standard Macintosh Finder file type information") },
  { gettext_noop ("creator"),
    gettext_noop ("standard Macintosh Finder file creator information") },
  { gettext_noop ("package name"),
    gettext_noop ("unique identifier for the package") },
  /* 70 */
  { gettext_noop ("package version"),
    gettext_noop ("version of the software and its package") },
  { gettext_noop ("section"),
    gettext_noop ("category the software package belongs to") },
  { gettext_noop ("upload priority"),
    gettext_noop ("priority for promoting the release to production") },
  { gettext_noop ("dependencies"),
    gettext_noop ("packages this package depends upon") },
  { gettext_noop ("conflicting packages"),
    gettext_noop ("packages that cannot be installed with this package") },
  /* 75 */
  { gettext_noop ("replaced packages"),
    gettext_noop ("packages made obsolete by this package") },
  { gettext_noop ("provides"),
    gettext_noop ("functionality provided by this package") },
  { gettext_noop ("recommendations"),
    gettext_noop ("packages recommended for installation in conjunction with this package") },
  { gettext_noop ("suggestions"),
    gettext_noop ("packages suggested for installation in conjunction with this package") },
  { gettext_noop ("maintainer"),
    gettext_noop ("name of the maintainer") },
  /* 80 */
  { gettext_noop ("installed size"),
    gettext_noop ("space consumption after installation") },
  { gettext_noop ("source"),
    gettext_noop ("original source code") },
  { gettext_noop ("is essential"),
    gettext_noop ("package is marked as essential") },
  { gettext_noop ("target architecture"),
    gettext_noop ("hardware architecture the package can be used for") },
  { gettext_noop ("pre-dependency"),
    gettext_noop ("dependency that must be satisfied before installation") }, 
  /* 85 */
  { gettext_noop ("license"),
    gettext_noop ("applicable copyright license") }, 
  { gettext_noop ("distribution"),
    gettext_noop ("distribution the package is a part of") }, 
  { gettext_noop ("build host"),
    gettext_noop ("machine the package was build on") }, 
  { gettext_noop ("vendor"),
    gettext_noop ("name of the software vendor") }, 
  { gettext_noop ("target operating system"),
    gettext_noop ("operating system for which this package was made") }, 
  /* 90 */
  { gettext_noop ("software version"),
    gettext_noop ("version of the software contained in the file") }, 
  { gettext_noop ("target platform"),
    gettext_noop ("name of the architecture, operating system and distribution this package is for") }, 
  { gettext_noop ("resource type"),
    gettext_noop ("categorization of the nature of the resource that is more specific than the file format") }, 
  { gettext_noop ("library search path"),
    gettext_noop ("path in the file system to be considered when looking for required libraries") }, 
  { gettext_noop ("library dependency"),
    gettext_noop ("name of a library that this file depends on") }, 
  /* 95 */
  { gettext_noop ("camera make"),
    gettext_noop ("") }, 
  { gettext_noop ("camera model"),
    gettext_noop ("") }, 
  { gettext_noop ("exposure"),
    gettext_noop ("") }, 
  { gettext_noop ("aperture"),
    gettext_noop ("") }, 
  { gettext_noop ("exposure bias"),
    gettext_noop ("") }, 
  /* 100 */
  { gettext_noop ("flash"),
    gettext_noop ("") }, 
  { gettext_noop ("flash bias"),
    gettext_noop ("") }, 
  { gettext_noop ("focal length"),
    gettext_noop ("") }, 
  { gettext_noop ("focal length 35mm"),
    gettext_noop ("") }, 
  { gettext_noop ("iso speed"),
    gettext_noop ("") }, 
  /* 105 */
  { gettext_noop ("exposure mode"),
    gettext_noop ("") }, 
  { gettext_noop ("metering mode"),
    gettext_noop ("") }, 
  { gettext_noop ("macro mode"),
    gettext_noop ("") }, 
  { gettext_noop ("image quality"),
    gettext_noop ("") }, 
  { gettext_noop ("white balance"),
    gettext_noop ("") }, 
  /* 110 */
  { gettext_noop ("orientation"),
    gettext_noop ("") }, 
  { gettext_noop ("magnification"),
    gettext_noop ("") }, 
  { gettext_noop ("image dimensions"),
    gettext_noop ("size of the image in pixels (width times height)") }, 
  { gettext_noop ("produced by software"),
    gettext_noop ("") }, /* what is the exact difference between the software
			    creator and the software producer? PDF and DVI
			    both have this distinction (i.e., Writer vs.
			    OpenOffice) */
  { gettext_noop ("thumbnail"),
    gettext_noop ("smaller version of the image for previewing") }, 
  /* 115 */
  { gettext_noop ("image resolution"),
    gettext_noop ("resolution in dots per inch") }, 
  { gettext_noop ("source"),
    gettext_noop ("Originating entity") }, 
  { gettext_noop ("character set"),
    gettext_noop ("character encoding used") }, 
  { gettext_noop ("line count"),
    gettext_noop ("number of lines") }, 
  { gettext_noop ("paragraph count"),
    gettext_noop ("number o paragraphs") }, 
  /* 120 */
  { gettext_noop ("word count"),
    gettext_noop ("number of words") }, 
  { gettext_noop ("character count"),
    gettext_noop ("number of characters") }, 
  { gettext_noop ("page orientation"),
    gettext_noop ("") }, 
  { gettext_noop ("paper size"),
    gettext_noop ("") }, 
  { gettext_noop ("template"),
    gettext_noop ("template the document uses or is based on") }, 
  /* 125 */
  { gettext_noop ("company"),
    gettext_noop ("") }, 
  { gettext_noop ("manager"),
    gettext_noop ("") }, 
  { gettext_noop ("revision number"),
    gettext_noop ("") }, 
  { gettext_noop ("duration"),
    gettext_noop ("play time for the medium") }, 
  { gettext_noop ("album"),
    gettext_noop ("name of the album") }, 
  /* 130 */
  { gettext_noop ("artist"),
    gettext_noop ("name of the artist or band") }, 
  { gettext_noop ("genre"),
    gettext_noop ("") }, 
  { gettext_noop ("track number"),
    gettext_noop ("original number of the track on the distribution medium") }, 
  { gettext_noop ("disk number"),
    gettext_noop ("number of the disk in a multi-disk (or volume) distribution") }, 
  { gettext_noop ("performer"),
    gettext_noop ("The artist(s) who performed the work (conductor, orchestra, soloists, actor, etc.)") }, 
  /* 135 */
  { gettext_noop ("contact"),
    gettext_noop ("Contact information for the creator or distributor") }, 
  { gettext_noop ("song version"),
    gettext_noop ("name of the version of the song (i.e. remix information)") }, 
  { gettext_noop ("picture"),
    gettext_noop ("associated misc. picture") }, 
  { gettext_noop ("cover picture"),
    gettext_noop ("picture of the cover of the distribution medium") }, 
  { gettext_noop ("contributor picture"),
    gettext_noop ("picture of one of the contributors") }, 
  /* 140 */
  { gettext_noop ("event picture"),
    gettext_noop ("picture of an associated event") }, 
  { gettext_noop ("logo"),
    gettext_noop ("logo of an associated organization") }, 
  { gettext_noop ("broadcast television system"),
    gettext_noop ("name of the television system for which the data is coded") }, 
  { gettext_noop (""),
    gettext_noop ("") }, 
  { gettext_noop (""),
    gettext_noop ("") }, 
  { gettext_noop (""),
    gettext_noop ("") }, 
#if 0
  
  gettext_noop("author"),
  gettext_noop("artist"), /* 5 */
  gettext_noop("description"),
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
  gettext_noop("disc number"), 
  gettext_noop("preferred display style (GNUnet)"), /* 135 */
  gettext_noop("GNUnet URI of ECBC data"),
  gettext_noop("Complete file data (for non-binary files only)"),
  gettext_noop("rating"), /* 145 */

#endif
};

/**
 * Total number of keyword types (for bounds-checking) 
 */
#define HIGHEST_METATYPE_NUMBER (sizeof (meta_type_descriptions) / sizeof(*meta_type_descriptions))


/**
 * Get the textual name of the keyword.
 *
 * @param type meta type to get a UTF-8 string for
 * @return NULL if the type is not known, otherwise
 *         an English (locale: C) string describing the type;
 *         translate using 'dgettext ("libextractor", rval)'
 */
const char *
EXTRACTOR_metatype_to_string(enum EXTRACTOR_MetaType type)
{
  if ((type < 0) || (type >= HIGHEST_METATYPE_NUMBER))
    return NULL;
  return meta_type_descriptions[type].short_description;
}


/**
 * Get a long description for the meta type.
 *
 * @param type meta type to get a UTF-8 description for
 * @return NULL if the type is not known, otherwise
 *         an English (locale: C) string describing the type;
 *         translate using 'dgettext ("libextractor", rval)'
 */
const char *
EXTRACTOR_metatype_to_description(enum EXTRACTOR_MetaType type)
{
  if ((type < 0) || (type >= HIGHEST_METATYPE_NUMBER))
    return NULL;
  return meta_type_descriptions[type].long_description;
}


/**
 * Return the highest type number, exclusive as in [0,max).
 *
 * @return highest legal metatype number for this version of libextractor
 */
enum EXTRACTOR_MetaType
EXTRACTOR_metatype_get_max ()
{
  return HIGHEST_METATYPE_NUMBER;
}


/* end of extractor_metatypes.c */
