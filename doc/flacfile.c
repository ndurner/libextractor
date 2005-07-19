/*
flacfile.c - FLAC file attachment utility
Copyright (C) 2005 Ian Gulliver

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "firemake.h"
#include <FLAC/metadata.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *get_mime_type(const char *filename) {
	magic_t magic_flags;
	const char *mime_type;

	magic_flags = magic_open(MAGIC_MIME);
	if (magic_flags == NULL) {
		perror("flacfile: magic_open");
		exit(1);
	}

	if (magic_load(magic_flags,NULL) != 0) {
		perror("flacfile: magic_load");
		exit(1);
	}

	mime_type = magic_file(magic_flags,filename);
	if (mime_type == NULL) {
		fprintf(stderr,"flacfile: Unable to determine MIME type automatically; please specify one.\n");
	}

	return mime_type;
}

FLAC__StreamMetadata *new_block(const char *filename, const char *description, const char *mime) {
	// block size is mime length + description length + file length + 2
	size_t desc_len = strlen(description);
	size_t mime_len = strlen(mime);
	struct stat s;
	int f = open(filename,O_RDONLY);
	static FLAC__StreamMetadata meta;
	char *block;
	int pos = 0;

	if (desc_len > 255 || mime_len > 255) {
		fprintf(stderr,"flacfile: Description and MIME type must both be shorter than 256 characters.\n");
		exit(1);
	}

	if (f == -1) {
		perror("flacfile: open(FLAC file)");
		exit(1);
	}

	if (fstat(f,&s) != 0) {
		perror("flacfile: fstat(FLAC file)");
		exit(1);
	}

	block = malloc(desc_len + mime_len + s.st_size + 2);
	if (block == NULL) {
		perror("flacfile: malloc");
		exit(1);
	}

	block[pos++] = (char) desc_len;
	strcpy(&block[pos],description);
	pos += desc_len;

	block[pos++] = (char) mime_len;
	strcpy(&block[pos],mime);
	pos += mime_len;

	if (read(f,&block[pos],s.st_size) != s.st_size) {
		perror("flacfile: read(FLAC file)");
		exit(1);
	}

	meta.is_last = false;
	meta.length = desc_len + mime_len + s.st_size + 6;
	meta.type = FLAC__METADATA_TYPE_APPLICATION;
	memcpy(meta.data.application.id,"ATCH",4);
	meta.data.application.data = block;

	return &meta;
}

int add(FLAC__Metadata_SimpleIterator *iter, char *description, char *attach, const char *mime_type) {
	if (mime_type == NULL)
		mime_type = get_mime_type(attach);

	if (FLAC__metadata_simple_iterator_insert_block_after(iter,new_block(attach,description,mime_type),true) != true) {
		perror("flacfile: FLAC__metadata_simple_iterator_insert_block_after");
		exit(1);
	}

	return 0;
}

int list(FLAC__Metadata_SimpleIterator *iter) {
	while (1) {
		int pos;
		FLAC__StreamMetadata *meta;
		if (FLAC__metadata_simple_iterator_get_block_type(iter) != FLAC__METADATA_TYPE_APPLICATION) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 0;
			continue;
		}

		meta = FLAC__metadata_simple_iterator_get_block(iter);
		if (memcmp(meta->data.application.id,"ATCH",4) != 0) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 0;
			FLAC__metadata_object_delete(meta);
			continue;
		}

		/* this is one of our blocks */
		write(1,"Description: '",14);
		if (meta->length < 1 || meta->length < meta->data.application.data[0] + 1) {
			write(1,"(invalid block)\n",16);
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 0;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		write(1,&meta->data.application.data[1],meta->data.application.data[0]);
		write(1,"', ",3);
		pos = meta->data.application.data[0] + 1;

		write(1,"MIME: '",7);
		if (meta->length < pos + 1 || meta->length < meta->data.application.data[pos] + pos + 1) {
			write(1,"(invalid block)\n",16);
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 0;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		write(1,&meta->data.application.data[pos+1],meta->data.application.data[pos]);
		write(1,"', ",3);
		pos += meta->data.application.data[pos] + 1;
		
		printf("Length: %d\n",meta->length - pos - 4);

		FLAC__metadata_object_delete(meta);

		if (FLAC__metadata_simple_iterator_next(iter) == false)
			return 0;
	}
}

