#include <stdio.h>
#include <string.h>
#include <zlib.h>

#ifndef OUTPUT_STREAM_H
#define OUTPUT_STREAM_H

typedef struct {
	FILE* file;
	int compressed;
	z_stream strm;
	unsigned char* in_buffer;
	unsigned char* out_buffer;
	size_t in_count;
} OutputStream;

OutputStream* output_stream_create(char* filename, int compressed);
void output_stream_destroy(OutputStream* stream);

void output_stream_flush(OutputStream* stream);
void output_stream_seek(OutputStream* stream, long offset);
void output_stream_write_raw(OutputStream* stream, void* buffer, size_t size);
void output_stream_write(OutputStream* stream, void* buffer, size_t size);

off_t output_stream_get_offset(OutputStream* stream);
int output_stream_get_fd(OutputStream* stream);

#endif
