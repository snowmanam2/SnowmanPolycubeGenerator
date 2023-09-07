#include "writer.h"
#include "key.h"

#ifndef PACKER_H
#define PACKER_H

typedef struct {
	uint8_t* spacemap;
	WriterMode mode;
} Packer;

Packer* packer_create(WriterMode mode, uint8_t* spacemap);
void packer_destroy(Packer* p);

uint64_t packer_pack_keys(Packer* p, void* output_data, Key* keys, uint64_t count);

#endif
