/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004 Vidyut Samanta and Christian Grothoff

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

     Portions of this code were adapted from libhtmlparse by
     Mooneer Salem (mooneer@translator.cs).  The main changes
     to libhtmlparse were the removal of globals to make the
     code reentrant.
 */
/**
 * Tool to build a bloomfilter from a dictionary.
 */

#include "platform.h"
#include "bloomfilter.h"
#include "bloomfilter.c"

#define ADDR_PER_ELEMENT 46

int main(int argc,
	 char ** argv) {
  Bloomfilter bf;
  HashCode160 hc;
  int i;
  int cnt;
  char * fn;
  char ** words;
  char line[2048]; /* buffer overflow, here we go */
  FILE *dictin;
  char * bn;
#define ALLOCSIZE 1024*1024

  if (argc<2) {
    fprintf(stderr,
	    _("Please provide the name of the language you are building\n"
	      "a dictionary for.  For example:\n"));
    fprintf(stderr, "$ ./dictionary-builder en > en.c\n");
    exit(-1);
  }

  fn = malloc(strlen(argv[1]) + 6);
  strcpy(fn, argv[1]);
  strcat(fn, ".txt");
  dictin=fopen(fn,"r");
  free(fn);
  if (dictin==NULL) {
    fprintf(stderr,
	    _("Error opening file `%s': %s\n"),
	    argv[1],strerror(errno));
    exit(-1);
  }

  words = malloc(sizeof(char*) * ALLOCSIZE); /* don't we LOVE constant size buffers? */
  if (words == NULL) {
    fprintf(stderr,
	    _("Error allocating: %s\n."),
	    strerror(errno));
    exit(-1);
  }
  cnt = 0;
  memset(&line[0], 0, 2048);
  while (1 == fscanf(dictin, "%s", (char*)&line)) {
    words[cnt] = strdup(line);
    cnt++;
    memset(&line[0], 0, 2048);
    if (cnt > ALLOCSIZE) {
      fprintf(stderr,
	      _("Increase ALLOCSIZE (in %s).\n"),
	      __FILE__);
      exit(-1);
    }

  }

  bf.addressesPerElement = ADDR_PER_ELEMENT;
  bf.bitArraySize = cnt*4;
  bf.bitArray = malloc(bf.bitArraySize);
  memset(bf.bitArray, 0, bf.bitArraySize);

  for (i=0;i<cnt;i++) {
    hash(words[i],
	 strlen(words[i]),
	 &hc);
    addToBloomfilter(&bf, &hc);
  }

  fprintf(stdout,
	  "#include \"bloomfilter.h\"\n");

  /* use int[] instead of char[] since it cuts the memory use of
     gcc down to a quarter; don't use long long since various
     gcc versions then output tons of warnings about "decimal constant
     is so large that it is unsigned" (even for unsigned long long[]
     that warning is generated and dramatically increases compile times). */
  fprintf(stdout,
	  "static int bits[] = { ");
  for (i=0;i<bf.bitArraySize/sizeof(int);i++)
    fprintf(stdout,
	    "%dL,",
	    (((int*)bf.bitArray)[i]));
  fprintf(stdout,
	  "};\n");
  bn = &argv[1][strlen(argv[1])];
  while ( (bn != argv[1]) &&
	  (bn[0] != '/') )
    bn--;
  if (bn[0] == '/')
    bn++;
  fprintf(stdout,
	  "Bloomfilter libextractor_printable_%s_filter = {\n"
	  "  %u,\n"
	  "  (unsigned char*)bits,\n"
	  "  %u };\n",
	  bn,
	  ADDR_PER_ELEMENT,
	  bf.bitArraySize);
  return 0;
}
