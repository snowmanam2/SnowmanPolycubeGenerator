#include <string.h>
#include <pthread.h>
#include <time.h>

#include "thread_pool.h"
#include "worker.h"
#include "compression.h"


#define FETCH_COUNT 5
#define OUTPUT_CACHE 10000
#define READ_COUNT 10000

ThreadPool* thread_pool_create(int n_threads, int input_length, int output_length) {
	ThreadPool* retval = calloc(1, sizeof(ThreadPool));
	
	retval->n_threads = n_threads;
	retval->input_length = input_length;
	retval->input_index = 0;
	retval->output_length = output_length;
	retval->mode = OutputCount;
	retval->input_file = NULL;
	
	retval->next_update_index = -1;
	
	pthread_mutex_init(&retval->input_lock, NULL);
	pthread_mutex_init(&retval->output_lock, NULL);
	pthread_mutex_init(&retval->write_lock, NULL);
	
	return retval;
}

void thread_pool_destroy(ThreadPool* pool) {
	if (pool->mode == OutputFile) {
		free(pool->output_keys);
		free(pool->write_keys);
		free(pool->spacemap);
	}
	
	if (pool->input_file != NULL) {
		free(pool->input_keys);
	}

	free(pool);
}

void thread_pool_set_input_keys(ThreadPool* pool, Key* input_keys, uint64_t input_count) {
	pool->input_keys = input_keys;
	pool->input_count = input_count;
}

void thread_pool_set_input_file(ThreadPool* pool, FILE* file) {
	pool->input_file = file;
	
	pool->input_keys = calloc(READ_COUNT, sizeof(Key));
}

void thread_pool_set_output_keys(ThreadPool* pool, Key* output_keys) {
	pool->mode = OutputKeys;
	pool->output_keys = output_keys;
}

void thread_pool_set_output_file(ThreadPool* pool, FILE* file) {
	pool->mode = OutputFile;
	pool->output_file = file;
	
	pool->output_keys = calloc(OUTPUT_CACHE + 1000, sizeof(Key));
	pool->write_keys = calloc(OUTPUT_CACHE + 1000, sizeof(Key));
	
	pool->spacemap = calloc(POINT_SPACEMAP_SIZE, sizeof(uint8_t));
	
	pool->output_index = 0;
	
	char len = pool->output_length;
	
	fwrite(&len, sizeof(char), 1, file);
}

void thread_pool_update_progress(ThreadPool* pool) {
	if (pool->input_index >= pool->next_update_index) {
		
		double diff = difftime(time(NULL), pool->start_time);
		double est_total = diff * pool->input_count / pool->input_index;
		
		printf("%d%% ... (est. %.f seconds remaining)\n", 
			(int)(100 * pool->input_index / pool->input_count),
			est_total - diff);
		
		pool->next_update_index += pool->input_count / 20;
	}
}

int thread_pool_get_fetch_count(ThreadPool* pool) {
	uint64_t new_index = pool->input_index + FETCH_COUNT;
	new_index = new_index >= pool->input_count ? pool->input_count : new_index;
	int count = new_index - pool->input_index;
	
	return count;
}

int thread_pool_fetch_seeds(ThreadPool* pool, Key* fetched_keys) {
	pthread_mutex_lock(&pool->input_lock);	
		
	int count = thread_pool_get_fetch_count(pool);
	
	if (count == 0 && pool->input_file != NULL) {
		thread_pool_read_file(pool);
		count = thread_pool_get_fetch_count(pool);
	}
	
	memcpy(fetched_keys, &pool->input_keys[pool->input_index], count * sizeof(Key));
	
	pool->input_index += count;
	
	if (pool->next_update_index >= 0) {
		thread_pool_update_progress(pool);
	}
		
	pthread_mutex_unlock(&pool->input_lock);
	
	return count;
}

uint64_t thread_pool_read_file(ThreadPool* pool) {
	size_t raw_size = compression_key_size(pool->input_length);
	size_t in_buf_size = READ_COUNT * raw_size;
	char buffer[in_buf_size];
	
	size_t read_count = 0;
	
	read_count = fread(buffer, sizeof(char), in_buf_size, pool->input_file);
	
	if (read_count == 0) return 0;
	
	size_t n_read = read_count / raw_size;
	
	for (uint64_t i = 0; i < n_read; i++) {	
		pool->input_keys[i] = compression_decompress(&buffer[i * raw_size], pool->input_length);
	}
	
	pool->input_count = n_read;
	pool->input_index = 0;
	
	return n_read;
}

void thread_pool_write_file(ThreadPool* pool, uint64_t write_count) {
	
	int len = compression_key_size(pool->output_length);
	
	char buffer[len];
	
	for (uint64_t i = 0; i < write_count; i++) {
		memset(buffer, 0, len);
		
		compression_compress(pool->write_keys[i], pool->output_length, buffer, pool->spacemap);
		
		fwrite(buffer, sizeof(char), len, pool->output_file);
	}
	
}

void thread_pool_swap_write_keys(ThreadPool* pool) {
	Key* temp = pool->write_keys;
	pool->write_keys = pool->output_keys;
	pool->output_keys = temp;
}

void thread_pool_enable_updates(ThreadPool* pool) {
	pool->next_update_index = pool->input_count / 20;
	pool->start_time = time(NULL);
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
		case OutputFile:
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
		thread_pool_write_file(pool, write_count);
		
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
	
	if (pool->mode == OutputFile) {
		thread_pool_swap_write_keys(pool);
		thread_pool_write_file(pool, pool->output_index);
	}
	
	return pool->output_count
;}


