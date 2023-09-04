#include <stdio.h>
#include <stdint.h>

#include "key.h"
#include "input_stream.h"

#ifndef READER_H
#define READER_H

#define READER_MAX_COUNT 10000

typedef enum {ReadBitFace, ReadPCube} ReaderMode;

typedef struct {
	InputStream* stream;
	ReaderMode mode;
	uint8_t length;
	uint64_t count;
} Reader;

Reader* reader_create(char* filename, ReaderMode mode);
void reader_destroy(Reader* reader);

uint8_t reader_get_n(Reader* reader);
uint64_t reader_get_count(Reader* reader);
uint64_t reader_read_keys(Reader* reader, Key* output_keys);

#endif
