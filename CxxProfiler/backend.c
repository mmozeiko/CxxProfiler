#include "backend.h"
#include "backend_commands.h"
#include "backend_debugger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static backend_globals globals;

#define CXX_BACKEND_MEMORY_BLOCK_SIZE (1024 * 1024)

typedef struct backend_memory
{
    struct backend_memory* next;
    uint32_t used;
    uint8_t data[0];
} backend_memory;

static backend_memory backend_mem;

static void backend_enable_debug_privileges(void)
{
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
        LUID Uid;
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Uid))
        {
            TOKEN_PRIVILEGES state;
            state.PrivilegeCount = 1;
            state.Privileges[0].Luid = Uid;
            state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(token, FALSE, &state, sizeof(state), NULL, NULL);
        }
        CloseHandle(token);
    }
}

static uint32_t backend_cmd_get32(const uint8_t* buffer, uint32_t offset)
{
    uint32_t result = *(uint32_t*)(buffer + offset);
    return result;
}

static wchar_t* backend_cmd_getW(const uint8_t* buffer, uint32_t offset, uint32_t size)
{
    int wsize = MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer + offset, size, NULL, 0);
    Assert(wsize > 0);

    wchar_t* result = backend_alloc((wsize + 1) * sizeof(wchar_t));
    wsize = MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer + offset, size, result, wsize);
    Assert(wsize > 0);

    result[wsize] = 0;
    return result;
}

static uint32_t backend_cmd_set_options(const uint8_t* buffer, uint32_t size)
{
    uint32_t cmd_size = CXX_CMD_HEADER + 2 * sizeof(uint32_t);
    if (size < cmd_size)
    {
        return 0;
    }

    globals.sampling_usec = backend_cmd_get32(buffer, CXX_CMD_HEADER + 0 * sizeof(uint32_t));
    globals.download_symbols = backend_cmd_get32(buffer, CXX_CMD_HEADER + 1 * sizeof(uint32_t));

    return cmd_size;
}

static uint32_t backend_cmd_stop(const uint8_t* buffer, uint32_t size)
{
    (void)buffer;

    if (size < CXX_CMD_HEADER)
    {
        return 0;
    }

    backend_debugger_stop(&globals);
    return CXX_CMD_HEADER;
}

static uint32_t backend_cmd_create_process(const uint8_t* buffer, uint32_t size)
{
    uint32_t cmd_size = CXX_CMD_HEADER + 3 * sizeof(uint32_t);
    if (size < cmd_size)
    {
        return 0;
    }

    uint32_t command_size = backend_cmd_get32(buffer, CXX_CMD_HEADER + 0 * sizeof(uint32_t));
    uint32_t arguments_size = backend_cmd_get32(buffer, CXX_CMD_HEADER + 1 * sizeof(uint32_t));
    uint32_t folder_size = backend_cmd_get32(buffer, CXX_CMD_HEADER + 2 * sizeof(uint32_t));

    if (size < cmd_size + command_size + arguments_size + folder_size)
    {
        return 0;
    }

    globals.launch_command = backend_cmd_getW(buffer, cmd_size, command_size);
    cmd_size += command_size;
    globals.launch_arguments = backend_cmd_getW(buffer, cmd_size, arguments_size);
    cmd_size += arguments_size;
    globals.launch_folder = backend_cmd_getW(buffer, cmd_size, folder_size);
    cmd_size += folder_size;

    backend_debugger_start(&globals);
    return cmd_size;
}

static uint32_t backend_cmd_attach_process(const uint8_t* buffer, uint32_t size)
{
    uint32_t cmd_size = CXX_CMD_HEADER + sizeof(uint32_t);
    if (size < cmd_size)
    {
        return 0;
    }

    globals.attach_pid = backend_cmd_get32(buffer, CXX_CMD_HEADER);

    backend_debugger_start(&globals);
    return cmd_size;
}

