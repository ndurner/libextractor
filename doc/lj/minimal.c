#include <extractor.h>
int main(int argc, char * argv[]) {
  EXTRACTOR_ExtractorList * plugins;
  EXTRACTOR_KeywordList   * md_list;
  plugins = EXTRACTOR_loadDefaultLibraries();
  md_list = EXTRACTOR_getKeywords(plugins, argv[1]);
  EXTRACTOR_printKeywords(stdout, md_list);
  EXTRACTOR_freeKeywords(md_list);
  EXTRACTOR_removeAll(plugins); /* unload plugins */
}

Caption: minimal.c shows the most important libextractor functions in concert.
