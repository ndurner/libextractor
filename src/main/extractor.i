/* libextractor interface for SWIG */
/* extractor.i */
%module extractor
%{
%}

typedef struct EXTRACTOR_Keywords {
  char * keyword;
  EXTRACTOR_KeywordType keywordType;
  struct EXTRACTOR_Keywords * next;
} EXTRACTOR_KeywordList;

EXTRACTOR_ExtractorList * EXTRACTOR_loadDefaultLibraries();

const char * EXTRACTOR_getKeywordTypeAsString(const EXTRACTOR_KeywordType type);

EXTRACTOR_ExtractorList * 
EXTRACTOR_loadConfigLibraries(EXTRACTOR_ExtractorList * prev,
			      const char * config);

EXTRACTOR_ExtractorList * 
EXTRACTOR_addLibrary(EXTRACTOR_ExtractorList * prev,
		     const char * library);

EXTRACTOR_ExtractorList * 
EXTRACTOR_addLibraryLast(EXTRACTOR_ExtractorList * prev,
			 const char * library);

EXTRACTOR_ExtractorList * 
EXTRACTOR_removeLibrary(EXTRACTOR_ExtractorList * prev,
			const char * library);

void EXTRACTOR_removeAll(EXTRACTOR_ExtractorList * libraries);

EXTRACTOR_KeywordList * 
EXTRACTOR_getKeywords(EXTRACTOR_ExtractorList * extractor,
		      const char * filename);

EXTRACTOR_KeywordList * 
EXTRACTOR_removeDuplicateKeywords(EXTRACTOR_KeywordList * list,
				  const unsigned int options);


EXTRACTOR_KeywordList *
EXTRACTOR_removeEmptyKeywords (EXTRACTOR_KeywordList * list);


void EXTRACTOR_freeKeywords(EXTRACTOR_KeywordList * keywords);

const char * EXTRACTOR_extractLast(const EXTRACTOR_KeywordType type, 
				   EXTRACTOR_KeywordList * keywords);

const char * EXTRACTOR_extractLastByString (const char * type,
					    EXTRACTOR_KeywordList * keywords);

unsigned int EXTRACTOR_countKeywords(EXTRACTOR_KeywordList * keywords);


