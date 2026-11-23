
#ifndef VM_H
#define VM_H

#include <stddef.h>

#define PAGE_SIZE 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 256
#define BACKING_STORE_SIZE 65536
#define BACKING_STORE_FILE "BACKING_STORE.bin"

typedef enum {
    FIFO,
    LRU
} ReplacementAlgorithm;

typedef struct {
    int frame_number;
    int valid;
    size_t loaded_at;
    size_t last_used;
} PageTableEntry;

typedef struct {
    unsigned char physical_memory[NUM_FRAMES][PAGE_SIZE];
    unsigned char backing_store[BACKING_STORE_SIZE];
    PageTableEntry page_table[PAGE_TABLE_SIZE];
    int frame_to_page[NUM_FRAMES];
    size_t next_free_frame;
} VirtualMemory;

int vm_initialize(VirtualMemory *memory, const char *backing_store_path);

#endif

