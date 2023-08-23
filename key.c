#include <string.h>

#include "key.h"

// This function captures the dimension size by finding
// the maximum component in every direction of the key
Point key_get_dimensions(Key key) {
	Point* k = key.data;
	Point d[3];
	d[0] = 0;
	d[1] = 0;
	d[2] = 0;
	
	Point mask[3];
	mask[0] = POINT_MASK_X;
	mask[1] = POINT_MASK_Y;
	mask[2] = POINT_MASK_Z;
	
	for (int i = 0; i < key.length; i++) {
		for (int j = 0; j < 3; j++) {
			Point value = k[i] & mask[j];
			if (value > d[j]) {
				d[j] = value;
			}
		}
	}
	
	return d[0] + d[1] + d[2];
}

// Keys need to be adjusted if using higher face numbers,
// which correspond to point coordinates outside the region
Key key_get_offset(Key key, uint8_t face) {
	if (face < 3) return key;
	
	Key retval = key;
	
	for (size_t i = 0; i < key.length; i++) {
		retval.data[i] = point_get_offset(key.data[i], face - 3);
	}
	
	return retval;
}

// Returns the maximum possible key for the current length
// Used for initial comparison when comparing rotations
Key key_get_comparison_maximum(uint8_t length) {
	Point p = POINT_MAX;
	
	Key retval;
	retval.length = length;
	
	for (size_t i = 0; i < length; i++) {
		retval.data[i] = p;
	}
	
	return retval;
}


// This is the faster function to determine if a key was generated
// from the maximum possible added point ("source_index").
// Points with single neighbors are guaranteed to not be considered
// "cut points" of the polycube.
// If this function finds such a point, the result will be thrown out
int key_has_larger_single_neighbor(Key key, uint8_t* places) {
	int retval = 0;
	const int* offsets_lut = point_get_offsets_lut();
	
	for (uint8_t i = 0; i < key.length; i++) {
		places[key.data[i]] = 1;
	}
	
	for (uint8_t i = key.length - 1; i > key.source_index; i--) {
		uint8_t count = 0;
		int ptbasekey = key.data[i];
		
		for (uint8_t f = 0; f < 6; f++) {
			count += places[ptbasekey + offsets_lut[f]];
		}
		
		if (count == 1) {
			retval = 1;
			break;
		}
	}
	
	for (uint8_t i = 0; i < key.length; i++) {
		places[key.data[i]] = 0;
	}
	
	return retval;
}

// This is the slower function to determine if a key was generated
// from the maximum possible added point ("source_index").
// This function will run for every candidate alternative "source_index"
// and tries to traverse the entire cube.
// If it fails to find all points, the caller will move to the next
// alternative "source_index".
int key_is_connected_without(Key key, int index, uint8_t* places) {
	int source_length = key.length - 1;
	const int* offsets_lut = point_get_offsets_lut();
	
	uint8_t point_index = 1;
	int point_keys[source_length];
	point_keys[0] = key.data[0];
	int ptkey = 0;
	
	for (uint8_t i = 1; i < key.length; i++) {
		places[key.data[i]] = 1;
	}
	
	places[key.data[index]] = 0;
	
	for (uint8_t i = 0; i < source_length - 1; i++) {
		
		if (i >= point_index) break;
		
		int ptbasekey = point_keys[i];
		
		for (uint8_t f = 0; f < 6; f++) {
			
			ptkey = ptbasekey + offsets_lut[f];
			
			if (!places[ptkey]) {
				continue;
			}
			
			places[ptkey] = 0;
			
			point_keys[point_index] = ptkey;
			point_index++;
		}
		
		if (point_index >= source_length) break;
	}
	
	// In case we don't fully traverse the cube, we need to make sure
	// the map is reset for the next check
	for (uint8_t i = 0; i < key.length; i++) {
		places[key.data[i]] = 0;
	}
	
	return point_index >= source_length;
}

// Parent slower function to check for candidate alternative "source_index" points.
// This might not be the most efficient algorithm (such as graph theory methods), but the faster
// single neighbor check removes most of the slowness from this process
int key_has_larger_connected_source(Key key, uint8_t* places) {
	int retval = 0;
	
	for (uint8_t i = key.length - 1; i > key.source_index; i--) {
		if (key_is_connected_without(key, i, places)) {
			retval = 1;
			break;
		}
	}
		
	return retval;
}

int key_compare(const void* a, const void* b) {
	Key* ak = (Key*) a;
	Key* bk = (Key*) b;
	
	return memcmp(ak->data, bk->data, ak->length << 1);
}

