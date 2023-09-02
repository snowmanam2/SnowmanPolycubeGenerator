#include <stdio.h>
#include <stdint.h>
#include "key.h"

#ifndef BITFACE_H
#define BITFACE_H

size_t bitface_key_size(size_t length);
size_t bitface_pack(Key key, uint8_t length, char* buffer, uint8_t* places);
Key bitface_unpack(char* buffer, uint8_t length);
uint64_t bitface_read_keys(FILE* file, Key* output_keys, uint8_t length, uint64_t count);
void bitface_write_keys(FILE* file, Key* keys, uint64_t count, uint8_t* places);
void bitface_write_n(FILE* file, uint8_t n);

#endif
