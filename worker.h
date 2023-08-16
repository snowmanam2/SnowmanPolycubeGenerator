#include <pthread.h>
#include <stdint.h>

#include "key_generator.h"
#include "thread_pool.h"

#ifndef WORKER_H
#define WORKER_H


typedef struct {
	Key* seed_keys;
	int seed_count;
	Key output_keys[180];
	int output_count;
	int new_length;
	int index;
} GenerationData;

typedef struct {
	int input_length;
	int output_length;
	GenerationData* generation_data;
	uint8_t* spacemap;
	ThreadPool* pool;
	int cache_count;
	uint64_t total;
} WorkerData;

void worker_create(WorkerData* wdata, ThreadPool* pool, int input_length, int output_length);
void worker_destroy(WorkerData* wdata);

void worker_generation_data_create(WorkerData* wdata);
void worker_generation_data_destroy(WorkerData* wdata);

int worker_process_chunk(WorkerData* wdata, Key** output_keys);
void* worker_thread_function (void* arg);

#endif
