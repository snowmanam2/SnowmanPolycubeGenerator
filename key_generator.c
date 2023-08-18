#include "key_generator.h"
#include "network_sort.h"
#include "spacemap.h"

#define NUM_ROTATIONS 24

const uint8_t rotations_lut[][3] = {
	{0,1,2}, // 0 x
	{0,5,1},
	{0,4,5},
	{0,2,4},
	{3,1,5}, // 4 x flip
	{3,2,1},
	{3,4,2},
	{3,5,4},
	{1,0,5}, // 8 y
	{1,2,0},
	{1,3,2},
	{1,5,3},
	{4,0,2}, // 12 y flip
	{4,5,0},
	{4,3,5},
	{4,2,3},
	{2,0,1}, // 16 z
	{2,4,0},
	{2,3,4},
	{2,1,3},
	{5,0,4}, // 20 z flip
	{5,1,0},
	{5,3,1},
	{5,4,3}
};

// This "PointData" includes the original point
// plus the additional 3 components dependent on the dimensions
// that would appear when reflected in rotations
PointData get_point_data(Point point, Point dimensions) {
	PointData retval;
	int8_t* p = point.data;
	int8_t* d = dimensions.data;
	
	retval.data[0] = p[0];
	retval.data[1] = p[1];
	retval.data[2] = p[2];
	
	retval.data[3] = d[0]-p[0]+1;
	retval.data[4] = d[1]-p[1]+1;
	retval.data[5] = d[2]-p[2]+1;
	
	return retval;
}

const uint8_t index_lut[6] = {0,  1,  2,  0,  1,  2};
const int8_t offset_lut[6] = {1,  1,  1, -1, -1, -1};

// Offsets the point toward the relevant face ID
Point get_point_offset(Point point, int face) {
	Point retval = point;
	
	retval.data[index_lut[face]] += offset_lut[face];
	
	return retval;
}

// Use the lookup table to build the relevant rotation components
// Components 3 to 5 are reflections of components 0 to 2
Point rotate(PointData point_data, uint8_t rot_num) {
	Point retval;
	int8_t* p = point_data.data;
	const uint8_t* r = rotations_lut[rot_num];
	
	retval.data[0] = p[r[0]];
	retval.data[1] = p[r[1]];
	retval.data[2] = p[r[2]];
	retval.data[3] = 0;
	
	return retval;
}

// This function captures the dimension size by finding
// the maximum component in every direction of the key
Point get_dim(Key key, size_t length) {
	Point retval;
	Point* k = key.data;
	int8_t* d = retval.data;
	
	d[0] = 0;
	d[1] = 0;
	d[2] = 0;
	d[3] = 0;
	
	for (size_t i = 0; i < length; i++) {
		for (int j = 0; j < 3; j++) {
			if (k[i].data[j] > d[j]) {
				d[j] = k[i].data[j];
			}
		}
	}
	
	return retval;
}

int cmp_keys(const void* a, const void* b) {
	Key* ak = (Key*) a;
	Key* bk = (Key*) b;
	
	return memcmp(ak->data, bk->data, ak->length << 2);
}

int cmp_points(const void* a, const void* b) {
	Point* ap = (Point*) a;
	Point* bp = (Point*) b;
	
	return ap->value - bp->value;
}


// Keys need to be adjusted if using higher face numbers,
// which correspond to point coordinates outside the region
Key get_offset_key(Key key, size_t length, int face) {
	Key retval = key;
	
	if (face < 3) return key;
	
	for (size_t i = 0; i < length; i++) {
		retval.data[i] = get_point_offset(key.data[i], face - 3);
	}
	
	return retval;
}

// Returns the maximum possible key for the current length
// Used for initial comparison
Key get_maximum_key(size_t length) {
	Point p;
	p.data[0] = 32;
	p.data[1] = 32;
	p.data[2] = 32;
	p.data[3] = 0;
	
	Key retval;
	
	for (size_t i = 0; i < length + 1; i++) {
		retval.data[i] = p;
	}
	
	retval.length = length;
	retval.source_index = 0;
	
	return retval;
}

