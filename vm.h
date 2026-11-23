
#ifndef VM_H
#define VM_H

#define PAGE_SIZE 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 256
#define BACKING_STORE_SIZE 65536

typedef enum {
    FIFO,
    LRU
} ReplacementAlgorithm;

#endif