void backend_main(const wchar_t* pipe_name)
{
    backend_enable_debug_privileges();

    HANDLE pipe = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return;
    }
    globals.pipe = pipe;

    globals.send_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    Assert(globals.send_event);

    OVERLAPPED overlapped =
    {
        .hEvent = CreateEventW(NULL, FALSE, FALSE, NULL),
    };
    Assert(overlapped.hEvent);

    HANDLE handles[2] = { overlapped.hEvent };
    DWORD handle_count = 1;

    uint8_t buffer[4096];
    uint32_t buffer_used = 0;

    for (;;)
    {
        DWORD bytes_read = ArrayCount(buffer) - buffer_used;
        if (!ReadFile(pipe, buffer + buffer_used, bytes_read, &bytes_read, &overlapped))
        {
            DWORD error = GetLastError();
            Assert(error == ERROR_IO_PENDING);

            DWORD wait = WaitForMultipleObjects(handle_count, handles, FALSE, INFINITE);
            if (wait == WAIT_OBJECT_0)
            {
                if (!GetOverlappedResult(pipe, &overlapped, &bytes_read, FALSE))
                {
                    // pipe closed
                    // backend_cmd_stop waits for backend thread to exit
                    backend_debugger_stop(&globals);
                    return;
                }
            }
            else if (wait == WAIT_OBJECT_0 + 1)
            {
                // backend thread finished
                return;
            }
        }

        buffer_used += bytes_read;

        while (buffer_used >= CXX_CMD_HEADER)
        {
            uint32_t used = 0;
            switch (buffer[0])
            {
            case CXX_CMD_SET_OPTIONS:
                used = backend_cmd_set_options(buffer, buffer_used);
                break;
            case CXX_CMD_STOP:
                used = backend_cmd_stop(buffer, buffer_used);
                if (used != 0)
                {
                    // backend_cmd_stop waits for backend thread to exit
                    return;
                }
                break;
            case CXX_CMD_CREATE_PROCESS:
                used = backend_cmd_create_process(buffer, buffer_used);
                handles[1] = globals.thread;
                handle_count = 2;
                break;
            case CXX_CMD_ATTACH_PROCESS:
                used = backend_cmd_attach_process(buffer, buffer_used);
                handles[1] = globals.thread;
                handle_count = 2;
                break;
            }
            backend_copy(buffer, buffer + used, buffer_used - used);
            buffer_used -= used;
        }
    }
}

void* backend_os_alloc(size_t size)
{
    void* result = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Assert(result);
    return result;
}

void backend_os_free(void* ptr)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void* backend_alloc(uint32_t size)
{
    size = AlignPow2(size, 16);
    uint32_t max_size = CXX_BACKEND_MEMORY_BLOCK_SIZE - AlignPow2(sizeof(backend_mem), 8);
    Assert(size <= max_size);

    backend_memory* mem = &backend_mem;
    for (;;)
    {
        if (mem->next == NULL)
        {
            backend_memory* next = backend_os_alloc(CXX_BACKEND_MEMORY_BLOCK_SIZE);
            Assert(next);
            mem->next = next;
            mem->used = 0;
        }
        if (size <= max_size - mem->used)
        {
            break;
        }
        mem = mem->next;
    }

    void* result = mem->next->data + mem->next->used;
    mem->next->used += size;
    return result;
}

uint32_t backend_utf8_length(const wchar_t* wstring, int wlength)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, wstring, wlength, NULL, 0, NULL, NULL);
    Assert(length > 0);
    return length;
}

uint32_t backend_utf8_convert(char* string, uint32_t length, const wchar_t* wstring, int wlength)
{
    int result = WideCharToMultiByte(CP_UTF8, 0, wstring, wlength, string, (int)length, NULL, NULL);
    Assert(result > 0);
    return result - 1;
}

void backend_zero(void* dst, size_t size)
{
    RtlZeroMemory(dst, size);
}

void backend_copy(void* dst, const void* src, size_t size)
{
    RtlMoveMemory(dst, src, size);
}

int backend_equal(const void* a, const void* b, size_t size)
{
    return RtlEqualMemory(a, b, size);
}

static void backend_reply_send(const void* data, uint32_t size)
{
    OVERLAPPED overlapped =
    {
        .hEvent = globals.send_event,
    };

    DWORD written;
    BOOL check = WriteFile(globals.pipe, data, size, &written, &overlapped);
    if (!check)
    {
        DWORD err = GetLastError();
        Assert(err == ERROR_IO_PENDING);
        check = GetOverlappedResult(globals.pipe, &overlapped, &written, TRUE);
    }
    Assert(check && written == size);
}

static void backend_reply_header(uint8_t command, uint32_t size)
{
    uint8_t data[sizeof(command) + sizeof(size)];
    *(uint8_t*)(data + 0) = command;
    *(uint32_t*)(data + 1) = size;
    backend_reply_send(data, sizeof(data));
}

