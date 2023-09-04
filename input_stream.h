#include <stdio.h>
#include <string.h>
#include <zlib.h>

#ifndef INPUT_STREAM_H
#define INPUT_STREAM_H

typedef struct {
	FILE* file;
	int compressed;
	z_stream strm;
	unsigned char* in_buffer;
	unsigned char* out_buffer;
	size_t out_count;
	int offset;
} InputStream;

InputStream* input_stream_create(char* filename);
void input_stream_destroy(InputStream* stream);

int input_stream_is_compressed(InputStream* stream);
void input_stream_set_compressed(InputStream* stream, int compressed);

long input_stream_get_offset(InputStream* stream);
void input_stream_rewind(InputStream* stream, long offset);
size_t input_stream_read(InputStream* stream, void* buffer, size_t size);
size_t input_stream_read_raw(InputStream* stream, void* buffer, size_t size);

size_t input_stream_get_size(InputStream* stream);

#endif
