#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

#include "generator.h"
#include "thread_pool.h"
#include "point.h"
#include "reader.h"
#include "writer.h"

#define SINGLE_THREAD_LENGTH 9
#define N_THREADS 16

typedef struct {
	FILE* results_file;
	int result_count;
	int result_lengths[32];
	uint64_t results[32];
} Results;

Results* results_create(char* filename) {
	FILE* rfile = fopen(filename, "w");
	if (rfile == NULL) return NULL;

	Results* r = calloc(1, sizeof(Results));
	r->results_file = rfile;
	return r;
}

void results_destroy(Results* r) {
	if (r == NULL) return;
	free(r);
}

void results_add(Results* r, int length, uint64_t result) {
	if (r == NULL) return;
	
	int index = r->result_count;
	r->result_lengths[index] = length;
	r->results[index] = result;
	r->result_count++;
}

void results_write(Results* r) {
	if (r ==  NULL) return;
	
	for (int i = 0; i < r->result_count; i++) {
		fprintf(r->results_file, "%d\n", r->result_lengths[i]);
		fprintf(r->results_file, "%lld\n", (long long int)r->results[i]);
	}
	
	fclose(r->results_file);
}

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

Reader* build_reader(char* opt, char* filename) {
	int len = strlen(filename);
	
	ReaderMode mode = ReadBitFace;
	
	if (len > 6) {
		if(strcmp(&filename[len-6], ".pcube") == 0) {
			mode = ReadPCube;
		}
	}
	
	return reader_create(filename, mode);
	
}

Writer* build_writer(char* opt, char* filename, uint8_t new_length) {
	int len = strlen(filename);
	
	WriterMode mode = WriteBitFace;
	
	int compressed = strcmp(opt, "-oz") == 0;
	
	if (len > 6) {
		if(strcmp(&filename[len-6], ".pcube") == 0) {
			mode = WritePCube;
		}
	}
	
	return writer_create(filename, mode, new_length, compressed);
}

void convert_files(Reader* reader, Writer* writer) {
	printf("Converting data of equal length between formats...\n");
	
	Key* keys = calloc(READER_MAX_COUNT, sizeof(Key));
	
	uint64_t total = 0;
	
	uint64_t count = 0;
	do {
		count = reader_read_keys(reader, keys);
		writer_write_keys(writer, keys, count);
		
		total += count;
	} while (count > 0);
	
	writer_write_count(writer, total);
	writer_destroy(writer);
	
	printf("Processed %lld polycubes.\n", (long long int) total);
}

