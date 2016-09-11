#pragma once

#include <stdint.h>
#include <wchar.h>

#if _DEBUG

#define Assert(cond) do \
    __pragma(warning(push))          \
    __pragma(warning(disable:4127))  \
    {                                \
        if (!(cond)) __debugbreak(); \
    } while (0)                      \
    __pragma(warning(pop))

#else

#define Assert(cond) __assume(cond)

#endif

#define ArrayCount(arr) (sizeof(arr)/sizeof(*arr))
#define AlignPow2(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#define CXX_FREELIST_GET(var, freelist)    \
    if (freelist.next)                     \
    {                                      \
        var = freelist.next;               \
        freelist.next = var->next;         \
    }                                      \
    else                                   \
    {                                      \
        var = backend_alloc(sizeof(*var)); \
    }

#define CXX_LIST_ADD(var, list) \
    var->next = list.next;      \
    list.next = var;

typedef void* HANDLE;
typedef struct backend_reply_symbol backend_reply_symbol;
typedef struct backend_stack backend_stack;

typedef struct backend_globals {
    HANDLE pipe;
    HANDLE send_event;

    uint32_t sampling_usec;
    int download_symbols;

    uint32_t attach_pid;
    wchar_t* launch_command;
    wchar_t* launch_arguments;
    wchar_t* launch_folder;

    int running;
    HANDLE process;
    HANDLE thread;
} backend_globals;

void backend_main(const wchar_t* pipe_name);

void backend_reply_error(const char* message);
void backend_reply_new_string(const char* string, uint32_t length);
void backend_reply_new_symbol(const backend_reply_symbol* symbol);
void backend_reply_process_start(uint32_t process_id, uint32_t ptr_size);
void backend_reply_process_end(uint32_t exit_code);
void backend_reply_thread_start(uint32_t thread_id, uint64_t entry);
void backend_reply_thread_end(uint32_t thread_id);
void backend_reply_module_load(uint64_t base, const wchar_t* name);
void backend_reply_module_unload(uint64_t base);
void backend_reply_stack_samples(uint32_t thread_id, const backend_stack* stack);
void backend_reply_symbols(uint32_t status);

void* backend_os_alloc(size_t size);
void backend_os_free(void* ptr);

void* backend_alloc(uint32_t size);

uint32_t backend_utf8_length(const wchar_t* wstring, int wlength);
uint32_t backend_utf8_convert(char* string, uint32_t length, const wchar_t* wstring, int wlength);

void backend_zero(void* dst, size_t size);
void backend_copy(void* dst, const void* src, size_t size);
int backend_equal(const void* a, const void* b, size_t size);
