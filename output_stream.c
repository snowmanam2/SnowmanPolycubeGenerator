#include <stdlib.h>

#include "output_stream.h"

#define CHUNK 16384

// Inner loop of the write method
// This is where the zlib work is actually happening
// We need to call this at the end to write the end of the file
void output_stream_write_inner(OutputStream* s, int flush) {
	s->strm.avail_in = s->in_count;
	s->strm.next_in = s->in_buffer;
	
	do {
		s->strm.avail_out = CHUNK;
		s->strm.next_out = s->out_buffer;
		int result = deflate(&s->strm, flush);
		
		if (result == Z_STREAM_ERROR) {
			printf("zlib STREAM_ERROR\n");
			return;
		}
		
		size_t to_write = CHUNK - s->strm.avail_out;
		fwrite(s->out_buffer, 1, to_write, s->file);
			
	} while (s->strm.avail_out == 0);
	
	s->in_count = 0;
}

// This is loosely based on the zlib "zpipe.c" example
// https://zlib.net/zlib_how.html
// It would have been much easier to use something like gzopen / gzwrite
// but the uncompressed header information would get in the way
void output_stream_write_compressed(OutputStream* s, char* buffer, size_t size) {
	size_t buffer_pos = 0;
	
	size_t available = CHUNK - s->in_count;
	size_t copy_amount = available < size ? available : size;
	
	memcpy(&s->in_buffer[s->in_count], &buffer[buffer_pos], copy_amount);
	buffer_pos += copy_amount;
	s->in_count += copy_amount;
	
	if (copy_amount < available) return;
	
	size_t to_copy = size - buffer_pos;
	do {
		output_stream_write_inner(s, Z_NO_FLUSH);
		
		available = CHUNK - s->in_count;
		copy_amount = available < to_copy ? available : to_copy;
		memcpy(&s->in_buffer[s->in_count], &buffer[buffer_pos], copy_amount);
		
		buffer_pos += copy_amount;
		s->in_count += copy_amount;
		
	} while (buffer_pos < size);
	
}

OutputStream* output_stream_create(char* filename, int compressed) {
	FILE* file = fopen(filename, "wb");
	if (file == NULL) {
		printf("Failed to open file `%s` for writing\n", filename);
		return NULL;
	}

	OutputStream* retval = calloc(1, sizeof(OutputStream));
	
	retval->file = file;
	retval->compressed = compressed;
	
	if (compressed) {
		retval->strm.zalloc = Z_NULL;
		retval->strm.zfree = Z_NULL;
		retval->strm.opaque = Z_NULL;
		int result = deflateInit2(&retval->strm, 2, Z_DEFLATED,
					15 | 16, 9, Z_DEFAULT_STRATEGY);
		if (result != Z_OK) {
			printf("zlib init failure\n");
			fflush(stdout);
			free(retval);
			return NULL;
		}
		
		retval->strm.avail_out = CHUNK;
		retval->strm.avail_in = 0;
		retval->strm.next_in = Z_NULL;
		
		retval->in_count = 0;
		retval->in_buffer = calloc(CHUNK, sizeof(unsigned char));
		retval->out_buffer = calloc(CHUNK, sizeof(unsigned char));
	}
	
	return retval;
}

void output_stream_destroy(OutputStream* s) {
	if (s->compressed) {
		(void)deflateEnd(&s->strm);
		free(s->in_buffer);
		free(s->out_buffer);
	}
	
	fclose(s->file);
	
	free(s);
}

void output_stream_flush(OutputStream* s) {
	if (s->compressed) output_stream_write_inner(s, Z_FINISH);
}

void output_stream_seek(OutputStream* s, long offset) {
	fseek(s->file, offset, SEEK_SET);
}

void output_stream_write_raw(OutputStream* s, void* buffer, size_t size) {
	fwrite(buffer, sizeof(char), size, s->file);
}

void output_stream_write(OutputStream* s, void* buffer, size_t size) {
	if (!s->compressed) {
		output_stream_write_raw(s, buffer, size);
	} else {
		output_stream_write_compressed(s, buffer, size);
	}
}

off_t output_stream_get_offset(OutputStream* s) {
	return ftello(s->file);
}

int output_stream_get_fd(OutputStream* s) {
	return fileno(s->file);
}
