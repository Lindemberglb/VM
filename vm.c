
#include "vm.h"

#include <stdio.h>
#include <string.h>

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

int vm_translate(VirtualMemory *memory, int logical_address, ReplacementAlgorithm page_algorithm, TranslationResult *result){
    if (memory == NULL || result == NULL) {
        fprintf(stderr, "Erro: parametros invalidos para traducao.\n");
        return 0;
    }

    if (page_algorithm != FIFO) {
        fprintf(stderr,"Erro: algoritmo de substituicao ainda nao implementado nesta etapa.\n");
        return 0;
    }

    if (logical_address < 0 || logical_address >= BACKING_STORE_SIZE) {
        fprintf(stderr, "Erro: endereco logico fora do intervalo permitido.\n");
        return 0;
    }

    int page_number = logical_address / PAGE_SIZE;
    int offset = logical_address % PAGE_SIZE;

    PageTableEntry *page_entry = &memory->page_table[page_number];

    int page_fault = 0;
    int frame_number;

    if (page_entry->valid) {
        frame_number = page_entry->frame_number;

        if (frame_number < 0 || frame_number >= NUM_FRAMES || memory->frame_to_page[frame_number] != page_number) {
            fprintf(stderr, "Erro: mapeamento inconsistente na tabela de paginas.\n");
            return 0;
        }
    } else {
        if (memory->next_free_frame < NUM_FRAMES) {
            frame_number = (int)memory->next_free_frame;
            memory->next_free_frame++;
        } else {
            frame_number = select_fifo_frame(memory);

            if (frame_number < 0) {
                fprintf(stderr,"Erro: nao foi possivel selecionar um quadro para substituicao.\n");
                return 0;
            }

            int old_page = memory->frame_to_page[frame_number];
            PageTableEntry *old_entry = &memory->page_table[old_page];

            old_entry->valid = 0;
            old_entry->frame_number = -1;
            old_entry->loaded_at = 0;
            old_entry->last_used = 0;
        }

        memcpy(memory->physical_memory[frame_number], memory->backing_store + page_number * PAGE_SIZE, PAGE_SIZE);

        memory->frame_to_page[frame_number] = page_number;

        page_entry->frame_number = frame_number;
        page_entry->valid = 1;

        page_fault = 1;
        memory->page_faults++;
    }

    memory->access_counter++;

    page_entry->last_used = memory->access_counter;

    if (page_fault) {
        page_entry->loaded_at = memory->access_counter;
    }

    result->page_number = page_number;
    result->offset = offset;
    result->frame_number = frame_number;
    result->physical_address = frame_number * PAGE_SIZE + offset;
    result->value = (int8_t)memory->physical_memory[frame_number][offset];
    result->page_fault = page_fault;

    return 1;
}
