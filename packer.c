#include <stdlib.h>
#include <stdint.h>

#include "packer.h"
#include "bitface.h"
#include "pcube.h"

Packer* packer_create(WriterMode mode, uint8_t* spacemap) {
	Packer* p = calloc(1, sizeof(Packer));
	p->mode = mode;
	p->spacemap = spacemap;
	
	return p;
}

void packer_destroy(Packer* p) {
	free(p);
}

uint64_t packer_pack_keys(Packer* p, void* output_data, Key* keys, uint64_t count) {
	switch (p->mode) {
		case WriteBitFace:
			return bitface_pack_keys(output_data, keys, count, p->spacemap);
		case WritePCube:
			return pcube_pack_keys(output_data, keys, count);
	}
	
	return 0;
}
