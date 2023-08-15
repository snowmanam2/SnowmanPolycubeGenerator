#include "key_generator.h"

#ifndef KEY_COMPRESSION_H
#define KEY_COMPRESSION_H

void write_data(char* buffer, int* bit, uint8_t data, size_t n_bits);
uint8_t read_data(char* buffer, int* bit, size_t n_bits);
void normalize(Point* points, size_t length);

size_t raw_key_size(size_t number);
size_t compress(Key key, uint8_t length, char* buffer, uint8_t* places);
Key decompress(char* buffer, size_t length);

#endif