int extract(FLAC__Metadata_SimpleIterator *iter, char *description, char *filename, char *mime) {
	while (1) {
		int pos;
		int o;
		FLAC__StreamMetadata *meta;

		if (FLAC__metadata_simple_iterator_get_block_type(iter) != FLAC__METADATA_TYPE_APPLICATION) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			continue;
		}

		meta = FLAC__metadata_simple_iterator_get_block(iter);
		if (memcmp(meta->data.application.id,"ATCH",4) != 0) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}

		/* this is one of our blocks */
		if (meta->length < 1 || meta->length < meta->data.application.data[0] + 1) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		if (meta->data.application.data[0] != strlen(description) || memcmp(&meta->data.application.data[1],description,meta->data.application.data[0]) != 0) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		pos = meta->data.application.data[0] + 1;

		if (meta->length < pos + 1 || meta->length < meta->data.application.data[pos] + pos + 1) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}

		if (mime != NULL && 
				(meta->data.application.data[pos] != strlen(mime) || memcmp(&meta->data.application.data[pos+1],mime,meta->data.application.data[pos]) != 0)) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		pos += meta->data.application.data[pos] + 1;

		o = open(filename,O_WRONLY|O_CREAT,0644);
		if (o == -1) {
			perror("flacfile: open");
			exit(1);
		}

		if (write(o,&meta->data.application.data[pos],meta->length - pos - 4));
		
		FLAC__metadata_object_delete(meta);

		return 0;
	}
}

int my_remove(FLAC__Metadata_SimpleIterator *iter, char *description, char *mime) {
	while (1) {
		int pos;
		FLAC__StreamMetadata *meta;

		if (FLAC__metadata_simple_iterator_get_block_type(iter) != FLAC__METADATA_TYPE_APPLICATION) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			continue;
		}

		meta = FLAC__metadata_simple_iterator_get_block(iter);
		if (memcmp(meta->data.application.id,"ATCH",4) != 0) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}

		/* this is one of our blocks */
		if (meta->length < 1 || meta->length < meta->data.application.data[0] + 1) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		if (meta->data.application.data[0] != strlen(description) || memcmp(&meta->data.application.data[1],description,meta->data.application.data[0]) != 0) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		pos = meta->data.application.data[0] + 1;

		if (meta->length < pos + 1 || meta->length < meta->data.application.data[pos] + pos + 1) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}

		if (mime != NULL && 
				(meta->data.application.data[pos] != strlen(mime) || memcmp(&meta->data.application.data[pos+1],mime,meta->data.application.data[pos]) != 0)) {
			if (FLAC__metadata_simple_iterator_next(iter) == false)
				return 1;
			FLAC__metadata_object_delete(meta);
			continue;
		}
		pos += meta->data.application.data[pos] + 1;

		if (FLAC__metadata_simple_iterator_delete_block(iter,false) != true) {
			perror("flacfile: FLAC__metadata_simple_iterator_delete_block");
			exit(1);
		}

		FLAC__metadata_object_delete(meta);

		return 0;
	}
}

void usage() {
	fprintf(stderr,"flacfile v" VERSION "\n\n");
	fprintf(stderr,"Usage: flacfile add <FLAC filename> <description> <filename to attach> [<MIME type>]\n");
	fprintf(stderr,"       flacfile list <FLAC filename>\n");
	fprintf(stderr,"       flacfile extract <FLAC filename> <description> <filename to save as> [<MIME type>]\n");
	fprintf(stderr,"       flacfile remove <FLAC filename> <description> [<MIME type>]\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	FLAC__Metadata_SimpleIterator *iter;

	if (argc < 3 || argc > 6)
		usage();

	iter = FLAC__metadata_simple_iterator_new();
	if (iter == NULL) {
		perror("flacfile: FLAC__metadata_simple_iterator_new");
		exit(1);
	}

	if (FLAC__metadata_simple_iterator_init(iter,argv[2],false,false) != true) {
		perror("flacfile: FLAC__metadata_simple_iterator_init");
		exit(1);
	}

	if (strcmp(argv[1],"add") == 0) {
		if (argc < 5)
			usage();
		if (strcmp(argv[2],argv[4]) == 0) {
			fprintf(stderr,"flacfile: You didn't really want to attach a file to itself.\n");
			exit(1);
		}
		return add(iter,argv[3],argv[4],argc == 6 ? argv[5] : NULL);
	} else if (strcmp(argv[1],"list") == 0) {
		if (argc > 3)
			usage();
		return list(iter);
	} else if (strcmp(argv[1],"extract") == 0) {
		int r;
		if (argc < 5)
			usage();
		if (strcmp(argv[2],argv[4]) == 0) {
			fprintf(stderr,"flacfile: You didn't really want to extract a file onto itself.\n");
			exit(1);
		}
		r = extract(iter,argv[3],argv[4],argc == 6 ? argv[5] : NULL);
		if (r != 0)
			fprintf(stderr,"flacfile: requested block not found\n");
		return r;
	} else if (strcmp(argv[1],"remove") == 0) {
		int r;
		if (argc < 4)
			usage();
		r = my_remove(iter,argv[3],argc == 5 ? argv[4] : NULL);
		if (r != 0)
			fprintf(stderr,"flacfile: requested block not found\n");
		return r;
	} else {
		usage();
		return 1;
	}
}
