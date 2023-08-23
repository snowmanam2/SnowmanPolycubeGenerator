#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdint.h>

#include "point.h"
#include "key.h"

Point get_expand_dim(Point dim, int face);
int get_expand_face(Point point, Point dim);
uint32_t generator_create_rotations(Key key, size_t length, Point dim, Key* rkeys);
int generator_generate(Key key, size_t new_length, Key* output);

int key_has_larger_single_neighbor(Key key, uint8_t* places);
int key_is_connected_without(Key key, int index, uint8_t* places);
int key_has_larger_connected_source(Key key, uint8_t* places);

#endif
