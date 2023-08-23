#include <stdio.h>

#include "generator.h"
#include "network_sort.h"
#include "point.h"
#include "key.h"

#define NUM_ROTATIONS 24
#define ROT_ALL 0xFFFFFF
#define ROT_X   0xFF
#define ROT_Y   0xFF00
#define ROT_Z   0xFF0000
#define ROT_XY  0xF
#define ROT_XZ  0xF0
#define ROT_YX  0xF00
#define ROT_YZ  0xF000
#define ROT_ZX  0xF0000
#define ROT_ZY  0xF00000

// This function determines the new dimensions of the expanded region
// Higher face IDs still increase the region size, so we use % 3
// to capture the dimension
// We add to the proper 5 bit field by bit shifting the added size
Point get_expand_dimensions(Point dim, int face) {
	return point_get_offset(dim, face % 3);
}

// This function determines if a point is outside of the current dimensions.
// The returned face is the direction in which the region will expand
int get_expand_face(Point point, Point dim) {
	Point mask[3];
	mask[0] = POINT_MASK_X;
	mask[1] = POINT_MASK_Y;
	mask[2] = POINT_MASK_Z;
	
	for (int i = 0; i < 3; i++) {
		if ((point & mask[i]) == 0) return i + 3;
		if ((point & mask[i]) > (dim & mask[i])) return i;
	}
	
	return -1;
}

void merge_candidates(Point* dest, Point* a, int n_a, Point* b, int n_b) {
	int ia = 0;
	int ib = 0;
	
	Point va = a[0];
	Point vb = b[0];
	
	for (int i = 0; i < n_a + n_b; i++) {
		if (va < vb) {
			dest[i] = va;
			ia++;
			
			va = ia < n_a ? a[ia] : POINT_MAX;
		} else {
			dest[i] = vb;
			ib++;
			
			vb = ib < n_b ? b[ib] : POINT_MAX;
		}
	}
	
}

void generator_create_candidates(Key key, size_t length, Point* candidates) {
	int len2 = 2 * length;
	int len4 = 4 * length;
	int len8 = 8 * length;
	Point buffer[len8];
	Point* b_addr[8];
	Point* c_addr[6];
	
	int j = 0;
	for (int i = 0; i < 8; i ++) {
		b_addr[i] = &buffer[j];
		j += length;
	}
	
	int index = 0;
	for (int face = 0; face < 6; face++) {
		c_addr[face] = &candidates[index];
		for (size_t i = 0; i < length; i++) {
			candidates[index] = point_get_offset(key.data[i], face);
			index++;
		}
	}
	
	merge_candidates(b_addr[0], c_addr[0], length, c_addr[1], length);
	merge_candidates(b_addr[2], c_addr[2], length, c_addr[3], length);
	merge_candidates(b_addr[4], b_addr[0],   len2, b_addr[2],   len2);
	merge_candidates(b_addr[0], c_addr[4], length, c_addr[5], length);
	merge_candidates(c_addr[0], b_addr[0],   len2, b_addr[4],   len4);
	
}

// Generates relevant rotation keys for the given starting key
// Returns a mask to test for which rotations were computed
// We eliminate some rotations by looking at the dimensions
// If we have a single maximum dimension, we use the 8 rotations from that axis
// Else if we have a single minimum dimension, we use the 8 rotations from that axis
// Eliminate further rotations by checking if other dimensions are not equal
// Otherwise we compute all rotations
uint32_t generator_create_rotations(Key key, size_t length, Point dim, Key* rkeys) {
	uint8_t d0 = POINT_GET_X(dim);
	uint8_t d1 = POINT_GET_Y(dim);
	uint8_t d2 = POINT_GET_Z(dim);
	uint32_t rotation_bits = 0;
	
	Point bigpoint = POINT_MAX; // 32,32,32
	
	if ((d0 == d1) && (d0 == d2)) {
		rotation_bits = ROT_ALL;
	} else if (d1 == d2) {
		rotation_bits = ROT_X;
	} else if (d0 == d2) {
		rotation_bits = ROT_Y;
	} else if (d0 == d1) {
		rotation_bits = ROT_Z;
	} else if (d0 > d1 && d0 > d2) {
		if (d1 > d2) rotation_bits = ROT_XY;
		else rotation_bits = ROT_XZ;
	} else if (d1 > d0 && d1 > d2) {
		if (d0 > d2) rotation_bits = ROT_YX;
		else rotation_bits = ROT_YZ;
	} else {
		if (d0 > d1) rotation_bits = ROT_ZX;
		else rotation_bits = ROT_ZY;
	}
	
	PointData pdata[length];
	
	for (size_t i = 0; i < length; i++) {
		pdata[i] = point_get_data(key.data[i], dim);
	}
	
	for (size_t i = 0; i < NUM_ROTATIONS; i++) {
		if (!(rotation_bits & (1 << i))) continue;
		
		Point* kdata = rkeys[i].data;
		
		const uint8_t* r = point_get_rotation_data(i);
		uint8_t r0 = r[0];
		uint8_t r1 = r[1];
		uint8_t r2 = r[2];
		
		for (size_t j = 0; j < length; j++) {
			uint8_t* p = pdata[j].data;
			kdata[j] = POINT_SET_X(p[r0]) + POINT_SET_Y(p[r1]) + POINT_SET_Z(p[r2]);
		}
		
		network_sort(kdata, length, point_compare);
		
		kdata[length] = bigpoint;
		rkeys[i].source_index = 0;
		rkeys[i].length = length;
		
	}
	
	return rotation_bits;
}

