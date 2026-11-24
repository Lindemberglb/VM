#ifndef VM_H
#define VM_H

#include <stddef.h>
#include <stdint.h>

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
    int page_number;
    int frame_number;
    int valid;
    size_t loaded_at;
    size_t last_used;
} TLBEntry;

typedef struct {
    unsigned char physical_memory[NUM_FRAMES][PAGE_SIZE];
    unsigned char backing_store[BACKING_STORE_SIZE];
    PageTableEntry page_table[PAGE_TABLE_SIZE];
    TLBEntry tlb[TLB_SIZE];
    int frame_to_page[NUM_FRAMES];
    size_t next_free_frame;
    size_t access_counter;
    size_t page_faults;
    size_t tlb_hits;
} VirtualMemory;

typedef struct {
    int page_number;
    int offset;
    int frame_number;
    int physical_address;
    int8_t value;
    int page_fault;
    int tlb_index;
    int tlb_hit;
} TranslationResult;

int vm_initialize(VirtualMemory *memory, const char *backing_store_path);

int vm_translate(
    VirtualMemory *memory,
    int logical_address,
    ReplacementAlgorithm page_algorithm,
    ReplacementAlgorithm tlb_algorithm,
    TranslationResult *result
);

#endif

