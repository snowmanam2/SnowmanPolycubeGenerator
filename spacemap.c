#include "spacemap.h"

#define SPACEMAP_SIZE 32768

uint8_t* spacemap_create() {
	return calloc(SPACEMAP_SIZE, sizeof(uint8_t));
}

void spacemap_destroy(uint8_t* map) {
	free(map);
}

const int spacemap_offsets_lut[6] = { 1, 32, 1024, -1, -32, -1024};

const int* spacemap_get_offsets() {
	return spacemap_offsets_lut;
}

int spacemap_get_key(Point pt) {
	return (pt.data[0]) + (pt.data[1] << 5) + (pt.data[2] << 10);
}

