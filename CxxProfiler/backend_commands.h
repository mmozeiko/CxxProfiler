#pragma once

#include <stdint.h>

#define CXX_CMD_HEADER (sizeof(uint8_t) + sizeof(uint32_t))

#define CXX_CMD_SET_OPTIONS    0
#define CXX_CMD_STOP           1
#define CXX_CMD_CREATE_PROCESS 2
#define CXX_CMD_ATTACH_PROCESS 3

#define CXX_REPLY_MESSAGE       0
#define CXX_REPLY_STACK_SAMPLES 1
#define CXX_REPLY_NEW_STRING    2
#define CXX_REPLY_NEW_SYMBOL    3
#define CXX_REPLY_PROCESS_START 4
#define CXX_REPLY_PROCESS_END   5
#define CXX_REPLY_THREAD_ADD    6
#define CXX_REPLY_THREAD_REMOVE 7
#define CXX_REPLY_MODULE_LOAD   8
#define CXX_REPLY_MODULE_UNLOAD 9
#define CXX_REPLY_SYMBOLS       10

#define CXX_SYMBOL_STATUS_DOWNLOADING    0
#define CXX_SYMBOL_STATUS_LOADED_PRIVATE 1
#define CXX_SYMBOL_STATUS_LOADED_PUBLIC  2
#define CXX_SYMBOL_STATUS_LOADED_EXPORT  3

typedef struct backend_reply_symbol
{
    uint32_t id;
    uint32_t name;
    uint32_t file;
    uint32_t size;
    uint64_t address;
    uint32_t module;
    uint32_t line;
    uint32_t line_last;
} backend_reply_symbol;

typedef struct {
    uint32_t symbol;
    uint32_t line;
    uint32_t offset;
} backend_stack_entry;

typedef struct backend_stack {
    uint32_t count;
    backend_stack_entry entries[256];
    struct backend_stack* next;
} backend_stack;