int main (int argc, char** argv) {
	if (argc < 2) {
		print_usage();
		return 0;
	}
	
	time_t start_time = time(NULL);
	
	int new_length;
	Writer* writer = NULL;
	Reader* reader = NULL;
	int n_threads = N_THREADS;
	int output_all = 0;
	
	Results* results = NULL;
	sscanf(argv[1], "%d", &new_length);
	
	if (new_length < 3) {
		printf("Length must be at least 3\n");
		return 0;
	} else if (new_length > 30) {
		printf("Length above 30 not implemented\n");
		return 0;
	}
	
	for (int i = 2; i < argc; i++) {
		if(strcmp(argv[i], "-t") == 0) {
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			int result = sscanf(value, "%d", &n_threads);
			
			if (result == 0) {
				printf("Invalid number of threads\n");
				return 0;
			}
		} else if (strncmp(argv[i], "-i", 2) == 0) {
			char* opt = argv[i];
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			
			reader = build_reader(opt, value);
			if (reader == NULL) return 0;
		} else if (strncmp(argv[i], "-o", 2) == 0) {
			char* opt = argv[i];
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			
			writer = build_writer(opt, value, new_length);
			
			if (writer == NULL) return 0;
		} else if (strcmp(argv[i], "-a") == 0) {
			output_all = 1;
		} else if (strcmp(argv[i], "-r") == 0) {
			char* value = get_value(&i, argc, argv);
			
			if (value == NULL) return 0;
			
			results = results_create(value);
			output_all = 1;
		} else {
			printf("Unrecognized argument \"%s\"\n", argv[i]);
			print_usage();
			return 0;
		}
	}
	
	char input_length = 0;
	if (reader != NULL) {
		input_length = reader_get_n(reader);
		
		if (input_length < 3) {
			reader_destroy(reader);
			reader = NULL;
			printf("Note: Ignoring cache file with unsupported length %d (greater than target %d)\n", input_length, new_length);
		} else if (input_length > new_length) {
			reader_destroy(reader);
			reader = NULL;
			printf("Note: Ignoring cache file with length %d (greater than target %d)\n", input_length, new_length);
		}
		else if (input_length == new_length) {
			if (writer == NULL) {
				reader_destroy(reader);
				reader = NULL;
				printf("Note: Ignoring cache file with length %d (equal to target %d)\n", input_length, new_length);
			} else {
				convert_files(reader, writer);
				return 0;
			}
		}
	}
	
	Key* output_keys = calloc(100000, sizeof(Key));
	uint64_t n_generated = 0;
	
	if (input_length < SINGLE_THREAD_LENGTH) {
		int target_length = new_length < SINGLE_THREAD_LENGTH ? new_length : SINGLE_THREAD_LENGTH;
		
		int start_length = reader != NULL ? input_length : 2;
		
		ThreadPool* pool = thread_pool_create(1, start_length, target_length);
		
		Key start;
		if (reader == NULL) {
			Point p = point_from_coords(1,1,1);
			
			start.data[0] = p;
			p = point_get_offset(p, 0);
			start.data[1] = p;
			
			start.length = 2;
			start.source_index = 0;
			
			thread_pool_set_input_keys(pool, &start, 1);
		} else {
			thread_pool_set_input_reader(pool, reader);
		}
		
		thread_pool_set_output_keys(pool, output_keys);
		
		if (new_length <= SINGLE_THREAD_LENGTH && writer != NULL) {
			thread_pool_set_output_writer(pool, writer);
		}
		
		n_generated = thread_pool_run(pool);
		
		if (output_all) {
			for (int i = 0; i < target_length - start_length; i++) {
				int length = i + start_length + 1;
				uint64_t result = thread_pool_get_total(pool, i);
				printf("%lld polycubes found of length %d                      \n", 
					(long long int)result, i + start_length + 1);
				
				results_add(results, length, result);
			}
		}
		
		thread_pool_destroy(pool);
	}
	
	if (new_length > SINGLE_THREAD_LENGTH) {
		int use_file = reader != NULL && input_length >= SINGLE_THREAD_LENGTH;
		
		int start_length = use_file ? input_length : SINGLE_THREAD_LENGTH;
			
		ThreadPool* pool = thread_pool_create(n_threads, start_length, new_length);
		
		if (use_file) {
			thread_pool_set_input_reader(pool, reader);
		} else {
			thread_pool_set_input_keys(pool, output_keys, n_generated);
			thread_pool_enable_updates(pool);
		}
		
		if (writer != NULL) {
			thread_pool_set_output_writer(pool, writer);
		}
		
		n_generated = thread_pool_run(pool);
		
		if (output_all) {
			for (int i = 0; i < new_length - start_length; i++) {
				int length = i + start_length + 1;
				uint64_t result = thread_pool_get_total(pool, i);
				printf("%lld polycubes found of length %d                      \n", 
					(long long int)result, i + start_length + 1);
				
				results_add(results, length, result);
			}
		}
		
		thread_pool_destroy(pool);
	}
	
	if (!output_all) printf("%lld polycubes found of length %d                      \n", (long long int)n_generated, new_length);
	
	double diff = difftime(time(NULL), start_time);
	
	printf("%.f seconds elapsed\n", diff);
	
	results_write(results);
	results_destroy(results);
	
	free(output_keys);
	
	if (writer != NULL) {
		writer_write_count(writer, n_generated);
		writer_destroy(writer);
	}
	if (reader != NULL) reader_destroy(reader);
	
	return 0;
}
