#include <pthread.h>
#include <stdint.h>

#include "key.h"
#include "thread_pool.h"

#ifndef WORKER_H
#define WORKER_H

#define WORKER_FETCH_COUNT 5

typedef struct {
	Key* seed_keys;
	int seed_count;
	Key output_keys[180];
	int output_count;
	int new_length;
	int index;
	uint64_t total;
} GenerationData;

typedef struct {
	int input_length;
	int output_length;
	GenerationData* generation_data;
	uint8_t* spacemap;
	ThreadPool* pool;
	int cache_count;
} WorkerData;

WorkerData* worker_create(ThreadPool* pool, int input_length, int output_length);
void worker_destroy(WorkerData* wdata);

void worker_generation_data_create(WorkerData* wdata);
void worker_generation_data_destroy(WorkerData* wdata);

int worker_process_chunk(WorkerData* wdata, Key** output_keys);
void* worker_thread_function (void* arg);

uint64_t worker_get_total(WorkerData* wdata, int index);

#endif