// This function determines the new dimensions of the expanded region
// Higher face IDs still increase the region size, so we use % 3
// to capture the dimension
Point get_expand_dim(Point dim, int face) {
	Point retval = dim;
	
	retval.data[face % 3] += 1;
	
	return retval;
}

// This function determines if a point is outside of the current dimensions.
// The returned face is the direction in which the region will expand
int get_expand_face(Point point, Point dim) {
	for (int i = 0; i < 3; i++) {
		if (point.data[i] == 0) return i + 3;
		if (point.data[i] > dim.data[i]) return i;
	}
	
	return -1;
}

// Generates relevant rotation keys for the given starting key
// Returns a mask to test for which rotations were computed
// We eliminate some rotations by looking at the dimensions
// If we have a single maximum dimension, we use the 8 rotations from that axis
// Else if we have a single minimum dimension, we use the 8 rotations from that axis
// Otherwise we compute all rotations
uint32_t create_rotation_keys(Key key, size_t length, Point dim, Key* rkeys) {
	int8_t* d = dim.data;
	uint32_t rotation_bits = 0;
	
	Point bigpoint;
	bigpoint.data[0] = 100;
	bigpoint.data[1] = 100;
	bigpoint.data[2] = 100;
	bigpoint.data[3] = 0;
	
	if ((d[0] == d[1]) && (d[0] == d[2])) {
		rotation_bits = 0xFFFFFF;
	} else if ((d[0] > d[1] && d[0] > d[2]) || d[1] == d[2]) {
		rotation_bits = 0xFF;
	} else if ((d[1] > d[0] && d[1] > d[2]) || d[0] == d[2]) {
		rotation_bits = 0xFF00;
	} else {
		rotation_bits = 0xFF0000;
	}
	
	PointData pdata[length];
	
	for (size_t i = 0; i < length; i++) {
		pdata[i] = get_point_data(key.data[i], dim);
	}
	
	for (size_t i = 0; i < NUM_ROTATIONS; i++) {
		if (!(rotation_bits & (1 << i))) continue;
		
		Point* kdata = rkeys[i].data;
		
		for (size_t j = 0; j < length; j++) {
			kdata[j] = rotate(pdata[j], i);
		}
		
		network_sort((int*)kdata, length, cmp_points);
		
		kdata[length] = bigpoint;
		rkeys[i].source_index = 0;
	}
	
	return rotation_bits;
}

