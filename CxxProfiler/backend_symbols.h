#pragma once

#include <stdint.h>

typedef void* HANDLE;

typedef struct backend_symbol {
    uint64_t address;
    uint32_t size;
    uint32_t id;

    // https://en.wikipedia.org/wiki/AVL_tree
    uint32_t height;
    struct backend_symbol* left;
    struct backend_symbol* right;
} backend_symbol;

typedef struct backend_symbol_memory backend_symbol_memory;

typedef struct backend_module {
    HANDLE handle;
    uint64_t base;
    uint32_t size;
    uint32_t name;
    backend_symbol* symbols;
    backend_symbol_memory* memory;
    struct backend_module* next;
} backend_module;

typedef struct backend_symbols {
    uint32_t symbol_id;
    backend_module list;
    backend_module free;
} backend_symbols;

typedef struct backend_strings backend_strings;

int backend_symbols_init(HANDLE process, backend_symbols* symbols, int download_symbols, int is_wow64);
void backend_symbols_done(HANDLE process);

void backend_symbols_load(HANDLE process, backend_symbols* symbols, backend_strings* strings, HANDLE handle, const wchar_t* name, uint64_t base);
void backend_symbols_unload(HANDLE process, backend_symbols* symbols, uint64_t base);
backend_symbol* backend_symbols_get(HANDLE process, backend_symbols* symbols, backend_strings* strings, uint64_t address);

uint32_t backend_symbols_lookup_line(HANDLE process, uint64_t address);
