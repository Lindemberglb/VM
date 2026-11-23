
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
