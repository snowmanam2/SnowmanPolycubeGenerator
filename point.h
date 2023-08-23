#ifndef POINT_H
#define POINT_H

#include <stdint.h>

#define POINT_SPACEMAP_SIZE 32768
#define POINT_MIN 0
#define POINT_MAX 0x7FFF
#define POINT_MASK_X 0x1F
#define POINT_MASK_Y 0x3E0
#define POINT_MASK_Z 0x7C00
#define POINT_HIGH 0x8000

#define POINT_GET_X(p) ((p) & POINT_MASK_X)
#define POINT_GET_Y(p) (((p) & POINT_MASK_Y) >> 5)
#define POINT_GET_Z(p) (((p) & POINT_MASK_Z) >> 10)

#define POINT_SET_X(x) ((x))
#define POINT_SET_Y(y) ((y) << 5)
#define POINT_SET_Z(z) ((z) << 10)

typedef uint16_t Point;

typedef struct {
	uint8_t data[6];
} PointData;

PointData point_get_data(Point point, Point dimensions);

const int* point_get_offsets_lut();

Point point_get_offset(Point point, int face);

Point point_rotate(PointData point_data, uint8_t rot_num);

const uint8_t* point_get_rotation_data(uint8_t rot_num);

int point_compare(const void* a, const void* b);

Point point_from_coords(uint8_t x, uint8_t y, uint8_t z);


#endif
