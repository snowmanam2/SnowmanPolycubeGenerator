#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "bitface.h"
#include "network_sort.h"


typedef struct {
	int8_t data[3];
} RawPoint;

RawPoint raw_point_get_offset(RawPoint point, int face) {
	int n = face % 3;
	int offset = face < 3 ? 1 : -1;
	
	RawPoint retval = point;
	
	retval.data[n] += offset;
	
	return retval;
}

void write_bit(char* buffer, int bit) {
	int byte = bit >> 3;
	buffer[byte] |= 1 << (bit - (byte << 3));
}

int read_bit(char* buffer, int bit) {
	int byte = bit >> 3;
	return buffer[byte] & (1 << (bit - (byte << 3)));
}

void normalize(RawPoint* points, uint8_t length) {
	RawPoint min;
	min.data[0] = 100;
	min.data[1] = 100;
	min.data[2] = 100;
	
	for (int i = 0; i < length; i++) {
		for (int j = 0; j < 3; j++) {
			if (points[i].data[j] < min.data[j]) { 
				min.data[j] = points[i].data[j];
			}
		}
	}
	
	for (int i = 0; i < length; i++) {
		for (int j = 0; j < 3; j++) {
			points[i].data[j] += 1 - min.data[j];
		}
	}
}

// Key allocation:
// 6 bits for first point faces
// 5 bits for face bits of each remaining point
// omit last point
size_t bitface_key_size(size_t number) {

	if (number < 2) return 1;
	
	size_t bits = 6 + 5 * (number - 2);
	
	// Convert to number of bytes, and round up
	return (bits >> 3) + ((bits % 8) > 0);
	
}

const int opposite_lut[7] = { 3, 4, 5, 0, 1, 2, 8 };

int get_opposite_face(int face) {
	return opposite_lut[face];
}

// This bitface algorithm uses a bit scheme to convert points
// into face connections represented as individual bits.
//
// Each bit represents the face offset to get the next point in the list.
// Valid keys must have exactly n-1 bits set
// and the number of set bits before the current point
// must be at least the index of the point in the list
// 
// Currently this reduces space consumption to 8 bytes or less for n=13
// (Note the output contains exactly n-1 set bits with the rest left zero)
size_t bitface_pack(Key key, uint8_t length, char* buffer, uint8_t* bitface_places) {
	
	uint8_t point_index = 1;
	int point_keys[length]; // place map keys based on order of point discovery
	point_keys[0] = key.data[0];
	uint8_t faces[length]; // face numbers based on order of point discovery
	faces[0] = 6; // bogus face that will yield a bogus opposite face that we can ignore
	int ptkey = 0;
	
	int bit = 0;
	const int* offsets_lut = point_get_offsets_lut();
		
	for (uint8_t i = 1; i < length; i++) {;
		bitface_places[key.data[i]] = 1;
	}
	

	for (uint8_t i = 0; i < length - 1; i++) {
		int ptbasekey = point_keys[i];
		uint8_t o = get_opposite_face(faces[i]);
		
		for (uint8_t f = 0; f < 6; f++) {
			// Skip the face which refers to this point (saves a bit)
			if (f == o) continue;
			
			ptkey = ptbasekey + offsets_lut[f];
			
			if (!bitface_places[ptkey]) {
				bit++;
				continue;
			}
			
			bitface_places[ptkey] = 0;
			
			write_bit(buffer, bit);
			bit++;
			point_keys[point_index] = ptkey;
			faces[point_index] = f;
			point_index++;
		}
		
		if (point_index == length) break;
	}
	
	// With this last set, we have cleared the conversion_places map fully
	// meaning we don't need to set memory
	ptkey = point_keys[point_index-1];
	bitface_places[ptkey] = 0;
	
	return bit;
}


// This debitface method assumes the first point is (1,1,1)
// From there we rebuild the point list by iterating through
// all the faces as in the bitface method.
// After we have these points, we normalize and sort them
// to conform to the "key" layout used in generation
Key bitface_unpack(char* buffer, uint8_t length) {
	Key retval;
	retval.length = length;
	
	RawPoint points[length];
	points[0].data[0] = 1;
	points[0].data[1] = 1;
	points[0].data[2] = 1;
	int points_index = 1;
	int faces[length]; // face numbers based on order of point discovery
	faces[0] = 6; // bogus face that will yield a bogus opposite face that we can ignore
	int bit = 0;
	
	if (length == 1) return retval; 
	
	for (int i = 0; i < length - 1; i++) {
		int o = get_opposite_face(faces[i]);
		for (int f = 0; f < 6; f++) {
			if (f == o) continue;
			
			int result = read_bit(buffer, bit);
			bit++;
			
			if (!result) continue;
			
			points[points_index] = raw_point_get_offset(points[i], f);
			faces[points_index] = f;
			
			points_index++;
		}
	}
	
	if (points_index != length) printf("Warning: found malformed polycube\n");
	
	normalize(points, length);
	
	for (int i = 0; i < length; i++) {
		RawPoint p = points[i];
		retval.data[i] = point_from_coords(p.data[0], p.data[1], p.data[2]);
	}
	
	network_sort(retval.data, length, point_compare);
	
	return retval;
}

uint64_t bitface_read_keys(FILE* file, Key* keys, uint8_t length, uint64_t count) {
	size_t raw_size = bitface_key_size(length);
	size_t in_buf_size = count * raw_size;
	char buffer[in_buf_size];
	
	size_t read_count = 0;
	
	read_count = fread(buffer, sizeof(char), in_buf_size, file);
	
	if (read_count == 0) return 0;
	
	size_t n_read = read_count / raw_size;
	
	for (uint64_t i = 0; i < n_read; i++) {	
		keys[i] = bitface_unpack(&buffer[i * raw_size], length);
	}
	
	return n_read;
}

void bitface_write_keys(FILE* file, Key* keys, uint64_t count, uint8_t* places) {
	if (count < 1) return;
	
	uint8_t length = keys[0].length;
	size_t raw_size = bitface_key_size(length);
	
	char buffer[raw_size];
	
	for (uint64_t i = 0; i < count; i++) {
		memset(buffer, 0, raw_size);
		
		bitface_pack(keys[i], length, buffer, places);
		
		fwrite(buffer, sizeof(char), raw_size, file);
	}
}

void bitface_write_n(FILE* file, uint8_t n) {
	fwrite(&n, sizeof(uint8_t), 1, file);
}
