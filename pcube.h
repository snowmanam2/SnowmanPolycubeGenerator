#include <stdio.h>
#include <stdint.h>
#include "key.h"

#ifndef PCUBE_H
#define PCUBE_H

void pcube_write_header(FILE* file);
void pcube_write_key(FILE* file, Key key);
void pcube_write_keys(FILE* file, Key* keys, uint64_t count);

int pcube_read_header(FILE* file);
uint64_t pcube_read_count(FILE* file);
uint8_t pcube_read_n(FILE* file);
int pcube_read_key(FILE* file, Key* key);
uint64_t pcube_read_keys(FILE* file, Key* output_keys, uint64_t count);

#endif