int generate(Key key, size_t new_length, Key* output) {
	int output_index = 0;
	size_t old_length = new_length - 1;
	int n_candidates = old_length * 6;
	Point candidates[n_candidates];
	
	// Generate initial candidates from existing key faces
	int index = 0;
	for (size_t i = 0; i < old_length; i++) {
		for (int face = 0; face < 6; face++) {
			candidates[index] = get_point_offset(key.data[i], face);
			index++;
		}
	}
	
	// Sort candidates to allow for duplicate removal later
	qsort(candidates, n_candidates, sizeof(Point), cmp_points);
	
	// Generate all relevant rotated keys for inside dimensions
	Point dimensions = get_dim(key, old_length);
	Key rotations_inside[NUM_ROTATIONS];
	uint32_t rotations_inside_mask = create_rotation_keys(key, old_length, dimensions, rotations_inside);
	
	// Generate all relevant rotated keys for the "plus" and "minus" dimension expansion regions
	// We are guaranteed to use these keys because we have at least one valid candidate on each region
	Key rotations_expand[6][NUM_ROTATIONS];
	uint32_t rotations_expand_mask[6];
	for (int i = 0; i < 6; i++) {
		Key offset_key = get_offset_key(key, old_length, i);
		Point expand_dim = get_expand_dim(dimensions, i);
		rotations_expand_mask[i] = create_rotation_keys(offset_key, old_length, expand_dim, rotations_expand[i]);
	}
	
	// Iterate through candidates:
	int key_index = 0;
	Point last; // init to 0,0,0 because no candidate will ever equal that
	last.data[0] = 0;
	last.data[1] = 0;
	last.data[2] = 0;
	last.data[3] = 0;
	Key max_key = get_maximum_key(new_length);
	for (size_t i = 0; i < n_candidates; i++) {
		Point candidate = candidates[i];
		
		// Skip if duplicate from last
		if (cmp_points(&last, &candidate) == 0) continue;
		
		// If candidate is greater than current key index point, increment key index point
		if (key_index < old_length) {
			int over = 0; // keep track if the key_index exceeds the usable portion of the key data
			while(cmp_points(&candidate, &key.data[key_index]) > 0) {
				key_index++;
				if (key_index >= old_length) {
					over = 1;
					break;
				}
			}
			
			// Skip if candidate equals current key index point (overlapping point)
			// Ignore this check if we are over the usable portion of the key data
			if(cmp_points(&candidate, &key.data[key_index]) == 0 && !over) continue;
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
			candidate_dim = get_expand_dim(dimensions, face);
			
			// If this is a zero indexed face, we need to increase the candidate index 
			// to match the bounds (minimum of 1 in all dimensions)
			if (face > 2) {
				candidate = get_point_offset(candidate, face - 3);
			}
		}
		
		// For each rotated key:
		Key min_key = max_key;
		Key current_key;
		current_key.length = new_length;
		PointData data = get_point_data(candidate, candidate_dim);
		for (int j = 0; j < NUM_ROTATIONS; j++) {
			if (!(candidate_rotations_mask & (1 << j))) continue;
			
			// Generate candidate rotated to relevant rotation
			Point candidate_rotated = rotate(data, j);
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
					if (cmp_points(&candidate_rotated, &next) < 0 || k == old_length) {
						next = candidate_rotated;
						placed = 1;
						inc_k = 0;
						current_key.source_index = n;
					}
				}
				
				int result = cmp_points(&next, &min_key.data[n]);
				
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


// This is the faster function to determine if a key was generated
// from the maximum possible added point ("source_index").
// Points with single neighbors are guaranteed to not be considered
// "cut points" of the polycube.
// If this function finds such a point, the result will be thrown out
int has_larger_single_neighbor(Key key, uint8_t* places) {
	int base_keys[key.length];
	int retval = 0;
	const int* offsets_lut = spacemap_get_offsets();
	
	for (uint8_t i = 0; i < key.length; i++) {
		int ptkey = spacemap_get_key(key.data[i]);
		base_keys[i] = ptkey;
		places[ptkey] = 1;
	}
	
	for (uint8_t i = key.length - 1; i > key.source_index; i--) {
		uint8_t count = 0;
		int ptbasekey = base_keys[i];
		
		for (uint8_t f = 0; f < 6; f++) {
			count += places[ptbasekey + offsets_lut[f]];
		}
		
		if (count == 1) {
			retval = 1;
			break;
		}
	}
	
	for (uint8_t i = 0; i < key.length; i++) {
		places[base_keys[i]] = 0;
	}
	
	return retval;
}

// This is the slower function to determine if a key was generated
// from the maximum possible added point ("source_index").
// This function will run for every candidate alternative "source_index"
// and tries to traverse the entire cube.
// If it fails to find all points, the caller will move to the next
// alternative "source_index".
int is_connected_without(Key key, int index, uint8_t* places) {
	int source_length = key.length - 1;
	const int* offsets_lut = spacemap_get_offsets();
	
	uint8_t point_index = 1;
	int base_keys[key.length];
	int point_keys[source_length];
	point_keys[0] = spacemap_get_key(key.data[0]);
	base_keys[0] = point_keys[0];
	int ptkey = 0;
	
	for (uint8_t i = 1; i < key.length; i++) {
		ptkey = spacemap_get_key(key.data[i]);
		base_keys[i] = ptkey;
				
		if (i != index)	places[ptkey] = 1;
	}
	
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
		places[base_keys[i]] = 0;
	}
	
	return point_index >= source_length;
}

// Parent slower function to check for candidate alternative "source_index" points.
// This might not be the most efficient algorithm (such as graph theory methods), but the faster
// single neighbor check removes most of the slowness from this process
int has_larger_connected_source(Key key, uint8_t* places) {
	int retval = 0;
	
	for (uint8_t i = key.length - 1; i > key.source_index; i--) {
		if (is_connected_without(key, i, places)) {
			retval = 1;
			break;
		}
	}
		
	return retval;
}

