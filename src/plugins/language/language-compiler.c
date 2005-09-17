/*
     This file is part of libextractor.
     (C) 2005 Vidyut Samanta and Christian Grothoff

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


int main(int argc,
	 char ** argv) {
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
	    _("Please provide a list of klp files as arguments.\n"));
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


  fprintf(stdout,
	  "#include \"somefile.h\"\n");
  fprintf(stdout,
	  "static int bits[] = { ");
  for (i=0;i<bf.bitArraySize/sizeof(int);i++)
    fprintf(stdout,
	    "%dL,",
	    (((int*)bf.bitArray)[i]));
  fprintf(stdout,
	  "};\n");
  return 0;
}
