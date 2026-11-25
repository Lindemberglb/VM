#include "vm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_FILE "correct.txt"

static int parse_algorithm(const char *argument, ReplacementAlgorithm *algorithm){
    if (strcmp(argument, "fifo") == 0) {
        *algorithm = FIFO;
        return 1;
    }

    if (strcmp(argument, "lru") == 0) {
        *algorithm = LRU;
        return 1;
    }

    return 0;
}

static int load_addresses(const char *filename, int **addresses, size_t *count){
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", filename);
        return 0;
    }

    char line[128];
    size_t capacity = 0;
    size_t total = 0;
    size_t line_number = 0;
    int *loaded_addresses = NULL;
    int valid = 1;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "Erro: linha %zu do arquivo excede o tamanho permitido.\n", line_number);
            valid = 0;
            break;
        }

        if (line_number == 1 && strlen(line) >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            memmove(line, line + 3, strlen(line + 3) + 1);
        }

        errno = 0;
        char *end = NULL;
        long value = strtol(line, &end, 10);

        if (line == end || errno == ERANGE || value < 0 || value >= BACKING_STORE_SIZE) {
            fprintf(stderr, "Erro: endereco logico invalido na linha %zu.\n", line_number);
            valid = 0;
            break;
        }

        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
            end++;
        }

        if (*end != '\0') {
            fprintf(stderr, "Erro: formato invalido na linha %zu do arquivo.\n", line_number);
            valid = 0;
            break;
        }

        if (total == capacity) {
            size_t new_capacity = capacity == 0 ? 16 : capacity * 2;

            int *resized_addresses = realloc(
                loaded_addresses,
                new_capacity * sizeof(*loaded_addresses)
            );

            if (resized_addresses == NULL) {
                fprintf(stderr, "Erro: memoria insuficiente para carregar os enderecos.\n");
                valid = 0;
                break;
            }

            loaded_addresses = resized_addresses;
            capacity = new_capacity;
        }

        loaded_addresses[total++] = (int)value;
    }

    if (ferror(file)) {
        fprintf(stderr, "Erro: falha durante a leitura do arquivo '%s'.\n", filename);
        valid = 0;
    }

    fclose(file);

    if (valid && total == 0) {
        fprintf(stderr, "Erro: o arquivo de enderecos esta vazio.\n");
        valid = 0;
    }

    if (!valid) {
        free(loaded_addresses);
        return 0;
    }

    *addresses = loaded_addresses;
    *count = total;

    return 1;
}

