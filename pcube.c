#include <string.h>

#include "pcube.h"
#include "network_sort.h"

#define HEADER_WIDTH 6
#define COUNT_WIDTH 10

// Notes:
// This implementation currently doesn't support writing:
// - Specific orientations
// - Compression (perhaps add zlib later?)
// - Count writing (LEB128 makes this harder than it needs to be...)
// As such, this header is the same for all pcube files
// TODO: Add read / write compressed data
void pcube_write_header(OutputStream* stream, uint8_t compressed) {
	uint8_t magic[4];
	magic[0] = 0xCB;
	magic[1] = 0xEC;
	magic[2] = 0xCB;
	magic[3] = 0xEC;
	output_stream_write_raw(stream, magic, 4);
	
	uint8_t orientation = 0;
	output_stream_write_raw(stream, &orientation, 1);
	
	output_stream_write_raw(stream, &compressed, 1);
	
	// Write 0 count as a placeholder
	pcube_write_count(stream, 0);
}

// Writes the LEB128 encoded count
// https://en.wikipedia.org/wiki/LEB128
// This version actually writes a 64 bit fixed width.
// Most decoding algorithms seem to be able to read it, though it
// isn't exactly formatted the way the standard is written.
// The reason is to allocate the space at the front of the file
// to save until the actual count is known.
void pcube_write_count(OutputStream* stream, uint64_t count) {
	uint8_t top = 0x80; // Top bit to check if reached end
	uint8_t mask = 0x7F; // Bottom 7 bits
	
	uint8_t bytes[COUNT_WIDTH];
	for (uint8_t i = 0; i < COUNT_WIDTH; i++) {
		bytes[i] = (count & mask) | top;
		count = count >> 7;
	}
	
	bytes[COUNT_WIDTH - 1] &= ~top; // Last byte must have top bit set to zero
	
	output_stream_seek(stream, HEADER_WIDTH);
	output_stream_write_raw(stream, bytes, COUNT_WIDTH);
}

void pcube_write_key(OutputStream* stream, Key key) {
	Point dim_pt = key_get_dimensions(key);
	
	uint8_t dim[3];
	dim[0] = POINT_GET_X(dim_pt);
	dim[1] = POINT_GET_Y(dim_pt);
	dim[2] = POINT_GET_Z(dim_pt);
	
	uint16_t offsets[3];
	offsets[0] = dim[1] * dim[2];
	offsets[1] = dim[2];
	offsets[2] = 1;
	
	output_stream_write(stream, dim, 3);
	
	uint8_t bits = dim[0] * dim[1] * dim[2];
	uint8_t bytes = bits >> 3;
	bytes += bits > (bytes << 3);
	uint8_t data[bytes];
	memset(data, 0, bytes);
	
	// Accounts for the base (1,1,1) base point
	uint16_t base_offset = offsets[0] + offsets[1] + offsets[2];
	
	for (int i = 0; i < key.length; i++) {
		Point pt = key.data[i];
		uint16_t position = offsets[0] * POINT_GET_X(pt) + offsets[1] * POINT_GET_Y(pt) + POINT_GET_Z(pt) - base_offset;
		uint8_t byte = position >> 3;
		uint8_t bit = position - (byte << 3);
		
		data[byte] |= 1 << bit;
	}
	
	output_stream_write(stream, data, bytes);
}

void pcube_write_keys(OutputStream* stream, Key* keys, uint64_t count) {
	for (uint64_t i = 0; i < count; i++) {
		pcube_write_key(stream, keys[i]);
	}
}

int pcube_read_header(InputStream* stream) {
	uint8_t magic[4];
	if (!input_stream_read_raw(stream, magic, 4)) return 0;
	
	if (magic[0] != 0xCB || magic[1] != 0xEC || magic[2] != 0xCB || magic[3] != 0xEC) {
		printf("pcube input file has incorrect identifier\n");
		return 0;
	}
	
	// Ignore orientation for now
	uint8_t orientation;
	if(!input_stream_read_raw(stream, &orientation, 1)) return 0;
	
	uint8_t is_compressed;
	if(!input_stream_read_raw(stream, &is_compressed, 1)) return 0;
	
	if (is_compressed) {
		input_stream_set_compressed(stream, 1);
	}
		
	return 1;
}

// Reads the LEB128 encoded count
// https://en.wikipedia.org/wiki/LEB128
// TODO: This should be able to handle counts up to 128 bits
// *though that might be unrealistic for this file format to store*
uint64_t pcube_read_count(InputStream* stream) {
	uint64_t retval = 0;
	uint8_t shift = 0;
	uint8_t top = 0x80; // Top bit to check if reached end
	uint8_t mask = 0x7F; // Bottom 7 bits
	uint8_t value = 0;
	
	do {
		if (!input_stream_read_raw(stream, &value, 1)) return 0;
		retval |= (value & mask) << shift;
		shift += 7;
	} while (top & value);
	
	return retval;
}

// Gets the number of cubes to expect in future shapes
// Must be called in the shape area of the file
uint8_t pcube_read_n(InputStream* stream) {
	long offset = input_stream_get_offset(stream);
	
	Key test;
	pcube_read_key(stream, &test);
	
	input_stream_rewind(stream, offset);
	
	return test.length;
}

// Reads a key into the output pointer
// Returns 0 if the file ended
// Otherwise returns 1 on success
int pcube_read_key(InputStream* stream, Key* key_output) {
	Key retval;
	
	uint8_t dim[3];
	if (!input_stream_read(stream, dim, 3)) {
		return 0;
	}
	
	uint16_t bits = dim[0] * dim[1] * dim[2];
	uint8_t bytes = bits >> 3;
	bytes += bits > (bytes << 3);
	
	uint8_t data[bytes];
	if (!input_stream_read(stream, data, bytes)) {
		return 0;
	}
	
	uint16_t position = 0;
	int point_index = 0;
	for (int x = 1; x <= dim[0]; x++) {
		for (int y = 1; y <= dim[1]; y++) {
			for (int z = 1; z <= dim[2]; z++) {
				uint8_t byte = position >> 3;
				uint8_t bit = position % 8;
				
				if (data[byte] & (1 << bit)) {
					Point p = POINT_SET_X(x) + POINT_SET_Y(y) + POINT_SET_Z(z);
					retval.data[point_index] = p;
					point_index++;
				}
				
				position++;
			}
		}
	}
	
	network_sort(retval.data, point_index, point_compare);
	
	retval.length = point_index;
	*key_output = retval;
	
	return 1;
}

uint64_t pcube_read_keys(InputStream* stream, Key* output_keys, uint64_t count) {
	uint64_t i = 0;
	for (i = 0; i < count; i++) {
		int result = pcube_read_key(stream, &output_keys[i]);
		
		if (!result) break;
	}
	
	return i;
}
