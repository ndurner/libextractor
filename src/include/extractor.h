/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005 Vidyut Samanta and Christian Grothoff

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

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 0.2.6-1 => 0x00020601
 * 4.5.2-0 => 0x04050200
 */
#define EXTRACTOR_VERSION 0x00040200

#include <stdio.h>

/* ignore the 'type' of the keyword when eliminating duplicates */
#define EXTRACTOR_DUPLICATES_TYPELESS 1
/* remove type 'UNKNOWN' if there is a duplicate keyword of
   known type, even if usually different types should be
   preserved */
#define EXTRACTOR_DUPLICATES_REMOVE_UNKNOWN 2

#define EXTRACTOR_DEFAULT_LIBRARIES EXTRACTOR_getDefaultLibraries()

const char * EXTRACTOR_getDefaultLibraries();

/**
 * Enumeration defining various sources of keywords.
 * See also
 * http://dublincore.org/documents/1998/09/dces/
 */
typedef enum {
  EXTRACTOR_UNKNOWN = 0,
  EXTRACTOR_FILENAME = 1,
  EXTRACTOR_MIMETYPE = 2,
  EXTRACTOR_TITLE = 3,
  EXTRACTOR_AUTHOR = 4,
  EXTRACTOR_ARTIST = 5,
  EXTRACTOR_DESCRIPTION = 6,
  EXTRACTOR_COMMENT = 7,
  EXTRACTOR_DATE = 8,
  EXTRACTOR_PUBLISHER = 9,
  EXTRACTOR_LANGUAGE = 10,
  EXTRACTOR_ALBUM = 11,
  EXTRACTOR_GENRE = 12,
  EXTRACTOR_LOCATION = 13,
  EXTRACTOR_VERSIONNUMBER = 14,
  EXTRACTOR_ORGANIZATION = 15,
  EXTRACTOR_COPYRIGHT = 16,
  EXTRACTOR_SUBJECT = 17,
  EXTRACTOR_KEYWORDS = 18,
  EXTRACTOR_CONTRIBUTOR = 19,
  EXTRACTOR_RESOURCE_TYPE = 20,
  EXTRACTOR_FORMAT = 21,
  EXTRACTOR_RESOURCE_IDENTIFIER = 22,
  EXTRACTOR_SOURCE = 23,
  EXTRACTOR_RELATION = 24,
  EXTRACTOR_COVERAGE = 25,
  EXTRACTOR_SOFTWARE = 26,
  EXTRACTOR_DISCLAIMER = 27,
  EXTRACTOR_WARNING = 28,
  EXTRACTOR_TRANSLATED = 29,
  EXTRACTOR_CREATION_DATE = 30,
  EXTRACTOR_MODIFICATION_DATE = 31,
  EXTRACTOR_CREATOR = 32,
  EXTRACTOR_PRODUCER = 33,
  EXTRACTOR_PAGE_COUNT = 34,
  EXTRACTOR_PAGE_ORIENTATION = 35,
  EXTRACTOR_PAPER_SIZE = 36,
  EXTRACTOR_USED_FONTS = 37,
  EXTRACTOR_PAGE_ORDER = 38,
  EXTRACTOR_CREATED_FOR = 39,
  EXTRACTOR_MAGNIFICATION = 40,
  EXTRACTOR_RELEASE = 41,
  EXTRACTOR_GROUP = 42,
  EXTRACTOR_SIZE = 43,
  EXTRACTOR_SUMMARY = 44,
  EXTRACTOR_PACKAGER = 45,
  EXTRACTOR_VENDOR = 46,
  EXTRACTOR_LICENSE = 47,
  EXTRACTOR_DISTRIBUTION = 48,
  EXTRACTOR_BUILDHOST = 49,
  EXTRACTOR_OS = 50,
  EXTRACTOR_DEPENDENCY = 51,
  EXTRACTOR_HASH_MD4 = 52,
  EXTRACTOR_HASH_MD5 = 53,
  EXTRACTOR_HASH_SHA0 = 54,
  EXTRACTOR_HASH_SHA1 = 55,
  EXTRACTOR_HASH_RMD160 = 56,
  EXTRACTOR_RESOLUTION = 57,
  EXTRACTOR_CATEGORY = 58,
  EXTRACTOR_BOOKTITLE = 59,
  EXTRACTOR_PRIORITY = 60,
  EXTRACTOR_CONFLICTS = 61,
  EXTRACTOR_REPLACES = 62,
  EXTRACTOR_PROVIDES = 63,
  EXTRACTOR_CONDUCTOR = 64,
  EXTRACTOR_INTERPRET = 65,
  EXTRACTOR_OWNER = 66,
  EXTRACTOR_LYRICS = 67,
  EXTRACTOR_MEDIA_TYPE = 68,
  EXTRACTOR_CONTACT = 69,
  EXTRACTOR_THUMBNAIL_DATA = 70,
  EXTRACTOR_PUBLICATION_DATE = 71,
} EXTRACTOR_KeywordType;

/**
 * A linked list of keywords. This structure is passed around
 * in libExtractor and is typically the result of any keyword
 * extraction operation.
 * <p>
 * Each entry in the keyword list consists of a string (the
 * keyword) and the keyword type (of type KeywordType)
 * describing how/from where the keyword was obtained.
 */
typedef struct EXTRACTOR_Keywords {
  /* the keyword that was found */
  char * keyword;
  /* the type of the keyword (classification) */
  EXTRACTOR_KeywordType keywordType;
  /* the next entry in the list */
  struct EXTRACTOR_Keywords * next;
} EXTRACTOR_KeywordList;

/**
 * Signature of the extract method that each plugin
 * must provide.
 */
typedef EXTRACTOR_KeywordList * (*ExtractMethod)(const char * filename,
						 char * data,
						 size_t filesize,
						 EXTRACTOR_KeywordList * next);