int generator_generate(Key key, size_t new_length, Key* output) {
	int output_index = 0;
	size_t old_length = new_length - 1;
	int n_candidates = old_length * 6;
	Point candidates[n_candidates];
	
	// Generate initial candidates from existing key faces
	generator_create_candidates(key, old_length, candidates);
	
	// Generate all relevant rotated keys for inside dimensions
	Point dimensions = key_get_dimensions(key);
	Key rotations_inside[NUM_ROTATIONS];
	uint32_t rotations_inside_mask = generator_create_rotations(key, old_length, dimensions, rotations_inside);
	
	// Generate all relevant rotated keys for the "plus" and "minus" dimension expansion regions
	// We are guaranteed to use these keys because we have at least one valid candidate on each region
	Key rotations_expand[6][NUM_ROTATIONS];
	uint32_t rotations_expand_mask[6];
	for (int i = 0; i < 6; i++) {
		Key offset_key = key_get_offset(key, i);
		Point expand_dim = get_expand_dimensions(dimensions, i);
		rotations_expand_mask[i] = generator_create_rotations(offset_key, old_length, expand_dim, rotations_expand[i]);
	}
	
	// Iterate through candidates:
	int key_index = 0;
	Point last = POINT_MIN; // init to 0,0,0 because no candidate will ever equal that
	Key max_key = key_get_comparison_maximum(new_length);
	for (size_t i = 0; i < n_candidates; i++) {
		Point candidate = candidates[i];
		
		// Skip if duplicate from last
		if (last == candidate) continue;
		
		// If candidate is greater than current key index point, increment key index point
		if (key_index < old_length) {
			while(candidate > key.data[key_index] && key_index < old_length) {
				key_index++;
			}
			
			// keep track if the key_index exceeds the usable portion of the key data
			int over = key_index >= old_length;
			
			// Skip if candidate equals current key index point (overlapping point)
			// Ignore this check if we are over the usable portion of the key data
			if(candidate == key.data[key_index] && !over) continue;
		}
		
		// Begin processing candidate
		last = candidate;
		int face = get_expand_face(candidate, dimensions);
		
		uint32_t candidate_rotations_mask;
		Key* candidate_rotations;
		Point candidate_dim;
		
		if (face == -1) {
			candidate_rotations_mask = rotations_inside_mask;
			candidate_rotations = rotations_inside;
			candidate_dim = dimensions;
		} else {
			candidate_rotations_mask = rotations_expand_mask[face];
			candidate_rotations = rotations_expand[face];
			candidate_dim = get_expand_dimensions(dimensions, face);
			
			// If this is a zero indexed face, we need to increase the candidate index 
			// to match the bounds (minimum of 1 in all dimensions)
			if (face > 2) {
				candidate = point_get_offset(candidate, face - 3);
			}
		}
		
		// For each rotated key:
		Key min_key = max_key;
		Key current_key;
		current_key.length = new_length;
		PointData data = point_get_data(candidate, candidate_dim);
		for (int j = 0; j < NUM_ROTATIONS; j++) {
			if (!(candidate_rotations_mask & (1 << j))) continue;
			
			// Generate candidate rotated to relevant rotation
			Point candidate_rotated = point_rotate(data, j);
			size_t k = 0;
			int placed = 0;
			int found = 0;
			int greater = 0;
			Key candidate_rotation = candidate_rotations[j];
			
			// Check if rotation key with candidate will be the minimum new rotated key
			// Merge the candidate into the sorted point stream in the process
			for(size_t n = 0; n < new_length; n++) {
				Point next = candidate_rotation.data[k];
				size_t inc_k = 1;
				if (!placed) {
					if (candidate_rotated < next || k == old_length) {
						next = candidate_rotated;
						placed = 1;
						inc_k = 0;
						current_key.source_index = n;
					}
				}
				
				int result = (next - min_key.data[n]);
				
				if (result > 0 && !found) {
					greater = 1;
					break;
				}
				
				found = found || (result < 0);
				
				current_key.data[n] = next;
				k += inc_k;
			}
			
			// Keep equal keys with higher source_index values
			// Required for making sure we have the proper output keys
			if (found || (!greater && current_key.source_index > min_key.source_index)) {
				min_key = current_key;
			}
		}
		
		// Return minimum new rotated key
		output[output_index] = min_key;
		output_index++;
	}
	
	return output_index;
}




