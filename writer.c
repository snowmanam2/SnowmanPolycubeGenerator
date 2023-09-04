#include "writer.h"
#include "key.h"
#include "bitface.h"
#include "pcube.h"

Writer* writer_create(char* filename, WriterMode mode, uint8_t length, int compressed) {
	Writer* retval = calloc(1, sizeof(Writer));
	
	retval->stream = output_stream_create(filename, compressed);
	retval->mode = mode;
	retval->spacemap = calloc(POINT_SPACEMAP_SIZE, sizeof(uint8_t));
	
	switch(mode) {
		case WriteBitFace:
			printf("Starting file writer in BitFace mode.\n");
			if (compressed) printf("Compression not implemented in BitFace mode\n");
			bitface_write_n(retval->stream, length);
			break;
		case WritePCube:
			printf("Starting file writer in PCube mode");
			if (compressed) printf(" with compression");
			printf(".\n");
			pcube_write_header(retval->stream, compressed);
			break;
	}
	
	return retval;
}

void writer_destroy(Writer* writer) {
	output_stream_flush(writer->stream);
	output_stream_destroy(writer->stream);
	free(writer->spacemap);
	
	free(writer);
}

void writer_write_keys(Writer* writer, Key* keys, uint64_t count) {
	switch (writer->mode) {
		case WriteBitFace:
			bitface_write_keys(writer->stream, keys, count, writer->spacemap);
			break;
		case WritePCube:
			pcube_write_keys(writer->stream, keys, count);
			break;
	}
}

void writer_write_count(Writer* writer, uint64_t count) {
	switch (writer->mode) {
		case WriteBitFace:
			break;
		case WritePCube:
			output_stream_flush(writer->stream);
			pcube_write_count(writer->stream, count);
			break;
	}
}
