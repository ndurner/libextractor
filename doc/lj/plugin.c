if ( (data[0] != 0xFF) || (data[1] != 0xD8) )
  return prev; /* not a JPEG */
addKeyword(&prev,
           strdup("image/jpeg"),
           EXTRACTOR_MIMETYPE);
/* ... more parsing code here ... */
return prev;

Caption: jpegextractor.c adds the Mime-Type to the list after parsing the file header.
