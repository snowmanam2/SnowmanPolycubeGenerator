#include "key.h"

#ifndef COMPRESSION_H
#define COMPRESSION_H

size_t compression_key_size(size_t length);
size_t compression_compress(Key key, uint8_t length, char* buffer, uint8_t* places);
Key compression_decompress(char* buffer, size_t length);

#endif
