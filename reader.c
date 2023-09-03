#include "reader.h"
#include "pcube.h"
#include "bitface.h"

Reader* reader_create(char* filename, ReaderMode mode) {
	FILE* file = fopen(filename, "rb");
	if (file == NULL) {
		printf("Failed to open file `%s` for reading\n", filename);
		return NULL;
	}

	Reader* retval = calloc(1, sizeof(Reader));
	retval->file = file;
	retval->mode = mode;
	retval->count = 0;
	
	switch (mode) {
		case ReadBitFace:
			printf("Starting file reader in BitFace mode.\n");
			if(!fread(&retval->length, 1, 1, file)) {
				printf("Failed to read length data\n");
				free(retval);
				return NULL;
			}
			retval->count = bitface_read_count(file, retval->length);
			break;
		case ReadPCube:
			printf("Starting file reader in PCube mode.\n");
			pcube_read_header(file);
			retval->count = pcube_read_count(file);
			retval->length = pcube_read_n(file);
			break;
	}
	
	return retval;
}

void reader_destroy(Reader* reader) {
	fclose(reader->file);
	free(reader);
}

uint8_t reader_get_n(Reader* reader) {
	return reader->length;
}

uint64_t reader_get_count(Reader* reader) {
	return reader->count;
}

uint64_t reader_read_keys(Reader* reader, Key* output_keys) {
	uint64_t n_read = 0;
	
	switch (reader->mode) {
		case ReadBitFace:
			n_read = bitface_read_keys(reader->file, output_keys, reader->length, READER_MAX_COUNT);
			break;
		case ReadPCube:
			n_read = pcube_read_keys(reader->file, output_keys, READER_MAX_COUNT);
			break;
	}
	
	return n_read;
}
