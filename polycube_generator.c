#include <stdio.h>
#include <time.h>
#include <string.h>

#include "generator.h"
#include "thread_pool.h"
#include "point.h"

#define SINGLE_THREAD_LENGTH 9
#define N_THREADS 16

void print_usage() {
	printf("Usage: polycube_generator <size> [options...]\n");
}

char* get_value(int* index, int argc, char** argv) {
	if (*index + 1 >= argc) {
		printf("Flag %s expects a value\n", argv[*index]);
		return NULL;
	}
	
	*index += 1;
	
	return argv[*index];
}

int main (int argc, char** argv) {
	if (argc < 2) {
		print_usage();
		return 0;
	}
	
	time_t start_time = time(NULL);
	
	int new_length;
	sscanf(argv[1], "%d", &new_length);
	
	if (new_length < 3) {
		printf("Length must be at least 3\n");
		return 0;
	} else if (new_length > 30) {
		printf("Length above 30 not implemented\n");
		return 0;
	}
	
	FILE* output_file = NULL;
	FILE* input_file = NULL;
		
	int n_threads = N_THREADS;
	
	for (int i = 2; i < argc; i++) {
		if(strcmp(argv[i], "-t") == 0) {
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			int result = sscanf(value, "%d", &n_threads);
			
			if (result == 0) {
				printf("Invalid number of threads\n");
				return 0;
			}
		} else if (strcmp(argv[i], "-i") == 0) {
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			
			input_file = fopen(value, "rb");
		} else if (strcmp(argv[i], "-o") == 0) {
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			
			output_file = fopen(value, "wb");
		} else {
			printf("Unrecognized argument \"%s\"\n", argv[i]);
			print_usage();
			return 0;
		}
	}
	
	char input_length = 0;
	if (input_file != NULL) {
		int result = fread(&input_length, 1, 1, input_file);
		
		if (result == 0) {
			fclose(input_file);
			printf("Unable to read cache file\n");
		} else if (input_length < 3) {
			fclose(input_file);
			printf("Note: Ignoring cache file with unsupported length %d (greater than target %d)\n", input_length, new_length);
		} else if (input_length > new_length) {
			fclose(input_file);
			printf("Note: Ignoring cache file with length %d (greater than target %d)\n", input_length, new_length);
		}
		else if (input_length == new_length) {
			fclose(input_file);
			printf("Note: Ignoring cache file with length %d (equal to target %d)\n", input_length, new_length);
		}
	}
	
	Key* output_keys = calloc(100000, sizeof(Key));
	uint64_t n_generated = 0;
	
	if (input_length < SINGLE_THREAD_LENGTH) {
		int target_length = new_length < SINGLE_THREAD_LENGTH ? new_length : SINGLE_THREAD_LENGTH;
		
		int start_length = input_file != NULL ? input_length : 2;
		
		ThreadPool* pool = thread_pool_create(1, start_length, target_length);
		
		if (input_file == NULL) {
			Key start;
			Point p = point_from_coords(1,1,1);
			
			start.data[0] = p;
			p = point_get_offset(p, 0);
			start.data[1] = p;
			
			start.length = 2;
			start.source_index = 0;
			
			thread_pool_set_input_keys(pool, &start, 1);
		} else {
			thread_pool_set_input_file(pool, input_file);
		}
		
		thread_pool_set_output_keys(pool, output_keys);
		
		if (new_length <= SINGLE_THREAD_LENGTH && output_file != NULL) {
			thread_pool_set_output_file(pool, output_file);
		}
		
		n_generated = thread_pool_run(pool);
		
		thread_pool_destroy(pool);
	}
	
	if (new_length > SINGLE_THREAD_LENGTH) {
		ThreadPool* pool = thread_pool_create(n_threads, SINGLE_THREAD_LENGTH, new_length);
		
		if (input_file != NULL && input_length >= SINGLE_THREAD_LENGTH) {
			thread_pool_set_input_file(pool, input_file);
		} else {
			thread_pool_set_input_keys(pool, output_keys, n_generated);
			thread_pool_enable_updates(pool);
		}
		
		if (output_file != NULL) {
			thread_pool_set_output_file(pool, output_file);
		}
		
		n_generated = thread_pool_run(pool);
		thread_pool_destroy(pool);
	}
	
	printf("%lld polycubes found of length %d\n", (long long int)n_generated, new_length);
	
	double diff = difftime(time(NULL), start_time);
	
	printf("%.f seconds elapsed\n", diff);
	
	free(output_keys);
	
	if (output_file != NULL) fclose(output_file);
	if (input_file != NULL) fclose(input_file);
	
	return 0;
}
