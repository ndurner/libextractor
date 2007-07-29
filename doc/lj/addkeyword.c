static void addKeyword
  (struct EXTRACTOR_Keywords **list,
   char *keyword, EXTRACTOR_KeywordType type)
{
  EXTRACTOR_KeywordList *next;
  next = malloc (sizeof (EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = keyword;
  next->keywordType = type;
  *list = next;
}

Caption:The plugins return the meta - data using a simple linked - list.
