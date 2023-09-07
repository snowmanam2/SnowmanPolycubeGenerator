#include <pthread.h>
#include <stdint.h>

#include "key.h"
#include "thread_pool.h"
#include "packer.h"

#ifndef WORKER_H
#define WORKER_H

#define WORKER_FETCH_COUNT 5
#define WORKER_OUTPUT_CACHE 2098304
#define WORKER_OUTPUT_OVERFLOW 32768

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
	uint8_t output_data[WORKER_OUTPUT_CACHE + WORKER_OUTPUT_OVERFLOW];
	size_t data_size;
	int do_write;
	int fd;
	Packer* packer;
} WorkerData;

WorkerData* worker_create(ThreadPool* pool, int input_length, int output_length, int do_write, int fd, WriterMode mode);
void worker_destroy(WorkerData* wdata);

void worker_generation_data_create(WorkerData* wdata);
void worker_generation_data_destroy(WorkerData* wdata);

int worker_process_chunk(WorkerData* wdata, Key** output_keys);
void* worker_thread_function (void* arg);

#endif