/**
 * Linked list of extractor helper-libraries. An application
 * builds this list by telling libextractor to load various
 * keyword-extraction libraries. Libraries can also be unloaded
 * (removed from this list, see removeLibrary).
 * <p>
 * Client code should never be concerned with the internals of
 * this struct.
 */
typedef struct EXTRACTOR_Extractor {
  void * libraryHandle;
  char * libname;
  ExtractMethod extractMethod;
  struct EXTRACTOR_Extractor * next;
} EXTRACTOR_ExtractorList;

/**
 * Load the default set of libraries.
 * @return the default set of libraries.
 */
EXTRACTOR_ExtractorList * EXTRACTOR_loadDefaultLibraries();

/**
 * Get the textual name of the keyword.
 * @return NULL if the type is not known
 */
const char * EXTRACTOR_getKeywordTypeAsString(const EXTRACTOR_KeywordType type);

/**
 * Return the highest type number, exclusive as in [0,highest).
 */
EXTRACTOR_KeywordType EXTRACTOR_getHighestKeywordTypeNumber();

/**
 * Load multiple libraries as specified by the user.
 * @param config a string given by the user that defines which
 *        libraries should be loaded. Has the format
 *        "[[-]LIBRARYNAME[:[-]LIBRARYNAME]]*". For example,
 *        libextractor_mp3.so:libextractor_ogg.so loads the
 *        mp3 and the ogg library. The '-' before the LIBRARYNAME
 *        indicates that the library should be added to the end
 *        of the library list (addLibraryLast).
 * @param prev the  previous list of libraries, may be NULL
 * @return the new list of libraries, equal to prev iff an error occured
 *         or if config was empty (or NULL).
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_loadConfigLibraries(EXTRACTOR_ExtractorList * prev,
			      const char * config);

/**
 * Add a library for keyword extraction.
 * @param prev the previous list of libraries, may be NULL
 * @param library the name of the library
 * @return the new list of libraries, equal to prev iff an error occured
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_addLibrary(EXTRACTOR_ExtractorList * prev,
		     const char * library);

/**
 * Add a library for keyword extraction at the END of the list.
 * @param prev the previous list of libraries, may be NULL
 * @param library the name of the library
 * @return the new list of libraries, always equal to prev
 *         except if prev was NULL and no error occurs
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_addLibraryLast(EXTRACTOR_ExtractorList * prev,
			 const char * library);

/**
 * Remove a library for keyword extraction.
 * @param prev the current list of libraries
 * @param library the name of the library to remove
 * @return the reduced list, unchanged if the library was not loaded
 */
EXTRACTOR_ExtractorList *
EXTRACTOR_removeLibrary(EXTRACTOR_ExtractorList * prev,
			const char * library);

/**
 * Remove all extractors.
 * @param libraries the list of extractors
 */
void EXTRACTOR_removeAll(EXTRACTOR_ExtractorList * libraries);

/**
 * Extract keywords from a file using the available extractors.
 * @param extractor the list of extractor libraries
 * @param filename the name of the file
 * @return the list of keywords found in the file, NULL if none
 *         were found (or other errors)
 */
EXTRACTOR_KeywordList *
EXTRACTOR_getKeywords(EXTRACTOR_ExtractorList * extractor,
		      const char * filename);


/**
 * Remove duplicate keywords from the list.
 * @param list the original keyword list (destroyed in the process!)
 * @param options a set of options (DUPLICATES_XXXX)
 * @return a list of keywords without duplicates
 */
EXTRACTOR_KeywordList *
EXTRACTOR_removeDuplicateKeywords(EXTRACTOR_KeywordList * list,
				  const unsigned int options);


/**
 * Remove empty (all-whitespace) keywords from the list.
 * @param list the original keyword list (destroyed in the process!)
 * @return a list of keywords without duplicates
 */
EXTRACTOR_KeywordList *
EXTRACTOR_removeEmptyKeywords (EXTRACTOR_KeywordList * list);

/**
 * Print a keyword list to a file.
 * For debugging.
 * @param handle the file to write to (stdout, stderr), must NOT be NULL
 * @param keywords the list of keywords to print, may be NULL
 */
void EXTRACTOR_printKeywords(FILE * handle,
			     EXTRACTOR_KeywordList * keywords);

/**
 * Free the memory occupied by the keyword list (and the
 * keyword strings in it!)
 * @param keywords the list to free
 */
void EXTRACTOR_freeKeywords(EXTRACTOR_KeywordList * keywords);

/**
 * Extract the last keyword that of the given type from the keyword list.
 * @param type the type of the keyword
 * @param keywords the keyword list
 * @return the last matching keyword, or NULL if none matches;
 *  the string returned is aliased in the keywords list and must
 *  not be freed or manipulated by the client.  It will become
 *  invalid once the keyword list is freed.
 */
const char * EXTRACTOR_extractLast(const EXTRACTOR_KeywordType type,
				   EXTRACTOR_KeywordList * keywords);

/**
 * Extract the last keyword of the given string from the keyword list.
 * @param type the string describing the type of the keyword
 * @param keywords the keyword list
 * @return the last matching keyword, or NULL if none matches;
 *  the string returned is aliased in the keywords list and must
 *  not be freed or manipulated by the client.  It will become
 *  invalid once the keyword list is freed.
 */
const char * EXTRACTOR_extractLastByString (const char * type,
					    EXTRACTOR_KeywordList * keywords);

/**
 * Count the number of keywords in the keyword list.
 * @param keywords the keyword list
 * @return the number of keywords in the list
 */
unsigned int EXTRACTOR_countKeywords(EXTRACTOR_KeywordList * keywords);


#ifdef __cplusplus
}
#endif

#endif
