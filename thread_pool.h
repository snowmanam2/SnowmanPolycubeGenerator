#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "key.h"
#include "reader.h"
#include "writer.h"

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

typedef enum {OutputWriter, OutputKeys, OutputCount} OutputMode;

typedef struct {
	int n_threads;
	Key* input_keys;
	uint64_t input_count; // Count of keys in the input buffer
	uint64_t input_index; // Index of keys in the input buffer
	int input_length;
	Reader* reader;
	Key* output_keys;
	Key* write_keys;
	uint64_t output_count;
	uint64_t output_index;
	int output_length;
	Writer* writer;
	uint8_t* spacemap;
	OutputMode mode;
	int do_updates;
	uint64_t total_input_index; // Total input index for progress updates
	uint64_t total_input_count; // Total input count for progress updates
	time_t start_time;
	time_t last_update_time;
	pthread_mutex_t input_lock;
	pthread_mutex_t output_lock;
	pthread_mutex_t write_lock;
	pthread_mutex_t progress_lock;
	off_t file_offset;
} ThreadPool;

ThreadPool* thread_pool_create(int n_threads, int input_length, int output_length);
void thread_pool_destroy(ThreadPool* pool);

void thread_pool_set_input_keys(ThreadPool* pool, Key* input_keys, uint64_t input_count);
void thread_pool_set_input_reader(ThreadPool* pool, Reader* reader);

void thread_pool_set_output_keys(ThreadPool* pool, Key* output_keys);
void thread_pool_set_output_writer(ThreadPool* pool, Writer* writer);

uint64_t thread_pool_read(ThreadPool* pool);
int thread_pool_fetch_seeds(ThreadPool* pool, Key* fetch_keys);
void thread_pool_push_output(ThreadPool* pool, Key* output_keys, int output_count);

void thread_pool_enable_updates(ThreadPool* pool);

off_t thread_pool_update_offset(ThreadPool* pool, size_t segment_size);

uint64_t thread_pool_run(ThreadPool* pool);

#endif
