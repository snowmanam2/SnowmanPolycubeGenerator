#include <stdlib.h>
#include <sys/stat.h>

#include "input_stream.h"

#define CHUNK 16384

// This is loosely based on the zlib "zpipe.c" example
// https://zlib.net/zlib_how.html
// It would have been much easier to use something like gzopen / gzread
// but the uncompressed header information would get in the way
size_t input_stream_read_compressed(InputStream* s, char* buffer, size_t size) {
	size_t buffer_pos = 0;
	
	size_t available = s->out_count - s->offset;
	size_t copy_amount = available < size ? available : size;
	
	memcpy(buffer, &s->out_buffer[s->offset], copy_amount);
	s->offset += copy_amount;
	buffer_pos += copy_amount;
	
	if (copy_amount < available) return copy_amount;
	
	int eof = 0;
	size_t to_copy = size - buffer_pos;
	do {
		if (s->strm.avail_out != 0) {
			s->strm.avail_in = fread(s->in_buffer, 1, CHUNK, s->file);
			if (s->strm.avail_in == 0) {
				eof = 1;
			}
		
			s->strm.next_in = s->in_buffer;
		}
			
		s->offset = 0;
		s->strm.avail_out = CHUNK;
		s->strm.next_out = s->out_buffer;
		int result = inflate(&s->strm, Z_NO_FLUSH);
		
		switch (result) {
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
				printf("zlib DATA_ERROR\n");
				return 0;
			case Z_MEM_ERROR:
				printf("zlib MEM_ERROR\n");
				return 0;
		}
		
		s->out_count = CHUNK - s->strm.avail_out;
		available = s->out_count - s->offset;
		copy_amount = available < to_copy ? available : to_copy;
		
		memcpy(&buffer[buffer_pos], &s->out_buffer[s->offset], copy_amount);
		s->offset += copy_amount;
		buffer_pos += copy_amount;
		to_copy = size - buffer_pos;
		
	} while (to_copy > 0 && !eof);
	
	return buffer_pos;
}

InputStream* input_stream_create(char* filename) {
	FILE* file = fopen(filename, "rb");
	if (file == NULL) {
		printf("Failed to open file `%s` for reading\n", filename);
		return NULL;
	}

	InputStream* retval = calloc(1, sizeof(InputStream));
	
	retval->file = file;
	retval->compressed = 0;
	
	return retval;
}

void input_stream_destroy(InputStream* s) {
	if (s->compressed) {
		(void)inflateEnd(&s->strm);
		free(s->in_buffer);
		free(s->out_buffer);
	}
	
	fclose(s->file);
	
	free(s);
}

int input_stream_is_compressed(InputStream* s) {
	return s->compressed;
}

void input_stream_set_compressed(InputStream* s, int compressed) {
	s->compressed = compressed;
	
	if (s->compressed) {
		s->strm.zalloc = Z_NULL;
		s->strm.zfree = Z_NULL;
		s->strm.opaque = Z_NULL;
		int result = inflateInit2(&s->strm, 16+MAX_WBITS);
		if (result != Z_OK) {
			printf("zlib init failure\n");
			fflush(stdout);
			free(s);
			return;
		}
		
		s->strm.avail_out = CHUNK;
		s->strm.avail_in = 0;
		s->strm.next_in = Z_NULL;
		
		s->offset = 0;
		s->out_count = 0;
		s->in_buffer = calloc(CHUNK, sizeof(unsigned char));
		s->out_buffer = calloc(CHUNK, sizeof(unsigned char));
	}
}

long input_stream_get_offset(InputStream* stream) {
	return ftell(stream->file);
}

void input_stream_rewind(InputStream* s, long offset) {
	if (s->compressed) s->offset = 0;
	else fseek(s->file, offset, SEEK_SET);
}

size_t input_stream_read_raw(InputStream* s, void* buffer, size_t size) {
	return fread(buffer, 1, size, s->file);
}

size_t input_stream_read(InputStream* s, void* buffer, size_t size) {
	if (!s->compressed) {
		return input_stream_read_raw(s, buffer, size);
	} else {
		return input_stream_read_compressed(s, buffer, size);
	}
}

size_t input_stream_get_size(InputStream* s) {
	struct stat buf;
	int fd = fileno(s->file);
	fstat(fd, &buf);
	off_t size = buf.st_size;
	
	return size;
}
