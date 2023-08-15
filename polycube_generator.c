#include <stdio.h>
#include <time.h>

#include "key_generator.h"
#include "thread_pool.h"

#define SINGLE_THREAD_LENGTH 9
#define N_THREADS 6

int main (int argc, char** argv) {
	if (argc < 2) {
		printf("Usage: polycube_generator <size> [output_file]\n");
		return 0;
	}
	
	time_t start_time = time(NULL);
	
	int new_length;
	sscanf(argv[1], "%d", &new_length);
	
	if (new_length < 3) {
		printf("Length must be at least 3\n");
		return 0;
	} else if (new_length > 20) {
		printf("Length above 20 not implemented\n");
	}
	
	FILE* output_file = NULL;
	
	if (argc >= 3) {
		output_file = fopen(argv[2], "wb");
	}
	
	int target_length = new_length < SINGLE_THREAD_LENGTH ? new_length : SINGLE_THREAD_LENGTH;
	
	ThreadPool* pool = thread_pool_create(1, 2, target_length);
	
	Key start;
	Point p;
	p.data[0] = 1;
	p.data[1] = 1;
	p.data[2] = 1;
	p.data[3] = 0;
		
	start.data[0] = p;
	p = get_point_offset(p, 0);
	start.data[1] = p;
	
	start.length = 2;
	start.source_index = 0;
	
	thread_pool_set_input_keys(pool, &start, 1);
	
	Key* output_keys = calloc(100000, sizeof(Key));
	
	thread_pool_set_output_keys(pool, output_keys);
	
	if (new_length <= SINGLE_THREAD_LENGTH && output_file != NULL) {
		thread_pool_set_output_file(pool, output_file);
	}
	
	uint64_t n_generated = thread_pool_run(pool);
		
	thread_pool_destroy(pool);
		
	if (new_length > SINGLE_THREAD_LENGTH) {
		pool = thread_pool_create(N_THREADS, SINGLE_THREAD_LENGTH, new_length);
		thread_pool_set_input_keys(pool, output_keys, n_generated);
		
		if (output_file != NULL) {
			thread_pool_set_output_file(pool, output_file);
		}
		
		thread_pool_enable_updates(pool);
		
		n_generated = thread_pool_run(pool);
		thread_pool_destroy(pool);
	}
	
	printf("%ld polycubes found of length %d\n", n_generated, new_length);
	
	double diff = difftime(time(NULL), start_time);
	
	printf("%.f seconds elapsed\n", diff);
	
	free(output_keys);
	
	if (output_file != NULL) fclose(output_file);
	
	return 0;
}