void backend_reply_error(const char* message)
{
    char* err_msg_utf8 = NULL;
    int err_msg_length = 0;
    LPWSTR err_msg;
    DWORD error = GetLastError();
    if (error && FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPWSTR)&err_msg, 0, NULL))
    {
        uint32_t err_msg_length = backend_utf8_length(err_msg, -1);
        char* err_msg_utf8 = _alloca(err_msg_length);
        err_msg_length = backend_utf8_convert(err_msg_utf8, err_msg_length, err_msg, -1);
        LocalFree(err_msg);
    }

    uint32_t msg_length = (uint32_t)strlen(message);
    backend_reply_header(CXX_REPLY_MESSAGE, 2 * sizeof(uint32_t) + msg_length + err_msg_length);
    backend_reply_send(&msg_length, sizeof(msg_length));
    backend_reply_send(&err_msg_length, sizeof(err_msg_length));
    backend_reply_send(message, msg_length);
    if (err_msg_length > 0)
    {
        backend_reply_send(err_msg_utf8, err_msg_length);
    }
}

void backend_reply_new_string(const char* string, uint32_t length)
{
    backend_reply_header(CXX_REPLY_NEW_STRING, sizeof(length) + length);
    backend_reply_send(&length, sizeof(length));
    backend_reply_send(string, length);
}

void backend_reply_new_symbol(const backend_reply_symbol* symbol)
{
    uint32_t size = sizeof(*symbol);
    backend_reply_header(CXX_REPLY_NEW_SYMBOL, size);
    backend_reply_send(symbol, size);
}

void backend_reply_process_start(uint32_t process_id, uint32_t ptr_size)
{
    uint32_t cmd_size = sizeof(process_id) + sizeof(ptr_size);
    backend_reply_header(CXX_REPLY_PROCESS_START, cmd_size);
    backend_reply_send(&process_id, sizeof(process_id));
    backend_reply_send(&ptr_size, sizeof(ptr_size));
}

void backend_reply_process_end(uint32_t exit_code)
{
    backend_reply_header(CXX_REPLY_PROCESS_END, sizeof(exit_code));
    backend_reply_send(&exit_code, sizeof(exit_code));
}

void backend_reply_thread_start(uint32_t thread_id, uint64_t entry)
{
    backend_reply_header(CXX_REPLY_THREAD_ADD, sizeof(thread_id) + sizeof(entry));
    backend_reply_send(&thread_id, sizeof(thread_id));
    backend_reply_send(&entry, sizeof(entry));
}

void backend_reply_thread_end(uint32_t thread_id)
{
    backend_reply_header(CXX_REPLY_THREAD_REMOVE, sizeof(thread_id));
    backend_reply_send(&thread_id, sizeof(thread_id));
}

void backend_reply_module_load(uint64_t base, const wchar_t* name)
{
    uint32_t length = 0;
    char* name_utf8 = NULL;
    if (name)
    {
        length = backend_utf8_length(name, -1);
        name_utf8 = _alloca(length);
        length = backend_utf8_convert(name_utf8, length, name, -1);
    }

    uint32_t cmd_size = sizeof(base) + sizeof(length) + length;
    backend_reply_header(CXX_REPLY_MODULE_LOAD, cmd_size);
    backend_reply_send(&base, sizeof(base));
    backend_reply_send(&length, sizeof(length));
    backend_reply_send(name_utf8, length);
}

void backend_reply_module_unload(uint64_t base)
{
    backend_reply_header(CXX_REPLY_MODULE_UNLOAD, sizeof(base));
    backend_reply_send(&base, sizeof(base));
}

static uint32_t backend_stack_count(const backend_stack* stack)
{
    uint32_t count = 0;
    while (stack)
    {
        count += stack->count;
        stack = stack->next;
    }
    return count;
}

void backend_reply_stack_samples(uint32_t thread_id, const backend_stack* stack)
{
    uint32_t count = backend_stack_count(stack);
    if (count)
    {
        uint32_t cmd_size = sizeof(thread_id) + sizeof(count) + count * sizeof(backend_stack_entry);
        backend_reply_header(CXX_REPLY_STACK_SAMPLES, cmd_size);
        backend_reply_send(&thread_id, sizeof(thread_id));
        backend_reply_send(&count, sizeof(count));
        while (stack)
        {
            backend_reply_send(stack->entries, stack->count * sizeof(backend_stack_entry));
            stack = stack->next;
        }
    }
}

void backend_reply_symbols(uint32_t status)
{
    backend_reply_header(CXX_REPLY_SYMBOLS, sizeof(status));
    backend_reply_send(&status, sizeof(status));
}
