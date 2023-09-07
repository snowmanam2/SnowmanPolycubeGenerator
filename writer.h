#include <stdio.h>
#include <stdint.h>

#include "key.h"
#include "output_stream.h"

#ifndef WRITER_H
#define WRITER_H

typedef enum {WriteBitFace, WritePCube} WriterMode;

typedef struct {
	OutputStream* stream;
	WriterMode mode;
	uint8_t* spacemap;
} Writer;

Writer* writer_create(char* filename, WriterMode mode, uint8_t length, int compressed);
void writer_destroy(Writer* writer);

void writer_write_keys(Writer* writer, Key* output_keys, uint64_t count);
void writer_write_count(Writer* writer, uint64_t count);

off_t writer_get_offset(Writer* writer);
int writer_get_fd(Writer* writer);
WriterMode writer_get_mode(Writer* writer);

#endif
