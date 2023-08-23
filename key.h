#ifndef KEY_H
#define KEY_H

#include <stdint.h>
#include <stdlib.h>
#include "point.h"

typedef struct {
	Point data[30];
	uint8_t source_index;
	uint8_t length;
} Key;

Point key_get_dimensions(Key key);

Key key_get_offset(Key key, uint8_t face);

Key key_get_comparison_maximum(uint8_t length);

int key_has_larger_single_neighbor(Key key, uint8_t* places);
int key_is_connected_without(Key key, int index, uint8_t* places);
int key_has_larger_connected_source(Key key, uint8_t* places);

int key_compare(const void* a, const void* b);


#endif