int main(int argc, char *argv[]){
    if (argc < 4) {
        fprintf(stderr,"Uso: %s <arquivo_de_enderecos> <fifo|lru para paginas> ""<fifo|lru para TLB> [--frames 128|256] [--no-tlb | --serial-tlb]\n",argv[0]);

        return EXIT_FAILURE;
    }

    ReplacementAlgorithm page_algorithm;
    ReplacementAlgorithm tlb_algorithm;

    if (!parse_algorithm(argv[2], &page_algorithm)) {
        fprintf(
            stderr,
            "Erro: algoritmo de substituicao de paginas invalido. Use 'fifo' ou 'lru'.\n"
        );

        return EXIT_FAILURE;
    }

    if (!parse_algorithm(argv[3], &tlb_algorithm)) {
        fprintf(
            stderr,
            "Erro: algoritmo de substituicao da TLB invalido. Use 'fifo' ou 'lru'.\n"
        );

        return EXIT_FAILURE;
    }

    size_t frame_count = NUM_FRAMES;
    int tlb_enabled = 1;
    int threaded_tlb = 1;
    int frames_option_seen = 0;
    int no_tlb_option_seen = 0;
    int serial_tlb_option_seen = 0;

    for (int index = 4; index < argc; index++) {
        if (strcmp(argv[index], "--frames") == 0) {
            if (frames_option_seen || index + 1 >= argc) {
                fprintf(stderr, "Erro: informe --frames uma unica vez com o valor 128 ou 256.\n");
                return EXIT_FAILURE;
            }

            const char *frame_argument = argv[++index];
            errno = 0;
            char *end = NULL;
            long parsed_frame_count = strtol(frame_argument, &end, 10);

            if (frame_argument == end || *end != '\0' || errno == ERANGE ||
                (parsed_frame_count != NUM_FRAMES && parsed_frame_count != MAX_FRAMES)) {
                fprintf(stderr, "Erro: o numero de frames deve ser 128 ou 256.\n");
                return EXIT_FAILURE;
            }

            frame_count = (size_t)parsed_frame_count;
            frames_option_seen = 1;

        } else if (strcmp(argv[index], "--no-tlb") == 0) {
            if (no_tlb_option_seen) {
                fprintf(stderr, "Erro: a opcao --no-tlb pode ser informada apenas uma vez.\n");
                return EXIT_FAILURE;
            }

            tlb_enabled = 0;
            threaded_tlb = 0;
            no_tlb_option_seen = 1;

        } else if (strcmp(argv[index], "--serial-tlb") == 0) {
            if (serial_tlb_option_seen) {
                fprintf(stderr, "Erro: a opcao --serial-tlb pode ser informada apenas uma vez.\n");
                return EXIT_FAILURE;
            }

            threaded_tlb = 0;
            serial_tlb_option_seen = 1;
        } else {
            fprintf(stderr, "Erro: opcao desconhecida '%s'.\n", argv[index]);
            return EXIT_FAILURE;
        }
    }

    if (!tlb_enabled && serial_tlb_option_seen) {
        fprintf(stderr, "Erro: --serial-tlb nao pode ser combinado com --no-tlb.\n");
        return EXIT_FAILURE;
    }

    int *addresses = NULL;
    size_t address_count = 0;

    if (!load_addresses(argv[1], &addresses, &address_count)) {
        return EXIT_FAILURE;
    }

    VirtualMemory memory;

    if (!vm_initialize(&memory,BACKING_STORE_FILE,frame_count,tlb_enabled,threaded_tlb)) {
        free(addresses);
        return EXIT_FAILURE;
    }

    FILE *output = fopen(OUTPUT_FILE, "w");

    if (output == NULL) {
        fprintf(
            stderr,
            "Erro: nao foi possivel criar o arquivo '%s': %s.\n",
            OUTPUT_FILE,
            strerror(errno)
        );

        free(addresses);
        return EXIT_FAILURE;
    }

    int success = 1;

    for (size_t index = 0; index < address_count; index++) {
        TranslationResult result;

        if (!vm_translate(
                &memory,
                addresses[index],
                page_algorithm,
                tlb_algorithm,
                &result
            )) {
            success = 0;
            break;
        }

        int write_result = fprintf(
            output,
            "Virtual address: %d TLB: %d Physical address: %d Value: %d\n",
            addresses[index],
            result.tlb_index,
            result.physical_address,
            result.value
        );

        if (write_result < 0) {
            fprintf(stderr, "Erro: falha ao escrever uma traducao no arquivo '%s'.\n", OUTPUT_FILE);
            success = 0;
            break;
        }
    }

    if (success) {
        double page_fault_rate = (double)memory.page_faults / address_count;
        double tlb_hit_rate = (double)memory.tlb_hits / address_count;

        if (fprintf(
                output,
                "Number of Translated Addresses = %zu\n"
                "Page Faults = %zu\n"
                "Page Fault Rate = %.3f\n"
                "TLB Hits = %zu\n"
                "TLB Hit Rate = %.3f\n",
                address_count,
                memory.page_faults,
                page_fault_rate,
                memory.tlb_hits,
                tlb_hit_rate
            ) < 0) {
            fprintf(stderr, "Erro: falha ao escrever as estatisticas no arquivo '%s'.\n", OUTPUT_FILE);
            success = 0;
        }
    }

    if (fclose(output) != 0) {
        fprintf(stderr, "Erro: falha ao fechar o arquivo '%s'.\n", OUTPUT_FILE);
        success = 0;
    }

    free(addresses);

    if (!success) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
