
/**
 * Convert the given input using the given converter
 * and return as a 0-terminated string.
 */
static char * iconvHelper(iconv_t cd,
			  const char * in) {
  size_t inSize;
  char * buf;
  char * ibuf;
  const char * i;
  size_t outSize;
  size_t outLeft;

  i = in;
  /* reset iconv */
  iconv(cd, NULL, NULL, NULL, NULL);

  inSize = strlen(in);
  outSize = 4 * strlen(in) + 2;
  outLeft = outSize - 2; /* make sure we have 2 0-terminations! */
  buf = malloc(outSize);
  ibuf = buf;
  memset(buf, 0, outSize);
  if (iconv(cd, 
	    (char**) &in,
	    &inSize,
	    &ibuf, 
	    &outLeft) == (size_t)-1) {
    /* conversion failed */
    free(buf);
    return strdup(i); 
  }
  return buf;
}
