#include <stdio.h>
#include <stdint.h>
#include "key.h"
#include "input_stream.h"
#include "output_stream.h"

#ifndef BITFACE_H
#define BITFACE_H

size_t bitface_key_size(size_t length);
size_t bitface_pack(Key key, uint8_t length, char* buffer, uint8_t* places);
Key bitface_unpack(char* buffer, uint8_t length);
uint64_t bitface_read_keys(InputStream* stream, Key* output_keys, uint8_t length, uint64_t count);
uint64_t bitface_read_count(InputStream* stream, uint8_t length);

void bitface_write_keys(OutputStream* stream, Key* keys, uint64_t count, uint8_t* places);
void bitface_write_n(OutputStream* stream, uint8_t n);

#endif
