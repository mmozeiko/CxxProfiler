#pragma once

#include <stdint.h>

typedef void* HANDLE;

typedef struct backend_thread {
    uint32_t id;
    HANDLE handle;
    struct backend_thread* next;
} backend_thread;

typedef struct backend_threads {
    backend_thread list;
    backend_thread free;
} backend_threads;

void backend_threads_init(backend_threads* threads);
void backend_threads_add(backend_threads* threads, uint32_t id, uint64_t entry, HANDLE handle);
void backend_threads_remove(backend_threads* threads, uint32_t id);
