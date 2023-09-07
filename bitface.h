#include <stdio.h>
#include <stdint.h>
#include "key.h"
#include "input_stream.h"
#include "output_stream.h"

#ifndef BITFACE_H
#define BITFACE_H

size_t bitface_key_size(size_t length);
Key bitface_unpack_key(void* raw_data, uint8_t length);
uint64_t bitface_read_keys(InputStream* stream, Key* output_keys, uint8_t length, uint64_t count);
uint64_t bitface_read_count(InputStream* stream, uint8_t length);

void bitface_write_keys(OutputStream* stream, Key* keys, uint64_t count, uint8_t* places);
void bitface_write_n(OutputStream* stream, uint8_t n);
uint64_t bitface_pack_key(void* raw_data, Key key, uint8_t* places);
uint64_t bitface_pack_keys(void* raw_data, Key* key, uint64_t count, uint8_t* places);

#endif
