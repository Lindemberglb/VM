
#include "vm.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    const TLBEntry *entry;
    int page_number;
    int matched;
} TLBSearchTask;

static void *search_tlb_entry(void *argument){
    TLBSearchTask *task = argument;

    task->matched = task->entry->valid && task->entry->page_number == task->page_number;

    return NULL;
}

int vm_initialize(VirtualMemory *memory, const char *backing_store_path){
    if (memory == NULL || backing_store_path == NULL) {
        fprintf(stderr, "Erro: parametros invalidos ao inicializar a memoria virtual.\n");
        return 0;
    }

    FILE *file = fopen(backing_store_path, "rb");

    if (file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o backing store '%s'.\n", backing_store_path);
        return 0;
    }

    memset(memory, 0, sizeof(*memory));

    size_t bytes_read = fread(memory->backing_store, 1, BACKING_STORE_SIZE, file);
    int extra_data = fgetc(file);
    int read_error = ferror(file);
    int close_error = fclose(file);

    if (read_error) {
        fprintf(stderr, "Erro: falha ao ler o backing store '%s'.\n", backing_store_path);
        return 0;
    }

    if (bytes_read != BACKING_STORE_SIZE || extra_data != EOF || close_error != 0) {
        fprintf(stderr, "Erro: o backing store '%s' deve conter exatamente %d bytes.\n", backing_store_path, BACKING_STORE_SIZE);
        return 0;
    }

    for (int page = 0; page < PAGE_TABLE_SIZE; page++) {
        memory->page_table[page].frame_number = -1;
        memory->page_table[page].valid = 0;
        memory->page_table[page].loaded_at = 0;
        memory->page_table[page].last_used = 0;
    }

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        memory->frame_to_page[frame] = -1;
    }

    for (int entry = 0; entry < TLB_SIZE; entry++) {
        memory->tlb[entry].page_number = -1;
        memory->tlb[entry].frame_number = -1;
        memory->tlb[entry].valid = 0;
        memory->tlb[entry].loaded_at = 0;
        memory->tlb[entry].last_used = 0;
    }

    memory->tlb_hits = 0;

    memory->next_free_frame = 0;

    return 1;
}

static int select_fifo_frame(const VirtualMemory *memory){
    int oldest_frame = -1;
    size_t oldest_load = (size_t)-1;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        int page_number = memory->frame_to_page[frame];

        if (page_number < 0 || page_number >= PAGE_TABLE_SIZE) {
            return -1;
        }

        const PageTableEntry *entry = &memory->page_table[page_number];

        if (!entry->valid || entry->frame_number != frame) {
            return -1;
        }

        if (oldest_frame == -1 || entry->loaded_at < oldest_load) {
            oldest_frame = frame;
            oldest_load = entry->loaded_at;
        }
    }

    return oldest_frame;
}

static int select_lru_frame(const VirtualMemory *memory){
    int least_recent_frame = -1;
    size_t oldest_access = (size_t)-1;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        int page_number = memory->frame_to_page[frame];

        if (page_number < 0 || page_number >= PAGE_TABLE_SIZE) {
            return -1;
        }

        const PageTableEntry *entry = &memory->page_table[page_number];

        if (!entry->valid || entry->frame_number != frame) {
            return -1;
        }

        if (least_recent_frame == -1 || entry->last_used < oldest_access) {
            least_recent_frame = frame;
            oldest_access = entry->last_used;
        }
    }

    return least_recent_frame;
}

static int find_tlb_entry_parallel(const VirtualMemory *memory, int page_number){
    pthread_t threads[TLB_SIZE];
    TLBSearchTask tasks[TLB_SIZE];

    int created_threads = 0;

    for (int index = 0; index < TLB_SIZE; index++) {
        tasks[index].entry = &memory->tlb[index];
        tasks[index].page_number = page_number;
        tasks[index].matched = 0;

        int error = pthread_create(&threads[index],NULL,search_tlb_entry,&tasks[index]);

        if (error != 0) {
            fprintf(stderr, "Erro: nao foi possivel criar uma thread para a busca na TLB: %s.\n", strerror(error));

            for (int created = 0; created < created_threads; created++) {
                pthread_join(threads[created], NULL);
            }

            return -2;
        }

        created_threads++;
    }

    for (int index = 0; index < created_threads; index++) {
        int error = pthread_join(threads[index], NULL);

        if (error != 0) {
            fprintf(stderr, "Erro: nao foi possivel aguardar uma thread da busca na TLB: %s.\n", strerror(error));

            exit(EXIT_FAILURE);
        }
    }

    for (int index = 0; index < TLB_SIZE; index++) {
        if (tasks[index].matched) {
            return index;
        }
    }

    return -1;
}

static int select_lru_tlb_entry(const VirtualMemory *memory);

static int insert_tlb_entry(VirtualMemory *memory, int page_number, int frame_number, ReplacementAlgorithm algorithm){
    int selected_index = -1;

    for (int index = 0; index < TLB_SIZE; index++) {
        if (!memory->tlb[index].valid) {
            selected_index = index;
            break;
        }
    }

    if (selected_index == -1) {
        if (algorithm == FIFO) {
            size_t oldest_load = (size_t)-1;

            for (int index = 0; index < TLB_SIZE; index++) {
                if (selected_index == -1 || memory->tlb[index].loaded_at < oldest_load) {
                    selected_index = index;
                    oldest_load = memory->tlb[index].loaded_at;
                }
            }
        } else {
            selected_index = select_lru_tlb_entry(memory);
        }
    }

    if (selected_index < 0 || selected_index >= TLB_SIZE) {
        return -1;
    }

    memory->tlb[selected_index].page_number = page_number;
    memory->tlb[selected_index].frame_number = frame_number;
    memory->tlb[selected_index].valid = 1;
    memory->tlb[selected_index].loaded_at = memory->access_counter;
    memory->tlb[selected_index].last_used = memory->access_counter;
    return selected_index;
}

