#ifndef KEY_GENERATOR_H
#define KEY_GENERATOR_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>



typedef union {
	int8_t data[4];
	int32_t value;
} Point;

typedef struct {
	int8_t data[6];
} PointData;

typedef struct {
	Point data[30];
	uint8_t source_index;
	uint8_t length;
} Key;

int cmp_keys(const void* a, const void* b);
PointData get_point_data(Point point, Point dimensions);
Point get_point_offset(Point point, int face);
Point rotate(PointData point_data, uint8_t rot_num);
Point get_dim(Key key, size_t length);
int cmp_points(const void* a, const void* b);
Key get_offset_key(Key key, size_t length, int face);
Key get_maximum_key(size_t length);
Point get_expand_dim(Point dim, int face);
int get_expand_face(Point point, Point dim);
uint32_t create_rotation_keys(Key key, size_t length, Point dim, Key* rkeys);
int generate(Key key, size_t new_length, Key* output);

int has_larger_single_neighbor(Key key, uint8_t* places);
int is_connected_without(Key key, int index, uint8_t* places);
int has_larger_connected_source(Key key, uint8_t* places);

#endif
