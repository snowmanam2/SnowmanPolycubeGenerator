#include <stdio.h>
#include <stdint.h>
#include "key.h"
#include "input_stream.h"
#include "output_stream.h"

#ifndef PCUBE_H
#define PCUBE_H

void pcube_write_header(OutputStream* stream, uint8_t compressed);
void pcube_write_count(OutputStream* stream, uint64_t count);
void pcube_write_key(OutputStream* stream, Key key);
void pcube_write_keys(OutputStream* stream, Key* keys, uint64_t count);
uint64_t pcube_pack_key(void* data, Key key);
uint64_t pcube_pack_keys(void* data, Key* keys, uint64_t count);

int pcube_read_header(InputStream* stream);
uint64_t pcube_read_count(InputStream* stream);
uint8_t pcube_read_n(InputStream* stream);
int pcube_read_key(InputStream* stream, Key* key);
uint64_t pcube_read_keys(InputStream* stream, Key* output_keys, uint64_t count);

#endif