static int select_lru_tlb_entry(const VirtualMemory *memory){
    int least_recent_index = -1;
    size_t oldest_access = (size_t)-1;

    for (int index = 0; index < TLB_SIZE; index++) {
        if (!memory->tlb[index].valid) {
            continue;
        }

        if (least_recent_index == -1 ||
            memory->tlb[index].last_used < oldest_access) {
            least_recent_index = index;
            oldest_access = memory->tlb[index].last_used;
        }
    }

    return least_recent_index;
}

static void invalidate_tlb_page(VirtualMemory *memory, int page_number){
    for (int index = 0; index < TLB_SIZE; index++) {
        if (memory->tlb[index].valid && memory->tlb[index].page_number == page_number) {
            memory->tlb[index].valid = 0;
            memory->tlb[index].page_number = -1;
            memory->tlb[index].frame_number = -1;
            memory->tlb[index].loaded_at = 0;
            memory->tlb[index].last_used = 0;
        }
    }
}


int vm_translate(VirtualMemory *memory, int logical_address, ReplacementAlgorithm page_algorithm, ReplacementAlgorithm tlb_algorithm, TranslationResult *result){
    if (memory == NULL || result == NULL) {
        fprintf(stderr, "Erro: parametros invalidos para traducao.\n");
        return 0;
    }

    if ((page_algorithm != FIFO && page_algorithm != LRU) || (tlb_algorithm != FIFO && tlb_algorithm != LRU)) {
        fprintf(stderr, "Erro: algoritmo de substituicao invalido.\n");
        return 0;
    }

    if (logical_address < 0 || logical_address >= BACKING_STORE_SIZE) {
        fprintf(stderr, "Erro: endereco logico fora do intervalo permitido.\n");
        return 0;
    }

    int page_number = logical_address / PAGE_SIZE;
    int offset = logical_address % PAGE_SIZE;
    int page_fault = 0;
    int tlb_index = find_tlb_entry_parallel(memory, page_number);

    if (tlb_index == -2) {
        return 0;
    }

    int tlb_hit = tlb_index >= 0;
    int frame_number;

    if (tlb_hit) {
        TLBEntry *tlb_entry = &memory->tlb[tlb_index];
        frame_number = tlb_entry->frame_number;

        if (frame_number < 0 || frame_number >= NUM_FRAMES ||
            !memory->page_table[page_number].valid ||
            memory->page_table[page_number].frame_number != frame_number ||
            memory->frame_to_page[frame_number] != page_number) {
            fprintf(stderr, "Erro: mapeamento inconsistente na TLB.\n");
            return 0;
        }

        memory->tlb_hits++;
        tlb_entry->last_used = memory->access_counter + 1;

    } else {
        PageTableEntry *page_entry = &memory->page_table[page_number];

        if (page_entry->valid) {
            frame_number = page_entry->frame_number;

            if (frame_number < 0 || frame_number >= NUM_FRAMES ||
                memory->frame_to_page[frame_number] != page_number) {
                fprintf(stderr, "Erro: mapeamento inconsistente na tabela de paginas.\n");
                return 0;
            }
        } else {
            if (memory->next_free_frame < NUM_FRAMES) {
                frame_number = (int)memory->next_free_frame;
                memory->next_free_frame++;
            } else {
                if (page_algorithm == FIFO) {
                    frame_number = select_fifo_frame(memory);
                } else {
                    frame_number = select_lru_frame(memory);
                }

                if (frame_number < 0) {
                    fprintf(stderr, "Erro: nao foi possivel selecionar um quadro para substituicao.\n");
                    return 0;
                }

                int old_page = memory->frame_to_page[frame_number];
                PageTableEntry *old_entry = &memory->page_table[old_page];

                invalidate_tlb_page(memory, old_page);

                old_entry->valid = 0;
                old_entry->frame_number = -1;
                old_entry->loaded_at = 0;
                old_entry->last_used = 0;
            }

            memcpy(memory->physical_memory[frame_number],memory->backing_store + page_number * PAGE_SIZE, PAGE_SIZE);

            memory->frame_to_page[frame_number] = page_number;
            page_entry->frame_number = frame_number;
            page_entry->valid = 1;

            page_fault = 1;
            memory->page_faults++;
        }
    }

    memory->access_counter++;
    memory->page_table[page_number].last_used = memory->access_counter;

    if (page_fault) {
        memory->page_table[page_number].loaded_at = memory->access_counter;
    }

    if (!tlb_hit) {
        tlb_index = insert_tlb_entry(memory, page_number, frame_number, tlb_algorithm);

        if (tlb_index < 0) {
            fprintf(stderr, "Erro: falha ao inserir a traducao na TLB.\n");
            return 0;
        }
    }

    result->page_number = page_number;
    result->offset = offset;
    result->frame_number = frame_number;
    result->physical_address = frame_number * PAGE_SIZE + offset;
    result->value = (int8_t)memory->physical_memory[frame_number][offset];
    result->page_fault = page_fault;
    result->tlb_index = tlb_index;
    result->tlb_hit = tlb_hit;

    return 1;
}
