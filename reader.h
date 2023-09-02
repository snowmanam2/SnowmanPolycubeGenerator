#include <stdio.h>
#include <stdint.h>

#include "key.h"

#ifndef READER_H
#define READER_H

#define READER_MAX_COUNT 10000

typedef enum {ReadBitFace, ReadPCube} ReaderMode;

typedef struct {
	FILE* file;
	ReaderMode mode;
	uint8_t length;
} Reader;

Reader* reader_create(char* filename, ReaderMode mode);
void reader_destroy(Reader* reader);

uint8_t reader_get_n(Reader* reader);

uint64_t reader_read_keys(Reader* reader, Key* output_keys);

#endif
