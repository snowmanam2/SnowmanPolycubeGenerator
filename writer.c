#include "writer.h"
#include "key.h"
#include "bitface.h"
#include "pcube.h"

Writer* writer_create(char* filename, WriterMode mode, uint8_t length) {
	FILE* file = fopen(filename, "wb");
	if (file == NULL) {
		printf("Failed to open file `%s` for writing\n", filename);
		return NULL;
	}
	
	Writer* retval = calloc(1, sizeof(Writer));
	
	retval->file = file;
	retval->mode = mode;
	retval->spacemap = calloc(POINT_SPACEMAP_SIZE, sizeof(uint8_t));
	
	switch(mode) {
		case WriteBitFace:
			printf("Starting file writer in BitFace mode.\n");
			bitface_write_n(file, length);
			break;
		case WritePCube:
			printf("Starting file writer in PCube mode.\n");
			pcube_write_header(file);
			break;
	}
	
	return retval;
}

void writer_destroy(Writer* writer) {
	fclose(writer->file);
	free(writer->spacemap);
	
	free(writer);
}

void writer_write_keys(Writer* writer, Key* keys, uint64_t count) {
	switch (writer->mode) {
		case WriteBitFace:
			bitface_write_keys(writer->file, keys, count, writer->spacemap);
			break;
		case WritePCube:
			pcube_write_keys(writer->file, keys, count);
			break;
	}
}

void writer_write_count(Writer* writer, uint64_t count) {
	switch (writer->mode) {
		case WriteBitFace:
			break;
		case WritePCube:
			pcube_write_count(writer->file, count);
			break;
	}
}