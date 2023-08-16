#include <stdio.h>

#include "worker.h"
#include "key_compression.h"
#include "spacemap.h"
#include "thread_pool.h"

void worker_create(WorkerData* wdata, ThreadPool* pool, int input_length, int output_length) {
	
	wdata->pool = pool;
	wdata->input_length = input_length;
	wdata->output_length = output_length;
	wdata->total = 0;
	
	wdata->spacemap = spacemap_create();
}

void worker_destroy(WorkerData* wdata) {
	spacemap_destroy(wdata->spacemap);
}

void worker_generation_data_create(WorkerData* wdata) {
	int count = wdata->output_length - wdata->input_length;
	
	GenerationData* retval = calloc(count, sizeof(GenerationData));
	
	for (int i = 0; i < count; i++) {
		retval[i].new_length = wdata->input_length + i + 1;
		
		if (i > 0) retval[i].seed_keys = retval[i-1].output_keys;
	}
	
	wdata->generation_data = retval;
}

void worker_generation_data_destroy(WorkerData* wdata) {
	free(wdata->generation_data);
}

void worker_generation_data_init(WorkerData* wdata) {
	GenerationData* gdata = wdata->generation_data;
	int levels = wdata->output_length - wdata->input_length;

	for (int i = 0; i < levels; i++) {
		gdata[i].seed_count = 0;
		gdata[i].index = 0;
	}
}

void worker_generate_level(GenerationData* gdata, uint8_t* spacemap) {
	
	Key seed = gdata->seed_keys[gdata->index];
	int new_length = gdata->new_length;
		
	int n_generated = generate(seed, gdata->new_length, gdata->output_keys);
	qsort(gdata->output_keys, n_generated, sizeof(Key), cmp_keys);
	
	int a = 0;
	int last_output = 0;
	Key last = get_maximum_key(new_length);
			
	for (int j = 0; j < n_generated; j++) {
		if (cmp_keys(&last, &gdata->output_keys[j]) == 0 && last_output) continue;
		
		last = gdata->output_keys[j];
		last_output = 0;
			
		if (has_larger_single_neighbor(gdata->output_keys[j], spacemap)) continue;
			
		if (has_larger_connected_source(gdata->output_keys[j], spacemap)) continue;
			
		gdata->output_keys[a] = gdata->output_keys[j];
		a++;
		last_output = 1;
	}
		
	gdata->output_count = a;
	gdata->index++;
}

// This function processes a single polycube
// at the required level for output.
// Returns the number of output_keys generated.
// Returns -1 if the GenerationData is complete
int worker_process_chunk(WorkerData* wdata, Key** output_keys) {
	GenerationData* gdata = wdata->generation_data;
	int levels = wdata->output_length - wdata->input_length;
	*output_keys = gdata[levels - 1].output_keys;
	
	int start = levels - 1;
	
	for (int i = start; i >= 0; i--) {
		start = i;
		
		if (gdata[i].index < gdata[i].seed_count) break;
		
		gdata[i].index = 0;
		
		if (i == 0) return -1;
	}
		
	for (int i = start; i < levels; i++) {
		if (gdata[i].seed_count > 0) worker_generate_level(&gdata[i], wdata->spacemap);
		else gdata[i].output_count = 0;
		
		if ((i + 1) < levels) {
			gdata[i + 1].seed_count = gdata[i].output_count;
		}
	}

	return gdata[levels - 1].output_count;
}

void* worker_thread_function(void* arg) {
	WorkerData* wdata = (WorkerData*) arg;
	worker_generation_data_create(wdata);
	Key* output_keys = NULL;
	
	while (1) {
		worker_generation_data_init(wdata);
		
		int fetch_count = thread_pool_fetch_seeds(wdata->pool, &wdata->generation_data[0].seed_keys);
				
		if (fetch_count == 0) break;
		
		wdata->generation_data[0].seed_count = fetch_count;
		
		while (1) {
			int result = worker_process_chunk(wdata, &output_keys);
			
			if (result < 0) break;
			
			thread_pool_push_output(wdata->pool, output_keys, result);
		}
	}
	
	worker_generation_data_destroy(wdata);
	pthread_exit(NULL);
}
