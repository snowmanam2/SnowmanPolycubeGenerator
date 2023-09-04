#include <string.h>
#include <pthread.h>
#include <time.h>

#include "thread_pool.h"
#include "worker.h"

#define OUTPUT_CACHE 100000

ThreadPool* thread_pool_create(int n_threads, int input_length, int output_length) {
	ThreadPool* retval = calloc(1, sizeof(ThreadPool));
	
	retval->n_threads = n_threads;
	retval->input_length = input_length;
	retval->input_index = 0;
	retval->total_input_index = 0;
	retval->output_length = output_length;
	retval->mode = OutputCount;
	retval->reader = NULL;
	
	retval->do_updates = 0;
	
	pthread_mutex_init(&retval->input_lock, NULL);
	pthread_mutex_init(&retval->output_lock, NULL);
	pthread_mutex_init(&retval->write_lock, NULL);
	
	printf("Using thread pool with %d threads to generate n=%d from n=%d\n", 
		n_threads, output_length, input_length);
	
	return retval;
}

void thread_pool_destroy(ThreadPool* pool) {
	if (pool->mode == OutputWriter) {
		free(pool->output_keys);
		free(pool->write_keys);
		free(pool->spacemap);
	}
	
	if (pool->reader != NULL) {
		free(pool->input_keys);
	}

	free(pool);
}

void thread_pool_set_input_keys(ThreadPool* pool, Key* input_keys, uint64_t input_count) {
	pool->input_keys = input_keys;
	pool->input_count = input_count;
	pool->total_input_count = input_count;
}

void thread_pool_set_input_reader(ThreadPool* pool, Reader* reader) {
	pool->reader = reader;
	pool->input_count = 0;
	pool->input_keys = calloc(READER_MAX_COUNT, sizeof(Key));
	
	uint64_t count = reader_get_count(reader);
	if (count > 0) {
		printf("Found %lld polycubes in input file\n", (long long int) count);
		
		pool->total_input_count = count;
		thread_pool_enable_updates(pool);
	} else {
		printf("Could not get count of polycubes in input file. Progress updates disabled.\n");
	}
}

void thread_pool_set_output_keys(ThreadPool* pool, Key* output_keys) {
	pool->mode = OutputKeys;
	pool->output_keys = output_keys;
}

void thread_pool_set_output_writer(ThreadPool* pool, Writer* writer) {
	pool->mode = OutputWriter;
	pool->writer = writer;
	
	pool->output_keys = calloc(OUTPUT_CACHE + 1000, sizeof(Key));
	pool->write_keys = calloc(OUTPUT_CACHE + 1000, sizeof(Key));
	
	pool->spacemap = calloc(POINT_SPACEMAP_SIZE, sizeof(uint8_t));
	
	pool->output_index = 0;
}

void thread_pool_update_progress(ThreadPool* pool) {
	time_t now = time(NULL);
	
	double since_last = difftime(now, pool->last_update_time);

	if (since_last >= 1) {
		
		double diff = difftime(now, pool->start_time);
		double est_total = diff * pool->total_input_count / pool->total_input_index;
		
		int percent = (int)(100 * pool->total_input_index / pool->total_input_count);
		int bars = percent / 5;
		
		char progress[21];
		progress[20] = 0;
		
		for (int i = 0; i < 20; i++) {
			if (bars > i) progress[i] = '=';
			else progress[i] = ' ';
		}
		
		printf("  [%s] %d%% (est. %.f seconds remaining)\r", 
			progress, percent, est_total - diff);
		fflush(stdout);
		pool->last_update_time = now;
	}
}

int thread_pool_get_fetch_count(ThreadPool* pool) {
	uint64_t new_index = pool->input_index + WORKER_FETCH_COUNT;
	new_index = new_index >= pool->input_count ? pool->input_count : new_index;
	int count = new_index - pool->input_index;
	
	return count;
}

int thread_pool_fetch_seeds(ThreadPool* pool, Key* fetched_keys) {
	pthread_mutex_lock(&pool->input_lock);	
		
	int count = thread_pool_get_fetch_count(pool);
	
	if (count == 0 && pool->reader != NULL) {
		thread_pool_read(pool);
		count = thread_pool_get_fetch_count(pool);
	}
	
	memcpy(fetched_keys, &pool->input_keys[pool->input_index], count * sizeof(Key));
	
	pool->input_index += count;
	pool->total_input_index += count;
	
	if (pool->do_updates) {
		thread_pool_update_progress(pool);
	}
		
	pthread_mutex_unlock(&pool->input_lock);
	
	return count;
}

uint64_t thread_pool_read(ThreadPool* pool) {
	uint64_t n_read = reader_read_keys(pool->reader, pool->input_keys);
	
	pool->input_count = n_read;
	pool->input_index = 0;
	
	return n_read;
}

void thread_pool_write(ThreadPool* pool, uint64_t write_count) {
	writer_write_keys(pool->writer, pool->write_keys, write_count);
}

void thread_pool_swap_write_keys(ThreadPool* pool) {
	Key* temp = pool->write_keys;
	pool->write_keys = pool->output_keys;
	pool->output_keys = temp;
}

void thread_pool_enable_updates(ThreadPool* pool) {
	pool->do_updates = 1;
	pool->start_time = time(NULL);
	pool->last_update_time = pool->start_time;
	
	printf("  Waiting for output data...\r");
	fflush(stdout);
}

void thread_pool_push_output(ThreadPool* pool, Key* output_keys, int output_count) {
	int do_write = 0;
	uint64_t write_count = 0;
	
	if (output_count == 0) return;
	
	pthread_mutex_lock(&pool->output_lock);
	
	switch (pool->mode) {
		case OutputCount:
			pool->output_count += output_count;
			break;
		case OutputKeys:
			memcpy(&pool->output_keys[pool->output_count], output_keys, output_count * sizeof(Key));
			pool->output_count += output_count;
			break;
		case OutputWriter:
			memcpy(&pool->output_keys[pool->output_index], output_keys, output_count * sizeof(Key));
			pool->output_count += output_count;
			pool->output_index += output_count;
			
			if (pool->output_index > OUTPUT_CACHE) {
				do_write = 1;
				
				pthread_mutex_lock(&pool->write_lock);
				
				thread_pool_swap_write_keys(pool);
				
				write_count = pool->output_index;
				
				pool->output_index = 0;
				
			}
		default:
			break;
	}
	
	pthread_mutex_unlock(&pool->output_lock);
	
	if (do_write) {
		thread_pool_write(pool, write_count);
		
		pthread_mutex_unlock(&pool->write_lock);
	}
}

uint64_t thread_pool_run(ThreadPool* pool) {
	pthread_t threads[pool->n_threads];
	WorkerData worker_data[pool->n_threads];
	
	for (int i = 0; i < pool->n_threads; i++) {
		worker_create(&worker_data[i], pool, pool->input_length, pool->output_length);
	
		pthread_create(&threads[i], NULL, worker_thread_function, &worker_data[i]);
	}
	
	for (int i = 0; i < pool->n_threads; i++) {
		pthread_join(threads[i], NULL);
		
		worker_destroy(&worker_data[i]);
	}
	
	if (pool->mode == OutputWriter) {
		thread_pool_swap_write_keys(pool);
		thread_pool_write(pool, pool->output_index);
	}
	
	return pool->output_count;
}


