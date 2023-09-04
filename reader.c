#include "reader.h"
#include "pcube.h"
#include "bitface.h"

Reader* reader_create(char* filename, ReaderMode mode) {

	Reader* retval = calloc(1, sizeof(Reader));
	retval->mode = mode;
	retval->count = 0;
	retval->stream = input_stream_create(filename);
	
	switch (mode) {
		case ReadBitFace:
			printf("Starting file reader in BitFace mode.\n");
			if (!input_stream_read_raw(retval->stream, &retval->length, 1)) {
				printf("Failed to read length data\n");
				free(retval);
				return NULL;
			}
			retval->count = bitface_read_count(retval->stream, retval->length);
			break;
		case ReadPCube:
			printf("Starting file reader in PCube mode");
			pcube_read_header(retval->stream);
			retval->count = pcube_read_count(retval->stream);
			retval->length = pcube_read_n(retval->stream);
			if (input_stream_is_compressed(retval->stream)) printf(" with compression");
			printf(".\n");
			break;
	}
	
	return retval;
}

void reader_destroy(Reader* reader) {
	input_stream_destroy(reader->stream);
	
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
			n_read = bitface_read_keys(reader->stream, output_keys, reader->length, READER_MAX_COUNT);
			break;
		case ReadPCube:
			n_read = pcube_read_keys(reader->stream, output_keys, READER_MAX_COUNT);
			break;
	}
	
	return n_read;
}
